#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

struct LSMConfig {
    std::string baseDir = "data";
    int flushThreshold = 100;
    int maxLevels = 5;
    int bloomFilterSize = 1024;
    bool useBlockBasedFormat = true;  // Use block-based SSTables by default

    // Load configuration from JSON file
    static LSMConfig load(const std::string& filepath) {
        LSMConfig config;
        std::ifstream file(filepath);

        if (!file.is_open()) {
            std::cerr << "Warning: Could not open config file '" << filepath
                      << "'. Using default configuration." << std::endl;
            return config;
        }

        std::string line, content;
        while (std::getline(file, line)) {
            // Remove whitespace
            line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
            // Skip comments and empty lines
            if (line.empty() || line[0] == '/' || line[0] == '{' || line[0] == '}') {
                continue;
            }
            content += line;
        }

        // Simple JSON parser for our specific config format
        try {
            config.baseDir = parseStringValue(content, "baseDir");
            config.flushThreshold = parseIntValue(content, "flushThreshold");
            config.maxLevels = parseIntValue(content, "maxLevels");
            config.bloomFilterSize = parseIntValue(content, "bloomFilterSize");
            config.useBlockBasedFormat = parseBoolValue(content, "useBlockBasedFormat");
        } catch (const std::exception& e) {
            std::cerr << "Error parsing config: " << e.what() << std::endl;
            std::cerr << "Using default configuration." << std::endl;
        }

        return config;
    }

    void print() const {
        std::cout << "LSM Configuration:" << std::endl;
        std::cout << "  Base Directory: " << baseDir << std::endl;
        std::cout << "  Flush Threshold: " << flushThreshold << std::endl;
        std::cout << "  Max Levels: " << maxLevels << std::endl;
        std::cout << "  Bloom Filter Size: " << bloomFilterSize << std::endl;
        std::cout << "  Block-Based Format: " << (useBlockBasedFormat ? "enabled" : "disabled") << std::endl;
    }

private:
    static std::string parseStringValue(const std::string& content, const std::string& key) {
        std::string searchKey = "\"" + key + "\":\"";
        size_t pos = content.find(searchKey);
        if (pos == std::string::npos) {
            throw std::runtime_error("Key '" + key + "' not found");
        }

        pos += searchKey.length();
        size_t endPos = content.find("\"", pos);
        if (endPos == std::string::npos) {
            throw std::runtime_error("Invalid string value for key '" + key + "'");
        }

        return content.substr(pos, endPos - pos);
    }

    static int parseIntValue(const std::string& content, const std::string& key) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = content.find(searchKey);
        if (pos == std::string::npos) {
            throw std::runtime_error("Key '" + key + "' not found");
        }

        pos += searchKey.length();
        size_t endPos = content.find_first_of(",}", pos);
        if (endPos == std::string::npos) {
            throw std::runtime_error("Invalid int value for key '" + key + "'");
        }

        std::string valueStr = content.substr(pos, endPos - pos);
        return std::stoi(valueStr);
    }

    static bool parseBoolValue(const std::string& content, const std::string& key) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = content.find(searchKey);
        if (pos == std::string::npos) {
            // If key not found, return default (true)
            return true;
        }

        pos += searchKey.length();
        size_t endPos = content.find_first_of(",}", pos);
        if (endPos == std::string::npos) {
            return true;
        }

        std::string valueStr = content.substr(pos, endPos - pos);
        return valueStr == "true";
    }
};