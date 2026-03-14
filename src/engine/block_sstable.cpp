#include "block_sstable.h"
#include "block.h"
#include "memtable.h"
#include "../utils/bloom_filter.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <algorithm>

int BlockSSTable::getNextSSTableID(const std::string& directory) {
    int maxID = 0;
    if (!std::filesystem::exists(directory)) {
        return 1;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".blk") {  // .blk for block-based
            std::string fileName = entry.path().filename().string();
            if (const size_t underscore = fileName.find('_'), dot = fileName.find('.');
                underscore != std::string::npos && dot != std::string::npos) {
                int id = std::stoi(fileName.substr(underscore + 1, dot - underscore - 1));
                maxID = std::max(maxID, id);
            }
        }
    }
    return maxID + 1;
}

std::string BlockSSTable::writeToFile(
    const std::map<std::string, std::string>& data,
    const std::string& directory,
    const int& level
) {
    std::string levelPath = directory + "/" + std::to_string(level);
    std::filesystem::create_directories(levelPath);

    int sstableId = getNextSSTableID(levelPath);
    std::string filename = levelPath + "/sstable_" + std::to_string(sstableId) + ".blk";

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    std::vector<BlockIndex> index;
    Block currentBlock;
    BloomFilter bloom;

    // Write data blocks
    for (const auto& [key, value] : data) {
        if (currentBlock.isFull()) {
            // Write current block
            uint64_t blockOffset = file.tellp();
            std::vector<uint8_t> blockData = currentBlock.serialize();

            BlockIndex indexEntry;
            indexEntry.firstKey = currentBlock.getFirstKey();
            indexEntry.lastKey = currentBlock.getLastKey();
            indexEntry.offset = blockOffset;
            indexEntry.size = static_cast<uint32_t>(blockData.size());

            file.write(reinterpret_cast<const char*>(blockData.data()), blockData.size());
            index.push_back(indexEntry);

            currentBlock.clear();
        }

        currentBlock.addEntry(key, value);
        bloom.add(key);
    }

    // Write last block if not empty
    if (!currentBlock.isEmpty()) {
        uint64_t blockOffset = file.tellp();
        std::vector<uint8_t> blockData = currentBlock.serialize();

        BlockIndex indexEntry;
        indexEntry.firstKey = currentBlock.getFirstKey();
        indexEntry.lastKey = currentBlock.getLastKey();
        indexEntry.offset = blockOffset;
        indexEntry.size = static_cast<uint32_t>(blockData.size());

        file.write(reinterpret_cast<const char*>(blockData.data()), blockData.size());
        index.push_back(indexEntry);
    }

    // Write index
    uint64_t indexOffset = file.tellp();

    for (const auto& entry : index) {
        // Write firstKey
        uint32_t firstKeyLen = static_cast<uint32_t>(entry.firstKey.size());
        file.write(reinterpret_cast<const char*>(&firstKeyLen), sizeof(uint32_t));
        file.write(entry.firstKey.data(), entry.firstKey.size());

        // Write lastKey
        uint32_t lastKeyLen = static_cast<uint32_t>(entry.lastKey.size());
        file.write(reinterpret_cast<const char*>(&lastKeyLen), sizeof(uint32_t));
        file.write(entry.lastKey.data(), entry.lastKey.size());

        // Write offset and size
        file.write(reinterpret_cast<const char*>(&entry.offset), sizeof(uint64_t));
        file.write(reinterpret_cast<const char*>(&entry.size), sizeof(uint32_t));
    }

    uint64_t indexEnd = file.tellp();
    uint32_t indexSize = static_cast<uint32_t>(indexEnd - indexOffset);

    // Write footer
    Footer footer;
    footer.indexOffset = indexOffset;
    footer.indexSize = indexSize;
    footer.numBlocks = static_cast<uint32_t>(index.size());
    footer.magicNumber = Footer::MAGIC;

    file.write(reinterpret_cast<const char*>(&footer.indexOffset), sizeof(uint64_t));
    file.write(reinterpret_cast<const char*>(&footer.indexSize), sizeof(uint32_t));
    file.write(reinterpret_cast<const char*>(&footer.numBlocks), sizeof(uint32_t));
    file.write(reinterpret_cast<const char*>(&footer.magicNumber), sizeof(uint32_t));

    file.close();

    // Write bloom filter
    std::string bloomFile = filename + ".bloom";
    std::ofstream bloomOut(bloomFile);
    if (bloomOut.is_open()) {
        bloomOut << bloom.serialize();
        bloomOut.close();
    }

    return filename;
}

BlockSSTable::Footer BlockSSTable::readFooter(const std::string& filename) {
    Footer footer = {0, 0, 0, 0};

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return footer;
    }

    // Seek to footer (last 20 bytes)
    file.seekg(-static_cast<int>(Footer::SIZE), std::ios::end);

    file.read(reinterpret_cast<char*>(&footer.indexOffset), sizeof(uint64_t));
    file.read(reinterpret_cast<char*>(&footer.indexSize), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&footer.numBlocks), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&footer.magicNumber), sizeof(uint32_t));

    file.close();

    if (footer.magicNumber != Footer::MAGIC) {
        std::cerr << "Warning: Invalid magic number in footer: " << filename << std::endl;
        footer = {0, 0, 0, 0};
    }

    return footer;
}

std::vector<BlockSSTable::BlockIndex> BlockSSTable::readIndex(
    const std::string& filename,
    const Footer& footer
) {
    std::vector<BlockIndex> index;

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return index;
    }

    file.seekg(footer.indexOffset);

    for (uint32_t i = 0; i < footer.numBlocks; ++i) {
        BlockIndex entry;

        // Read firstKey
        uint32_t firstKeyLen;
        file.read(reinterpret_cast<char*>(&firstKeyLen), sizeof(uint32_t));
        entry.firstKey.resize(firstKeyLen);
        file.read(&entry.firstKey[0], firstKeyLen);

        // Read lastKey
        uint32_t lastKeyLen;
        file.read(reinterpret_cast<char*>(&lastKeyLen), sizeof(uint32_t));
        entry.lastKey.resize(lastKeyLen);
        file.read(&entry.lastKey[0], lastKeyLen);

        // Read offset and size
        file.read(reinterpret_cast<char*>(&entry.offset), sizeof(uint64_t));
        file.read(reinterpret_cast<char*>(&entry.size), sizeof(uint32_t));

        index.push_back(entry);
    }

    file.close();
    return index;
}

int BlockSSTable::findBlockForKey(
    const std::vector<BlockIndex>& index,
    const std::string& key
) {
    // Binary search to find the block that might contain the key
    int left = 0;
    int right = static_cast<int>(index.size()) - 1;
    int result = -1;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (key >= index[mid].firstKey && key <= index[mid].lastKey) {
            return mid;  // Key is definitely in this block's range
        }

        if (key < index[mid].firstKey) {
            right = mid - 1;
        } else {
            result = mid;  // This might be the block if key > lastKey
            left = mid + 1;
        }
    }

    return result;
}

std::string BlockSSTable::readFromFile(
    const std::string& filename,
    const std::string& key
) {
    Footer footer = readFooter(filename);
    if (footer.magicNumber != Footer::MAGIC) {
        return "";
    }

    std::vector<BlockIndex> index = readIndex(filename, footer);
    int blockIdx = findBlockForKey(index, key);

    if (blockIdx < 0 || blockIdx >= static_cast<int>(index.size())) {
        return "";
    }

    // Read the specific block
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    const BlockIndex& blockIndex = index[blockIdx];
    file.seekg(blockIndex.offset);

    std::vector<uint8_t> blockData(blockIndex.size);
    file.read(reinterpret_cast<char*>(blockData.data()), blockIndex.size);
    file.close();

    Block block = Block::deserialize(blockData);
    auto result = block.find(key);

    if (result.has_value() && result.value() != MemTable::TOMBSTONE) {
        return result.value();
    }

    return "";
}

std::string BlockSSTable::findInBlockSSTable(
    const std::string& directory,
    const std::string& key
) {
    std::vector<std::filesystem::directory_entry> files;

    if (!std::filesystem::exists(directory)) {
        return "";
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".blk") {
            files.push_back(entry);
        }
    }

    // Sort by modification time (newest first)
    std::ranges::sort(files, [](const auto& a, const auto& b) {
        return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
    });

    for (const auto& file : files) {
        // Check bloom filter first
        std::string bloomPath = file.path().string() + ".bloom";
        BloomFilter bloom;

        if (std::ifstream bloomIn(bloomPath); bloomIn.is_open()) {
            std::string bits;
            std::getline(bloomIn, bits);
            bloom.deserialize(bits);

            if (!bloom.mightContain(key)) {
                continue;  // Skip this file
            }
        }

        std::string value = readFromFile(file.path().string(), key);
        if (!value.empty()) {
            return value;
        }
    }

    return "";
}

std::vector<std::pair<std::string, std::string>> BlockSSTable::scanSSTable(
    const std::string& directory,
    const std::string& startKey,
    const std::string& endKey
) {
    std::map<std::string, std::string> results;

    if (!std::filesystem::exists(directory)) {
        return {};
    }

    std::vector<std::filesystem::directory_entry> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".blk") {
            files.push_back(entry);
        }
    }

    // Sort by modification time (newest first for correct value precedence)
    std::ranges::sort(files, [](const auto& a, const auto& b) {
        return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
    });

    for (const auto& file : files) {
        Footer footer = readFooter(file.path().string());
        if (footer.magicNumber != Footer::MAGIC) continue;

        std::vector<BlockIndex> index = readIndex(file.path().string(), footer);

        std::ifstream infile(file.path(), std::ios::binary);
        if (!infile.is_open()) continue;

        for (const auto& blockIdx : index) {
            // Skip blocks that don't overlap with our range
            if (blockIdx.lastKey < startKey || blockIdx.firstKey > endKey) {
                continue;
            }

            // Read block
            infile.seekg(blockIdx.offset);
            std::vector<uint8_t> blockData(blockIdx.size);
            infile.read(reinterpret_cast<char*>(blockData.data()), blockIdx.size);

            Block block = Block::deserialize(blockData);

            for (const auto& [key, value] : block.getEntries()) {
                if (key >= startKey && key <= endKey) {
                    // Only add if not already present (newer values take precedence)
                    if (results.find(key) == results.end() && value != MemTable::TOMBSTONE) {
                        results[key] = value;
                    }
                }
            }
        }

        infile.close();
    }

    // Convert to vector
    std::vector<std::pair<std::string, std::string>> resultVec;
    for (const auto& [key, value] : results) {
        resultVec.emplace_back(key, value);
    }

    return resultVec;
}

void BlockSSTable::compactSSTables(
    const std::string& directory,
    const int& level
) {
    std::vector<std::filesystem::directory_entry> files;
    std::map<std::string, std::string> latestData;

    std::string levelDir = directory + "/" + std::to_string(level);
    if (!std::filesystem::exists(levelDir)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(levelDir)) {
        if (entry.path().extension() == ".blk") {
            files.push_back(entry);
        }
    }

    // Sort by modification time (newest first to keep latest values)
    std::ranges::sort(files, [](const auto& a, const auto& b) {
        return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
    });

    // Read all blocks from all files
    for (const auto& file : files) {
        Footer footer = readFooter(file.path().string());
        if (footer.magicNumber != Footer::MAGIC) continue;

        std::vector<BlockIndex> index = readIndex(file.path().string(), footer);

        std::ifstream infile(file.path(), std::ios::binary);
        if (!infile.is_open()) continue;

        for (const auto& blockIdx : index) {
            infile.seekg(blockIdx.offset);
            std::vector<uint8_t> blockData(blockIdx.size);
            infile.read(reinterpret_cast<char*>(blockData.data()), blockIdx.size);

            Block block = Block::deserialize(blockData);

            for (const auto& [key, value] : block.getEntries()) {
                // Only add if not already present (first occurrence from newest file wins)
                if (latestData.find(key) == latestData.end()) {
                    if (value != MemTable::TOMBSTONE) {
                        latestData[key] = value;
                    }
                }
            }
        }

        infile.close();
    }

    // Delete old files
    for (const auto& file : files) {
        std::filesystem::remove(file.path());
        std::filesystem::remove(file.path().string() + ".bloom");
    }

    // Write compacted data to next level
    if (!latestData.empty()) {
        writeToFile(latestData, directory, level + 1);
    }
}
