#include <gtest/gtest.h>
#include <filesystem>
#include "engine/lsm_engine.h"

class LSMEngineTest : public ::testing::Test {
protected:
    const std::string test_dir = "/tmp/lsm_test";

    void SetUp() override {
        // Clean up test directory if it exists
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
        std::filesystem::create_directories(test_dir);
    }

    void TearDown() override {
        // Clean up after tests
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }
};

TEST_F(LSMEngineTest, BasicPutAndGet) {
    LSMEngine engine(test_dir, 10, 3);

    engine.put("key1", "value1");
    engine.put("key2", "value2");

    EXPECT_EQ(engine.get("key1"), "value1");
    EXPECT_EQ(engine.get("key2"), "value2");
}

TEST_F(LSMEngineTest, GetNonExistentKey) {
    LSMEngine engine(test_dir, 10, 3);

    std::string result = engine.get("nonexistent");
    EXPECT_EQ(result, "");
}

TEST_F(LSMEngineTest, UpdateExistingKey) {
    LSMEngine engine(test_dir, 10, 3);

    engine.put("key1", "value1");
    engine.put("key1", "value2");

    EXPECT_EQ(engine.get("key1"), "value2");
}

TEST_F(LSMEngineTest, DeleteKey) {
    LSMEngine engine(test_dir, 10, 3);

    engine.put("key1", "value1");
    engine.remove("key1");

    std::string result = engine.get("key1");
    EXPECT_EQ(result, "");  // Deleted key returns empty string
}

TEST_F(LSMEngineTest, AutoFlushOnThreshold) {
    LSMEngine engine(test_dir, 3, 3);  // Flush threshold = 3

    engine.put("key1", "value1");
    engine.put("key2", "value2");
    engine.put("key3", "value3");  // This should trigger flush

    // After flush, data should still be retrievable from SSTable
    EXPECT_EQ(engine.get("key1"), "value1");
    EXPECT_EQ(engine.get("key2"), "value2");
    EXPECT_EQ(engine.get("key3"), "value3");

    // Check that SSTable files were created
    std::string level0_dir = test_dir + "/ssTables/0";
    EXPECT_TRUE(std::filesystem::exists(level0_dir));
}

TEST_F(LSMEngineTest, ManualFlush) {
    LSMEngine engine(test_dir, 100, 3);  // High threshold

    engine.put("key1", "value1");
    engine.flush();

    // After flush, MemTable should be cleared but data still accessible
    EXPECT_EQ(engine.get("key1"), "value1");

    // Verify SSTable was created
    std::string level0_dir = test_dir + "/ssTables/0";
    EXPECT_TRUE(std::filesystem::exists(level0_dir));
}

TEST_F(LSMEngineTest, CrashRecovery) {
    // Write data and simulate crash (destroy engine without clean shutdown)
    {
        LSMEngine engine(test_dir, 100, 3);  // High threshold - won't auto-flush
        engine.put("key1", "value1");
        engine.put("key2", "value2");
        // Engine destroyed here - data only in WAL
    }

    // Restart engine - should recover from WAL
    {
        LSMEngine engine(test_dir, 100, 3);
        EXPECT_EQ(engine.get("key1"), "value1");
        EXPECT_EQ(engine.get("key2"), "value2");
    }
}

TEST_F(LSMEngineTest, RecoveryAfterFlush) {
    {
        LSMEngine engine(test_dir, 3, 3);
        engine.put("key1", "value1");
        engine.put("key2", "value2");
        engine.put("key3", "value3");  // Auto-flush
        engine.put("key4", "value4");  // In MemTable only
    }

    // Restart - should recover key4 from WAL and others from SSTable
    {
        LSMEngine engine(test_dir, 3, 3);
        EXPECT_EQ(engine.get("key1"), "value1");
        EXPECT_EQ(engine.get("key2"), "value2");
        EXPECT_EQ(engine.get("key3"), "value3");
        EXPECT_EQ(engine.get("key4"), "value4");
    }
}

TEST_F(LSMEngineTest, DeleteAndRecovery) {
    {
        LSMEngine engine(test_dir, 100, 3);
        engine.put("key1", "value1");
        engine.remove("key1");
    }

    // After restart, key should still be deleted
    {
        LSMEngine engine(test_dir, 100, 3);
        EXPECT_EQ(engine.get("key1"), "");
    }
}

TEST_F(LSMEngineTest, MultipleOperationsSequence) {
    LSMEngine engine(test_dir, 100, 3);

    // Complex sequence of operations
    engine.put("key1", "value1");
    engine.put("key2", "value2");
    engine.remove("key1");
    engine.put("key3", "value3");
    engine.put("key2", "updated_value2");

    EXPECT_EQ(engine.get("key1"), "");
    EXPECT_EQ(engine.get("key2"), "updated_value2");
    EXPECT_EQ(engine.get("key3"), "value3");
}