#pragma once

#include "memtable.h"
#include "wal_writer.h"
#include <vector>

class LSMEngine {
public:
    LSMEngine(const std::string& baseDir, int flushThreshold, int maxLevels, bool useBlockBasedFormat = true);

    void put(const std::string& key, const std::string& value);
    std::string get(const std::string& key);
    void remove(const std::string& key);
    std::vector<std::pair<std::string, std::string>> scan(const std::string& startKey, const std::string& endKey);
    void flush();
    void manualCompact();
    void autoCompact();

private:
    MemTable memTable;
    WALWriter wal;
    std::string baseDir;
    int flushThreshold;
    int maxLevels;
    bool useBlockBasedFormat;

    void runCompactionCascade();
    std::string getSSTableBaseDir() const;
};
