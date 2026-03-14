#include <gtest/gtest.h>
#include "engine/memtable.h"

class MemTableTest : public ::testing::Test {
protected:
    MemTable memtable;
};

TEST_F(MemTableTest, PutAndGet) {
    memtable.put("key1", "value1");
    memtable.put("key2", "value2");

    auto result1 = memtable.get("key1");
    auto result2 = memtable.get("key2");

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result1.value(), "value1");
    EXPECT_EQ(result2.value(), "value2");
}

TEST_F(MemTableTest, GetNonExistentKey) {
    auto result = memtable.get("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(MemTableTest, UpdateExistingKey) {
    memtable.put("key1", "value1");
    memtable.put("key1", "value2");

    auto result = memtable.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value2");
}

TEST_F(MemTableTest, TombstoneHandling) {
    memtable.put("key1", "value1");
    memtable.remove("key1");

    auto result = memtable.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");  // Tombstone returns empty string
}

TEST_F(MemTableTest, RemoveNonExistentKey) {
    memtable.remove("nonexistent");

    auto result = memtable.get("nonexistent");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");  // Tombstone marker
}

TEST_F(MemTableTest, SizeTracking) {
    EXPECT_EQ(memtable.size(), 0);

    memtable.put("key1", "value1");
    EXPECT_EQ(memtable.size(), 1);

    memtable.put("key2", "value2");
    EXPECT_EQ(memtable.size(), 2);

    memtable.put("key1", "updated");  // Update doesn't increase size
    EXPECT_EQ(memtable.size(), 2);
}

TEST_F(MemTableTest, ClearMemTable) {
    memtable.put("key1", "value1");
    memtable.put("key2", "value2");

    EXPECT_EQ(memtable.size(), 2);

    memtable.clear();
    EXPECT_EQ(memtable.size(), 0);

    auto result = memtable.get("key1");
    EXPECT_FALSE(result.has_value());
}

TEST_F(MemTableTest, DataAccessReturnsConstReference) {
    memtable.put("key1", "value1");
    memtable.put("key2", "value2");

    const auto& data = memtable.data();

    EXPECT_EQ(data.size(), 2);
    EXPECT_EQ(data.at("key1"), "value1");
    EXPECT_EQ(data.at("key2"), "value2");
}

TEST_F(MemTableTest, KeysAreSorted) {
    memtable.put("zebra", "z");
    memtable.put("apple", "a");
    memtable.put("mango", "m");

    const auto& data = memtable.data();
    auto it = data.begin();

    EXPECT_EQ(it->first, "apple");
    ++it;
    EXPECT_EQ(it->first, "mango");
    ++it;
    EXPECT_EQ(it->first, "zebra");
}