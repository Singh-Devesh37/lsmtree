#pragma once
#include <bitset>
#include <string>
#include <functional>

class BloomFilter {
    public:
        explicit BloomFilter(const size_t size = 1024) : filter(size), SIZE(size) {}

        void add(const std::string& key){
            filter[hash1(key)%SIZE] = true;
            filter[hash2(key)%SIZE] = true;
        }

        bool mightContain(const std::string& key){
            return filter[hash1(key)%SIZE] && filter[hash2(key)%SIZE];
        }

        std::string serialize() const {
            std::string result;
            result.reserve(SIZE);
            for(const bool bit : filter){
                result += bit ? '1' : '0';
            }
            return result;
        }

        void deserialize(const std::string& data) {
            filter.clear();
            SIZE = data.size();
            filter.resize(SIZE);
            for(size_t i = 0; i < SIZE; i++){
                filter[i] = data[i] == '1';
            }
        }
    

    private:
        std::vector<bool> filter;
        size_t SIZE;

        static size_t hash1(const std::string& key){
            return std::hash<std::string>()(key);
        }
        
        // DJB2 Hashing
        static size_t hash2(const std::string& key){
            size_t hash = 5381;
            for(const char c : key){
                hash = ((hash << 5) + hash) + c;
            }
            return hash;
        }

};