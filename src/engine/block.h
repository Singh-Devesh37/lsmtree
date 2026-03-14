#pragma once

#include <string>
#include <map>
#include <optional>
#include <cstdint>
#include <vector>

class Block {
public:
    static const size_t MAX_BLOCK_SIZE = 4096;  // 4KB

    Block() = default;

    // Add an entry to the block
    // Returns false if block would exceed MAX_BLOCK_SIZE
    bool addEntry(const std::string& key, const std::string& value);

    // Check if block is full
    bool isFull() const;

    // Find a key in the block (binary search since entries are sorted)
    std::optional<std::string> find(const std::string& key) const;

    // Get the first key in the block (for indexing)
    std::string getFirstKey() const;

    // Get the last key in the block
    std::string getLastKey() const;

    // Get current size in bytes
    size_t getSize() const { return currentSize; }

    // Get number of entries
    size_t getNumEntries() const { return entries.size(); }

    // Check if block is empty
    bool isEmpty() const { return entries.empty(); }

    // Clear all entries
    void clear();

    // Serialize block to binary format
    // Format: [num_entries:4][block_size:4][[key_len:4][key][val_len:4][val]]...
    std::vector<uint8_t> serialize() const;

    // Deserialize block from binary data
    static Block deserialize(const std::vector<uint8_t>& data);

    // Get all entries (for testing/debugging)
    const std::map<std::string, std::string>& getEntries() const { return entries; }

private:
    std::map<std::string, std::string> entries;  // Sorted by key
    size_t currentSize = 0;  // Current size in bytes (header + entries)

    // Calculate size of an entry in bytes
    static size_t calculateEntrySize(const std::string& key, const std::string& value);

    // Header size: num_entries (4 bytes) + block_size (4 bytes)
    static const size_t HEADER_SIZE = 8;
};
