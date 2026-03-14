#include "block.h"
#include <cstring>
#include <algorithm>

size_t Block::calculateEntrySize(const std::string& key, const std::string& value) {
    // Entry format: [key_len:4][key][val_len:4][val]
    return sizeof(uint32_t) +  // key length
           key.size() +         // key data
           sizeof(uint32_t) +   // value length
           value.size();        // value data
}

bool Block::addEntry(const std::string& key, const std::string& value) {
    size_t entrySize = calculateEntrySize(key, value);
    size_t newSize = currentSize + entrySize;

    // First entry includes header
    if (isEmpty()) {
        newSize += HEADER_SIZE;
    }

    if (newSize > MAX_BLOCK_SIZE) {
        return false;  // Block would be too large
    }

    entries[key] = value;
    currentSize = newSize;
    return true;
}

bool Block::isFull() const {
    // Consider block full if it's at least 90% of max size
    // This prevents edge cases where we can't fit even small entries
    return currentSize >= (MAX_BLOCK_SIZE * 9 / 10);
}

std::optional<std::string> Block::find(const std::string& key) const {
    auto it = entries.find(key);
    if (it != entries.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string Block::getFirstKey() const {
    if (entries.empty()) {
        return "";
    }
    return entries.begin()->first;
}

std::string Block::getLastKey() const {
    if (entries.empty()) {
        return "";
    }
    return entries.rbegin()->first;
}

void Block::clear() {
    entries.clear();
    currentSize = 0;
}

std::vector<uint8_t> Block::serialize() const {
    std::vector<uint8_t> buffer;

    // Calculate total size needed
    size_t totalSize = HEADER_SIZE;
    for (const auto& [key, value] : entries) {
        totalSize += calculateEntrySize(key, value);
    }

    buffer.reserve(totalSize);

    // Write header
    uint32_t numEntries = static_cast<uint32_t>(entries.size());
    uint32_t blockSize = static_cast<uint32_t>(totalSize);

    // Write num_entries
    buffer.insert(buffer.end(),
                  reinterpret_cast<const uint8_t*>(&numEntries),
                  reinterpret_cast<const uint8_t*>(&numEntries) + sizeof(uint32_t));

    // Write block_size
    buffer.insert(buffer.end(),
                  reinterpret_cast<const uint8_t*>(&blockSize),
                  reinterpret_cast<const uint8_t*>(&blockSize) + sizeof(uint32_t));

    // Write entries
    for (const auto& [key, value] : entries) {
        // Write key length
        uint32_t keyLen = static_cast<uint32_t>(key.size());
        buffer.insert(buffer.end(),
                      reinterpret_cast<const uint8_t*>(&keyLen),
                      reinterpret_cast<const uint8_t*>(&keyLen) + sizeof(uint32_t));

        // Write key data
        buffer.insert(buffer.end(), key.begin(), key.end());

        // Write value length
        uint32_t valLen = static_cast<uint32_t>(value.size());
        buffer.insert(buffer.end(),
                      reinterpret_cast<const uint8_t*>(&valLen),
                      reinterpret_cast<const uint8_t*>(&valLen) + sizeof(uint32_t));

        // Write value data
        buffer.insert(buffer.end(), value.begin(), value.end());
    }

    return buffer;
}

Block Block::deserialize(const std::vector<uint8_t>& data) {
    Block block;

    if (data.size() < HEADER_SIZE) {
        return block;  // Invalid data
    }

    size_t offset = 0;

    // Read header
    uint32_t numEntries;
    uint32_t blockSize;

    std::memcpy(&numEntries, &data[offset], sizeof(uint32_t));
    offset += sizeof(uint32_t);

    std::memcpy(&blockSize, &data[offset], sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Read entries
    for (uint32_t i = 0; i < numEntries; ++i) {
        if (offset + sizeof(uint32_t) > data.size()) {
            break;  // Invalid data
        }

        // Read key length
        uint32_t keyLen;
        std::memcpy(&keyLen, &data[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);

        if (offset + keyLen > data.size()) {
            break;  // Invalid data
        }

        // Read key
        std::string key(reinterpret_cast<const char*>(&data[offset]), keyLen);
        offset += keyLen;

        if (offset + sizeof(uint32_t) > data.size()) {
            break;  // Invalid data
        }

        // Read value length
        uint32_t valLen;
        std::memcpy(&valLen, &data[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);

        if (offset + valLen > data.size()) {
            break;  // Invalid data
        }

        // Read value
        std::string value(reinterpret_cast<const char*>(&data[offset]), valLen);
        offset += valLen;

        block.entries[key] = value;
    }

    // Recalculate current size
    block.currentSize = HEADER_SIZE;
    for (const auto& [key, value] : block.entries) {
        block.currentSize += calculateEntrySize(key, value);
    }

    return block;
}
