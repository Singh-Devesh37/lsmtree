#include <gtest/gtest.h>
#include "config/lsm_config.h"
#include <fstream>
#include <filesystem>

class ConfigTest : public ::testing::Test {
protected:
    const std::string test_config_path = "/tmp/test_config.json";

    void TearDown() override {
        if (std::filesystem::exists(test_config_path)) {
            std::filesystem::remove(test_config_path);
        }
    }

    void writeConfigFile(const std::string& content) {
        std::ofstream file(test_config_path);
        file << content;
        file.close();
    }
};

TEST_F(ConfigTest, LoadDefaultConfig) {
    LSMConfig config;

    EXPECT_EQ(config.baseDir, "data");
    EXPECT_EQ(config.flushThreshold, 100);
    EXPECT_EQ(config.maxLevels, 5);
    EXPECT_EQ(config.bloomFilterSize, 1024);
}

TEST_F(ConfigTest, LoadValidConfigFile) {
    writeConfigFile(R"({
  "baseDir": "test_data",
  "flushThreshold": 50,
  "maxLevels": 7,
  "bloomFilterSize": 2048
})");

    LSMConfig config = LSMConfig::load(test_config_path);

    EXPECT_EQ(config.baseDir, "test_data");
    EXPECT_EQ(config.flushThreshold, 50);
    EXPECT_EQ(config.maxLevels, 7);
    EXPECT_EQ(config.bloomFilterSize, 2048);
}

TEST_F(ConfigTest, LoadNonExistentFileUsesDefaults) {
    LSMConfig config = LSMConfig::load("/tmp/nonexistent_config.json");

    // Should use default values
    EXPECT_EQ(config.baseDir, "data");
    EXPECT_EQ(config.flushThreshold, 100);
    EXPECT_EQ(config.maxLevels, 5);
    EXPECT_EQ(config.bloomFilterSize, 1024);
}

TEST_F(ConfigTest, LoadMinimalConfig) {
    writeConfigFile(R"({
  "baseDir": "minimal_data",
  "flushThreshold": 10,
  "maxLevels": 3,
  "bloomFilterSize": 512
})");

    LSMConfig config = LSMConfig::load(test_config_path);

    EXPECT_EQ(config.baseDir, "minimal_data");
    EXPECT_EQ(config.flushThreshold, 10);
    EXPECT_EQ(config.maxLevels, 3);
    EXPECT_EQ(config.bloomFilterSize, 512);
}

TEST_F(ConfigTest, ConfigPrintDoesNotCrash) {
    LSMConfig config;

    // Should not crash
    EXPECT_NO_THROW(config.print());
}

TEST_F(ConfigTest, LoadConfigWithComments) {
    writeConfigFile(R"({
  // This is a comment
  "baseDir": "data_with_comments",
  "flushThreshold": 25,
  "maxLevels": 4,
  "bloomFilterSize": 1024
})");

    LSMConfig config = LSMConfig::load(test_config_path);

    EXPECT_EQ(config.baseDir, "data_with_comments");
    EXPECT_EQ(config.flushThreshold, 25);
}

TEST_F(ConfigTest, LoadConfigWithWhitespace) {
    writeConfigFile(R"(
    {
        "baseDir"         :    "data"     ,
        "flushThreshold"  :    200        ,
        "maxLevels"       :    6          ,
        "bloomFilterSize" :    4096
    }
    )");

    LSMConfig config = LSMConfig::load(test_config_path);

    EXPECT_EQ(config.baseDir, "data");
    EXPECT_EQ(config.flushThreshold, 200);
    EXPECT_EQ(config.maxLevels, 6);
    EXPECT_EQ(config.bloomFilterSize, 4096);
}