cmake_minimum_required(VERSION 3.15)
project(lsm_tree_cpp)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Include source directories
include_directories(src)

# Core engine sources
file(GLOB_RECURSE ENGINE_SRC
    "src/engine/*.cpp"
    "src/utils/*.cpp"
)

# CLI executable
add_executable(lsm_cli src/cli/main.cpp ${ENGINE_SRC})

# Testing
enable_testing()

# Google Test setup (FetchContent method - downloads automatically)
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test executables
add_executable(memtable_test tests/memtable_test.cpp src/engine/memtable.cpp)
target_link_libraries(memtable_test GTest::gtest_main)

add_executable(bloom_filter_test tests/bloom_filter_test.cpp)
target_link_libraries(bloom_filter_test GTest::gtest_main)

add_executable(wal_test tests/wal_test.cpp src/engine/wal_writer.cpp src/engine/memtable.cpp)
target_link_libraries(wal_test GTest::gtest_main)

add_executable(integration_test tests/integration_test.cpp ${ENGINE_SRC})
target_link_libraries(integration_test GTest::gtest_main)

add_executable(range_query_test tests/range_query_test.cpp ${ENGINE_SRC})
target_link_libraries(range_query_test GTest::gtest_main)

add_executable(config_test tests/config_test.cpp)
target_link_libraries(config_test GTest::gtest_main)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(memtable_test)
gtest_discover_tests(bloom_filter_test)
gtest_discover_tests(wal_test)
gtest_discover_tests(integration_test)
gtest_discover_tests(range_query_test)
gtest_discover_tests(config_test)

# Benchmark executable
add_executable(lsm_benchmark src/benchmark/benchmark.cpp ${ENGINE_SRC})
target_include_directories(lsm_benchmark PRIVATE src)

# Block tests
add_executable(block_test tests/block_test.cpp src/engine/block.cpp)
target_link_libraries(block_test GTest::gtest_main)
gtest_discover_tests(block_test)