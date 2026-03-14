#include "sstable.h"
#include "memtable.h"
#include "../utils/bloom_filter.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>


std::string SSTable::writeToFile(const std::map<std::string, std::string>& data, const std::string& directory, const int& level){
    std::string path = directory + "/" + std::to_string(level);
    std::filesystem::create_directories(path);
    int sstable_id = getNextSSTableID(path);
    std::ostringstream filename;
    filename << path << "/sstable_" << sstable_id << ".dat";
    std::ofstream file(filename.str());

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open SSTable file: " + filename.str());
    }

    BloomFilter bloom;

    for(const auto& [key,value] : data){
        file << key << "\t" << value << "\n";
        bloom.add(key);
    }

    std::string bloomFile = filename.str() + ".bloom";
    std::ofstream bloomOut(bloomFile);
    if (!bloomOut.is_open()) {
        throw std::runtime_error("Failed to open bloom filter file: " + bloomFile);
    }
    bloomOut << bloom.serialize();

    return filename.str();
}

int SSTable::getNextSSTableID(const std::string& directory) {
    int maxID = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".dat") {
            std::string fileName = entry.path().filename().string();
            if (const size_t underscore = fileName.find('_'), dot = fileName.find('.'); underscore != std::string::npos && dot != std::string::npos) {
                int id = std::stoi(fileName.substr(underscore + 1, dot - underscore - 1));
                maxID = std::max(maxID, id);
            }
        }
    }
    return maxID + 1;
}


std::string SSTable::readFromFile(const std::string& filename, const std::string& key){
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open SSTable file: " << filename << std::endl;
        return "";
    }

    std::string line;
    while(std::getline(file,line)){
        std::istringstream iss(line);
        std::string curr_key,curr_value;
        if(std::getline(iss,curr_key,'\t') && std::getline(iss,curr_value)){
            if(curr_key == key && curr_value!=MemTable::TOMBSTONE){
                return curr_value;
            }
        }
    }
    return "";
}

std::string SSTable::findInSSTable(const std::string& directory, const std::string& key){
    std::vector<std::filesystem::directory_entry> files;

    if (!std::filesystem::exists(directory)) {
        return "";
    }

    for(const auto& entry : std::filesystem::directory_iterator(directory)){
        if(entry.path().extension() == ".dat"){
            files.push_back(entry);
        }
    }

    std::ranges::sort(files, [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b){
        return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
    });

    for(const auto& file : files){
        std::string bloomPath = file.path().string() + ".bloom";
        BloomFilter bloom;

        if(std::ifstream bloomIn(bloomPath); bloomIn.is_open()){
            std::string bits;
            std::getline(bloomIn,bits);
            bloom.deserialize(bits);
        }else{
            std::cerr << "Could not open bloom file " << bloomPath << std::endl;
        }

        if(bloom.mightContain(key)){
            if(const std::string value = readFromFile(file.path().string(), key); !value.empty()){
                return value;
            }
        }
    }

    return "";
}

std::vector<std::pair<std::string, std::string>> SSTable::scanSSTable(const std::string& directory, const std::string& startKey, const std::string& endKey) {
    std::map<std::string, std::string> results;  // Use map to handle duplicates (newest wins)

    if (!std::filesystem::exists(directory)) {
        return {};
    }

    std::vector<std::filesystem::directory_entry> files;

    for(const auto& entry : std::filesystem::directory_iterator(directory)){
        if(entry.path().extension() == ".dat"){
            files.push_back(entry);
        }
    }

    // Sort by modification time (newest first)
    std::ranges::sort(files, [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b){
        return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
    });

    // Scan through all SSTables
    for(const auto& file : files){
        std::ifstream infile(file.path());
        if (!infile.is_open()) continue;

        std::string line;
        while(std::getline(infile, line)){
            std::istringstream iss(line);
            std::string key, value;
            if(std::getline(iss, key, '\t') && std::getline(iss, value)){
                // Check if key is in range
                if(key >= startKey && key <= endKey){
                    // Only add if not already present (newer values take precedence)
                    if(results.find(key) == results.end() && value != MemTable::TOMBSTONE){
                        results[key] = value;
                    }
                }
            }
        }
    }

    // Convert map to vector
    std::vector<std::pair<std::string, std::string>> resultVec;
    for(const auto& [key, value] : results){
        resultVec.emplace_back(key, value);
    }

    return resultVec;
}

void SSTable::compactSSTables(const std::string& directory, const int& level){
    std::vector<std::filesystem::directory_entry> files;
    std::map<std::string, std::string> latestData;

    std::string levelDir = directory + "/" + std::to_string(level);
    if (!std::filesystem::exists(levelDir)) {
        std::cerr << "Warning: Level directory does not exist: " << levelDir << std::endl;
        return;
    }

    for(const auto& entry : std::filesystem::directory_iterator(levelDir)){
        if(entry.path().extension() == ".dat"){
            files.push_back(entry);
        }
    }

    // Sort by modification time (NEWEST first) to keep latest values
    std::sort(files.begin(),files.end(),[](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b){
        return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
    });

    for(const auto& file : files){
        std::ifstream infile(file.path());
        std::string line;

        while(std::getline(infile,line)){
            std::string key, value;
            if(std::istringstream iss(line); std::getline(iss,key,'\t') && std::getline(iss,value)){
                // Only insert if key doesn't exist (first occurrence from newest file wins)
                if(latestData.find(key)==latestData.end()){
                    if(value!=MemTable::TOMBSTONE){
                        latestData[key] = value;
                    }
                }
            }
        }
    }
    
    for(const auto& file:files){
        std::filesystem::remove(file.path());
        std::filesystem::remove(file.path().string() + ".bloom");
    }

    writeToFile(latestData, directory, level+1);
}