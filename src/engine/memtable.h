#pragma once
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

class MemTable {

    private:
        std::map<std::string, std::string> table;

    public:
        static constexpr const char* TOMBSTONE = "__TOMBSTONE__";

        void put(const std::string& key, const std::string& value);

        std::optional<std::string> get(const std::string& key);

        void remove(const std::string& key);

        std::vector<std::pair<std::string, std::string>> scan(const std::string& startKey, const std::string& endKey);

        const std::map<std::string, std::string>& data() const{
            return table;
        };

        const int& size() const{
            return table.size();
        };

        void clear(){
            table.clear();
        }

};