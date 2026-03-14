#include <gtest/gtest.h>
#include "engine/memtable.h"
#include "engine/lsm_engine.h"
#include <filesystem>

class RangeQueryTest : public ::testing::Test {
protected:
    MemTable memtable;
};

TEST_F(RangeQueryTest, ScanEmptyMemTable) {
    auto results = memtable.scan("a", "z");
    EXPECT_TRUE(results.empty());
}

TEST_F(RangeQueryTest, ScanBasicRange) {
    memtable.put("apple", "fruit1");
    memtable.put("banana", "fruit2");
    memtable.put("cherry", "fruit3");
    memtable.put("date", "fruit4");

    auto results = memtable.scan("banana", "cherry");

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].first, "banana");
    EXPECT_EQ(results[0].second, "fruit2");
    EXPECT_EQ(results[1].first, "cherry");
    EXPECT_EQ(results[1].second, "fruit3");
}

TEST_F(RangeQueryTest, ScanInclusiveRange) {
    memtable.put("a", "1");
    memtable.put("b", "2");
    memtable.put("c", "3");
    memtable.put("d", "4");
    memtable.put("e", "5");

    auto results = memtable.scan("b", "d");

    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0].first, "b");
    EXPECT_EQ(results[1].first, "c");
    EXPECT_EQ(results[2].first, "d");
}

TEST_F(RangeQueryTest, ScanExcludesTombstones) {
    memtable.put("a", "1");
    memtable.put("b", "2");
    memtable.put("c", "3");
    memtable.remove("b");  // Tombstone

    auto results = memtable.scan("a", "c");

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].first, "a");
    EXPECT_EQ(results[1].first, "c");
}

TEST_F(RangeQueryTest, ScanNoMatches) {
    memtable.put("a", "1");
    memtable.put("z", "26");

    auto results = memtable.scan("m", "n");

    EXPECT_TRUE(results.empty());
}

TEST_F(RangeQueryTest, ScanSingleKey) {
    memtable.put("key1", "value1");
    memtable.put("key2", "value2");
    memtable.put("key3", "value3");

    auto results = memtable.scan("key2", "key2");

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].first, "key2");
    EXPECT_EQ(results[0].second, "value2");
}

TEST_F(RangeQueryTest, ScanResultsAreSorted) {
    memtable.put("zebra", "z");
    memtable.put("apple", "a");
    memtable.put("mango", "m");
    memtable.put("banana", "b");

    auto results = memtable.scan("a", "z");

    ASSERT_EQ(results.size(), 4);
    EXPECT_EQ(results[0].first, "apple");
    EXPECT_EQ(results[1].first, "banana");
    EXPECT_EQ(results[2].first, "mango");
    EXPECT_EQ(results[3].first, "zebra");
}

class LSMEngineRangeTest : public ::testing::Test {
protected:
    const std::string test_dir = "/tmp/lsm_range_test";

    void SetUp() override {
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
        std::filesystem::create_directories(test_dir);
    }

    void TearDown() override {
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }
};

TEST_F(LSMEngineRangeTest, ScanMemTableOnly) {
    LSMEngine engine(test_dir, 100, 3);  // High threshold - won't flush

    engine.put("key1", "value1");
    engine.put("key2", "value2");
    engine.put("key3", "value3");

    auto results = engine.scan("key1", "key3");

    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0].first, "key1");
    EXPECT_EQ(results[1].first, "key2");
    EXPECT_EQ(results[2].first, "key3");
}

TEST_F(LSMEngineRangeTest, ScanAcrossMemTableAndSSTable) {
    LSMEngine engine(test_dir, 2, 3);  // Low threshold

    engine.put("key1", "value1");
    engine.put("key2", "value2");  // This will trigger flush
    engine.put("key3", "value3");
    engine.put("key4", "value4");  // Another flush

    auto results = engine.scan("key1", "key4");

    ASSERT_EQ(results.size(), 4);
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(results[i].first, "key" + std::to_string(i + 1));
        EXPECT_EQ(results[i].second, "value" + std::to_string(i + 1));
    }
}

TEST_F(LSMEngineRangeTest, ScanReturnsNewestValues) {
    LSMEngine engine(test_dir, 2, 3);

    engine.put("key1", "old_value");
    engine.put("key2", "value2");  // Flush
    engine.put("key1", "new_value");  // Update in MemTable

    auto results = engine.scan("key1", "key2");

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].second, "new_value");  // Should get newest value from MemTable
    EXPECT_EQ(results[1].second, "value2");
}

TEST_F(LSMEngineRangeTest, ScanExcludesDeletedKeys) {
    LSMEngine engine(test_dir, 100, 3);

    engine.put("key1", "value1");
    engine.put("key2", "value2");
    engine.put("key3", "value3");
    engine.remove("key2");

    auto results = engine.scan("key1", "key3");

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].first, "key1");
    EXPECT_EQ(results[1].first, "key3");
}