#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "engine/wal_writer.h"
#include "engine/memtable.h"

class WALTest : public ::testing::Test {
protected:
    const std::string test_wal_path = "/tmp/test_wal.log";

    void SetUp() override {
        // Clean up any existing test WAL file
        if (std::filesystem::exists(test_wal_path)) {
            std::filesystem::remove(test_wal_path);
        }
    }

    void TearDown() override {
        // Clean up test WAL file after test
        if (std::filesystem::exists(test_wal_path)) {
            std::filesystem::remove(test_wal_path);
        }
    }

    std::string readWALFile() {
        std::ifstream file(test_wal_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

TEST_F(WALTest, LogPutOperation) {
    WALWriter wal(test_wal_path);
    wal.logPut("key1", "value1");

    std::string contents = readWALFile();
    EXPECT_TRUE(contents.find("PUT key1 value1") != std::string::npos);
}

TEST_F(WALTest, LogDeleteOperation) {
    WALWriter wal(test_wal_path);
    wal.logDelete("key1");

    std::string contents = readWALFile();
    EXPECT_TRUE(contents.find("DEL key1") != std::string::npos);
}

TEST_F(WALTest, LogMultipleOperations) {
    WALWriter wal(test_wal_path);
    wal.logPut("key1", "value1");
    wal.logPut("key2", "value2");
    wal.logDelete("key3");

    std::string contents = readWALFile();
    EXPECT_TRUE(contents.find("PUT key1 value1") != std::string::npos);
    EXPECT_TRUE(contents.find("PUT key2 value2") != std::string::npos);
    EXPECT_TRUE(contents.find("DEL key3") != std::string::npos);
}

TEST_F(WALTest, ReplayPutOperations) {
    {
        WALWriter wal(test_wal_path);
        wal.logPut("key1", "value1");
        wal.logPut("key2", "value2");
    }

    MemTable memtable;
    WALWriter::replay(test_wal_path, memtable);

    auto result1 = memtable.get("key1");
    auto result2 = memtable.get("key2");

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result1.value(), "value1");
    EXPECT_EQ(result2.value(), "value2");
}

TEST_F(WALTest, ReplayDeleteOperations) {
    {
        WALWriter wal(test_wal_path);
        wal.logPut("key1", "value1");
        wal.logDelete("key1");
    }

    MemTable memtable;
    WALWriter::replay(test_wal_path, memtable);

    auto result = memtable.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");  // Tombstone
}

TEST_F(WALTest, ReplayMixedOperations) {
    {
        WALWriter wal(test_wal_path);
        wal.logPut("key1", "value1");
        wal.logPut("key2", "value2");
        wal.logDelete("key1");
        wal.logPut("key3", "value3");
    }

    MemTable memtable;
    WALWriter::replay(test_wal_path, memtable);

    auto result1 = memtable.get("key1");
    auto result2 = memtable.get("key2");
    auto result3 = memtable.get("key3");

    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), "");  // Deleted

    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), "value2");

    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(result3.value(), "value3");
}

TEST_F(WALTest, ReplayNonExistentFile) {
    MemTable memtable;
    // Should not crash when replaying non-existent file
    EXPECT_NO_THROW(WALWriter::replay("/tmp/nonexistent_wal.log", memtable));
}

TEST_F(WALTest, ReplayEmptyFile) {
    // Create empty WAL file
    std::ofstream file(test_wal_path);
    file.close();

    MemTable memtable;
    WALWriter::replay(test_wal_path, memtable);

    EXPECT_EQ(memtable.size(), 0);
}