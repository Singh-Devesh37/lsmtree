#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>

class BlockSSTable {
public:
    // Write data to a block-based SSTable file
    // Returns the filename of the created SSTable
    static std::string writeToFile(
        const std::map<std::string, std::string>& data,
        const std::string& directory,
        const int& level
    );

    // Read a specific key from a block-based SSTable file
    static std::string readFromFile(
        const std::string& filename,
        const std::string& key
    );

    // Find a key across all block-based SSTables in a directory
    static std::string findInBlockSSTable(
        const std::string& directory,
        const std::string& key
    );

    // Scan a range of keys from block-based SSTables
    static std::vector<std::pair<std::string, std::string>> scanSSTable(
        const std::string& directory,
        const std::string& startKey,
        const std::string& endKey
    );

    // Compact block-based SSTables at a given level
    static void compactSSTables(
        const std::string& directory,
        const int& level
    );

private:
    // Block index entry
    struct BlockIndex {
        std::string firstKey;  // First key in the block
        std::string lastKey;   // Last key in the block
        uint64_t offset;       // Offset in file where block starts
        uint32_t size;         // Size of the block in bytes
    };

    // File footer
    struct Footer {
        uint64_t indexOffset;  // Offset where index starts
        uint32_t indexSize;    // Size of the index in bytes
        uint32_t numBlocks;    // Number of data blocks
        uint32_t magicNumber;  // Magic number for validation (0xBEEFCAFE)

        static const uint32_t MAGIC = 0xBEEFCAFE;
        static const size_t SIZE = 20;  // Total footer size in bytes
    };

    // Read the footer from a file
    static Footer readFooter(const std::string& filename);

    // Read the index from a file
    static std::vector<BlockIndex> readIndex(const std::string& filename, const Footer& footer);

    // Find which block might contain the key (using binary search on index)
    static int findBlockForKey(const std::vector<BlockIndex>& index, const std::string& key);

    // Get next SSTable ID
    static int getNextSSTableID(const std::string& directory);
};
