#include <gtest/gtest.h>
#include "engine/block.h"

TEST(BlockTest, AddEntry) {
    Block block;

    EXPECT_TRUE(block.addEntry("key1", "value1"));
    EXPECT_FALSE(block.isEmpty());
    EXPECT_EQ(block.getNumEntries(), 1);
}

TEST(BlockTest, FindEntry) {
    Block block;

    block.addEntry("apple", "red");
    block.addEntry("banana", "yellow");
    block.addEntry("cherry", "red");

    auto result = block.find("banana");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "yellow");

    auto notFound = block.find("grape");
    EXPECT_FALSE(notFound.has_value());
}

TEST(BlockTest, GetFirstAndLastKey) {
    Block block;

    block.addEntry("mango", "orange");
    block.addEntry("apple", "red");
    block.addEntry("zebra", "stripes");

    // Keys are sorted in std::map
    EXPECT_EQ(block.getFirstKey(), "apple");
    EXPECT_EQ(block.getLastKey(), "zebra");
}

TEST(BlockTest, SerializeDeserialize) {
    Block original;

    original.addEntry("key1", "value1");
    original.addEntry("key2", "value2");
    original.addEntry("key3", "value3");

    std::vector<uint8_t> serialized = original.serialize();
    Block deserialized = Block::deserialize(serialized);

    EXPECT_EQ(deserialized.getNumEntries(), 3);

    auto val1 = deserialized.find("key1");
    ASSERT_TRUE(val1.has_value());
    EXPECT_EQ(val1.value(), "value1");

    auto val2 = deserialized.find("key2");
    ASSERT_TRUE(val2.has_value());
    EXPECT_EQ(val2.value(), "value2");

    auto val3 = deserialized.find("key3");
    ASSERT_TRUE(val3.has_value());
    EXPECT_EQ(val3.value(), "value3");
}

TEST(BlockTest, BlockSizeLimit) {
    Block block;

    // Add entries until block is full
    std::string largeValue(1000, 'x');  // 1KB value

    int count = 0;
    while (block.addEntry("key" + std::to_string(count), largeValue)) {
        count++;
        if (count > 10) break;  // Safety limit
    }

    // Should have added a few entries
    EXPECT_GT(count, 0);
    EXPECT_LE(block.getSize(), Block::MAX_BLOCK_SIZE);

    // Block should be full or nearly full
    EXPECT_TRUE(block.isFull() || !block.addEntry("extraKey", largeValue));
}

TEST(BlockTest, Clear) {
    Block block;

    block.addEntry("key1", "value1");
    block.addEntry("key2", "value2");

    EXPECT_EQ(block.getNumEntries(), 2);

    block.clear();

    EXPECT_EQ(block.getNumEntries(), 0);
    EXPECT_TRUE(block.isEmpty());
    EXPECT_EQ(block.getSize(), 0);
}

TEST(BlockTest, EmptyBlockSerialization) {
    Block empty;

    std::vector<uint8_t> serialized = empty.serialize();
    Block deserialized = Block::deserialize(serialized);

    EXPECT_TRUE(deserialized.isEmpty());
    EXPECT_EQ(deserialized.getNumEntries(), 0);
}
