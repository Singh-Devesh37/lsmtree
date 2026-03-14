#pragma once
#include <string>
#include <fstream>
#include "memtable.h"

class WALWriter {
public:
    explicit WALWriter(const std::string& filename);
    void logPut(const std::string& key, const std::string& value);
    void logDelete(const std::string& key);
    void clear();
    static void replay(const std::string& filename, MemTable& memTable);
private:
    std::ofstream walFile;
    std::string filename;
};