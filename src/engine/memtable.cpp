#include "memtable.h"

void MemTable::put(const std::string& key, const std::string& value) {
    table[key] = value;
}

std::optional<std::string> MemTable::get(const std::string& key) {
    const auto it = table.find(key);
    if(it==table.end()){
        return std::nullopt;
    }else if(it->second == TOMBSTONE){
        return "";
    }
    return it->second;
}

void MemTable::remove(const std::string& key) {
    table[key] = TOMBSTONE;
}

std::vector<std::pair<std::string, std::string>> MemTable::scan(const std::string& startKey, const std::string& endKey) {
    std::vector<std::pair<std::string, std::string>> results;

    auto it = table.lower_bound(startKey);
    auto end = table.upper_bound(endKey);

    for (; it != end; ++it) {
        // Skip tombstones in scan results
        if (it->second != TOMBSTONE) {
            results.emplace_back(it->first, it->second);
        }
    }

    return results;
}