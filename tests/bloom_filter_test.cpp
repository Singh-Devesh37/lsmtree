#include <gtest/gtest.h>
#include "utils/bloom_filter.h"

class BloomFilterTest : public ::testing::Test {
protected:
    BloomFilter bloom{1024};
};

TEST_F(BloomFilterTest, BasicAddAndContains) {
    bloom.add("key1");
    bloom.add("key2");
    bloom.add("key3");

    EXPECT_TRUE(bloom.mightContain("key1"));
    EXPECT_TRUE(bloom.mightContain("key2"));
    EXPECT_TRUE(bloom.mightContain("key3"));
}

TEST_F(BloomFilterTest, NonExistentKeyMayReturnFalse) {
    bloom.add("existing_key");

    // This key was never added, so bloom filter should
    // either return false (correct) or true (false positive)
    // We can't assert false positives, but we can verify
    // the added key is definitely present
    EXPECT_TRUE(bloom.mightContain("existing_key"));
}

TEST_F(BloomFilterTest, EmptyBloomFilter) {
    // Empty bloom filter should return false for any key
    EXPECT_FALSE(bloom.mightContain("any_key"));
    EXPECT_FALSE(bloom.mightContain("another_key"));
}

TEST_F(BloomFilterTest, SerializeAndDeserialize) {
    bloom.add("key1");
    bloom.add("key2");
    bloom.add("key3");

    std::string serialized = bloom.serialize();

    BloomFilter bloom2{1024};
    bloom2.deserialize(serialized);

    EXPECT_TRUE(bloom2.mightContain("key1"));
    EXPECT_TRUE(bloom2.mightContain("key2"));
    EXPECT_TRUE(bloom2.mightContain("key3"));
}

TEST_F(BloomFilterTest, SerializedSizeMatchesBloomSize) {
    std::string serialized = bloom.serialize();
    EXPECT_EQ(serialized.size(), 1024);
}

TEST_F(BloomFilterTest, SerializedFormatIsBinaryString) {
    bloom.add("test_key");
    std::string serialized = bloom.serialize();

    // Check that serialized string contains only '0' and '1'
    for (char c : serialized) {
        EXPECT_TRUE(c == '0' || c == '1');
    }
}

TEST_F(BloomFilterTest, DifferentKeysProduceDifferentPatterns) {
    BloomFilter bloom1{1024};
    BloomFilter bloom2{1024};

    bloom1.add("key1");
    bloom2.add("key2");

    // While there might be collisions, different keys
    // should generally produce different patterns
    std::string serialized1 = bloom1.serialize();
    std::string serialized2 = bloom2.serialize();

    EXPECT_NE(serialized1, serialized2);
}

TEST_F(BloomFilterTest, CustomSize) {
    BloomFilter small_bloom{512};
    small_bloom.add("key1");

    std::string serialized = small_bloom.serialize();
    EXPECT_EQ(serialized.size(), 512);

    EXPECT_TRUE(small_bloom.mightContain("key1"));
}

TEST_F(BloomFilterTest, MultipleAddsOfSameKey) {
    bloom.add("key1");
    bloom.add("key1");
    bloom.add("key1");

    // Adding the same key multiple times should still work
    EXPECT_TRUE(bloom.mightContain("key1"));
}