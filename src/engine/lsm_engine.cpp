#include "lsm_engine.h"
#include "sstable.h"
#include "block_sstable.h"
#include <filesystem>
#include <algorithm>

LSMEngine::LSMEngine(const std::string& baseDir, int flushThreshold, int maxLevels, bool useBlockBasedFormat)
    : wal(baseDir + "/wal.log"),
      baseDir(baseDir),
      flushThreshold(flushThreshold),
      maxLevels(maxLevels),
      useBlockBasedFormat(useBlockBasedFormat) {
    WALWriter::replay(baseDir + "/wal.log", memTable);
}

std::string LSMEngine::getSSTableBaseDir() const {
    return baseDir + "/ssTables";
}

void LSMEngine::put(const std::string& key, const std::string& value) {
    memTable.put(key, value);
    wal.logPut(key, value);
    if (memTable.size() >= flushThreshold) {
        flush();
    }
}

std::string LSMEngine::get(const std::string& key) {
    if (auto optValue = memTable.get(key); optValue.has_value()) {
        return optValue.value();  // Can be "" (tombstone)
    }

    if (useBlockBasedFormat) {
        return BlockSSTable::findInBlockSSTable(getSSTableBaseDir(), key);
    } else {
        return SSTable::findInSSTable(getSSTableBaseDir(), key);
    }
}

std::vector<std::pair<std::string, std::string>> LSMEngine::scan(const std::string& startKey, const std::string& endKey) {
    // First, get results from MemTable
    auto results = memTable.scan(startKey, endKey);

    // Convert to map for merging (newest data from MemTable takes precedence)
    std::map<std::string, std::string> mergedResults;
    for (const auto& [key, value] : results) {
        mergedResults[key] = value;
    }

    // Then scan SSTables
    std::vector<std::pair<std::string, std::string>> sstableResults;
    if (useBlockBasedFormat) {
        sstableResults = BlockSSTable::scanSSTable(getSSTableBaseDir(), startKey, endKey);
    } else {
        sstableResults = SSTable::scanSSTable(getSSTableBaseDir(), startKey, endKey);
    }

    // Merge SSTable results (only add if not already in MemTable)
    for (const auto& [key, value] : sstableResults) {
        if (mergedResults.find(key) == mergedResults.end()) {
            mergedResults[key] = value;
        }
    }

    // Convert back to vector (already sorted due to map)
    std::vector<std::pair<std::string, std::string>> finalResults;
    for (const auto& [key, value] : mergedResults) {
        finalResults.emplace_back(key, value);
    }

    return finalResults;
}

void LSMEngine::remove(const std::string& key) {
    memTable.remove(key);
    wal.logDelete(key);

    if (memTable.size() >= flushThreshold) {
        flush();
    }
}

void LSMEngine::flush() {
    if (useBlockBasedFormat) {
        BlockSSTable::writeToFile(memTable.data(), getSSTableBaseDir(), 0);
    } else {
        SSTable::writeToFile(memTable.data(), getSSTableBaseDir(), 0);
    }
    memTable.clear();
    wal.clear();  // Clear WAL after successful flush
}

void LSMEngine::manualCompact() {
    runCompactionCascade();
}

void LSMEngine::autoCompact() {
    runCompactionCascade();
}

void LSMEngine::runCompactionCascade() {
    for (int i = 0; i < maxLevels - 1; ++i) {
        std::string levelDir = getSSTableBaseDir() + "/" + std::to_string(i);
        if (!std::filesystem::exists(levelDir)) continue;

        std::string fileExtension = useBlockBasedFormat ? ".blk" : ".dat";
        const int fileCount = std::count_if(
            std::filesystem::directory_iterator(levelDir),
            std::filesystem::directory_iterator{},
            [&fileExtension](const auto& entry) {
                return entry.path().extension() == fileExtension;
            }
        );

        if (fileCount >= flushThreshold) {
            if (useBlockBasedFormat) {
                BlockSSTable::compactSSTables(getSSTableBaseDir(), i);
            } else {
                SSTable::compactSSTables(getSSTableBaseDir(), i);
            }
        }
    }
}
