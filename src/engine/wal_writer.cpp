#include "wal_writer.h"
#include "memtable.h"
#include <fstream>
#include <sstream>
#include <iostream>

WALWriter::WALWriter(const std::string& filename) : filename(filename) {
    walFile.open(filename, std::ios::app);
    walFile << std::unitbuf;
}

void WALWriter::logPut(const std::string& key, const std::string& value) {
    walFile << "PUT " << key << " " << value << "\n";
}

void WALWriter::logDelete(const std::string& key) {
    walFile << "DEL " << key << "\n";
}

void WALWriter::clear() {
    walFile.close();
    // Truncate the WAL file
    walFile.open(filename, std::ios::out | std::ios::trunc);
    walFile.close();
    // Reopen in append mode
    walFile.open(filename, std::ios::app);
    walFile << std::unitbuf;
}

void WALWriter::replay(const std::string& filename, MemTable& memTable){
    std::ifstream file(filename);
    std::string line;

    if(!file.is_open()){
        std::cerr << "Could not open file " << filename << std::endl;
        return;
    }

    while(std::getline(file,line)){
        std::istringstream iss(line);
        std::string action, key;
        iss >> action >> key;
        if(action == "PUT"){
            // Read the rest of the line as value (handles values with spaces)
            std::string value;
            std::getline(iss >> std::ws, value);
            memTable.put(key,value);
        } else if(action == "DEL"){
            memTable.remove(key);
        } else{
            std::cerr << "Unknown Entry: " << line << std::endl;
        }
    }
}