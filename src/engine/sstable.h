#pragma once
#include <string>
#include <map>
#include <vector>

class SSTable {
    public:
        static std::string writeToFile(const std::map<std::string, std::string>& data, const std::string& directory, const int& level);
        static std::string readFromFile(const std::string& filename, const std::string& key);
        static std::string findInSSTable(const std::string& directory, const std::string& key);
        static std::vector<std::pair<std::string, std::string>> scanSSTable(const std::string& directory, const std::string& startKey, const std::string& endKey);
        static void compactSSTables(const std::string& directory, const int& level);
    private:
        static int getNextSSTableID(const std::string& directory);
};