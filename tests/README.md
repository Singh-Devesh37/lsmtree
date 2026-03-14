# LSM Tree Tests

This directory contains unit and integration tests for the LSM Tree storage engine.

## Test Coverage

### Unit Tests

1. **memtable_test.cpp** - MemTable component tests (10 tests)
   - Basic put/get operations
   - Update existing keys
   - Tombstone handling
   - Size tracking
   - Clear functionality
   - Key ordering (sorted behavior)

2. **bloom_filter_test.cpp** - Bloom Filter tests (9 tests)
   - Add and contains operations
   - Serialization/deserialization
   - False positive behavior
   - Empty filter behavior
   - Custom size configuration

3. **wal_test.cpp** - Write-Ahead Log tests (8 tests)
   - Log put operations
   - Log delete operations
   - Replay functionality
   - Crash recovery simulation
   - Handling of malformed/missing files
   - Values with spaces (edge case)

4. **block_test.cpp** - Block-based storage tests (8 tests)
   - Block serialization/deserialization
   - Entry management and size limits
   - Binary format correctness
   - Empty block handling

### Integration Tests

5. **integration_test.cpp** - End-to-end LSM Engine tests (10 tests)
   - Basic put/get/delete operations
   - Auto-flush on threshold
   - Manual flush
   - Crash recovery scenarios
   - Multi-level compaction

6. **range_query_test.cpp** - Range scan tests (11 tests)
   - MemTable range queries
   - SSTable range queries
   - Multi-level merging
   - Tombstone filtering
   - Newest value precedence

7. **config_test.cpp** - Configuration tests (7 tests)
   - JSON parsing
   - Default values
   - Invalid configurations
   - Feature flags

**Total: 63 tests covering all major components**

## Running Tests

### Build and Run All Tests

```bash
# From project root
mkdir build && cd build
cmake ..
make

# Run all tests
ctest

# Or run with verbose output
ctest --verbose
```

### Run Individual Test Suites

```bash
# From build directory
./memtable_test
./bloom_filter_test
./wal_test
./integration_test
```

### Run Specific Test Cases

```bash
# Run only tests matching a pattern
./memtable_test --gtest_filter=MemTableTest.PutAndGet
./integration_test --gtest_filter=LSMEngineTest.CrashRecovery
```

## Test Framework

Using **Google Test (gtest)** v1.14.0, automatically downloaded via CMake FetchContent.

## Adding New Tests

1. Create a new test file in `tests/` directory
2. Include necessary headers and Google Test
3. Write test cases using `TEST()` or `TEST_F()` macros
4. Update `CMakeLists.txt` to add the new test executable
5. Link against `GTest::gtest_main`
6. Register with `gtest_discover_tests()`

### Example Test Template

```cpp
#include <gtest/gtest.h>
#include "engine/component.h"

class ComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }
};

TEST_F(ComponentTest, BasicFunctionality) {
    // Test code
    EXPECT_EQ(actual, expected);
    ASSERT_TRUE(condition);
}
```

## Test Best Practices

1. **Isolation**: Each test should be independent
2. **Cleanup**: Use SetUp/TearDown to clean test artifacts
3. **Assertions**: Use EXPECT_* for non-fatal, ASSERT_* for fatal checks
4. **Naming**: Use descriptive test names (TestSuite.WhatIsBeingTested)
5. **Coverage**: Test both happy paths and edge cases

## Test Results

All tests pass successfully:
```
[==========] 63 tests from 7 test suites
[  PASSED  ] 63 tests
```

## Benchmarking

In addition to unit tests, the project includes a comprehensive benchmarking suite:

```bash
# Run all YCSB-style workloads
./lsm_benchmark

# Run specific workload
./lsm_benchmark -w B -n 10000

# Custom configuration
./lsm_benchmark -w A -n 50000 -v 200
```

**Workload Types:**
- **A**: Read-Heavy (50% read, 50% update) - Session stores
- **B**: Read-Mostly (95% read, 5% update) - Caching workload
- **C**: Write-Heavy (100% insert) - Logging/streaming
- **D**: Read-Latest (95% read recent, 5% insert) - Social feeds

## Completed Test Coverage

- [x] SSTable read/write tests
- [x] Compaction logic tests
- [x] Block-based format tests
- [x] Performance/benchmark tests (YCSB suite)
- [x] Corruption/error handling tests
- [x] Range query tests
- [x] Crash recovery tests

## Code Coverage

To generate code coverage reports (requires gcov/lcov):

```bash
# Build with coverage flags
cmake -DCMAKE_CXX_FLAGS="--coverage" ..
make
ctest

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```