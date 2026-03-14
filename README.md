# LSM Tree Storage Engine

A C++20 implementation of a **Log-Structured Merge-Tree (LSM Tree)** storage engine, demonstrating the core concepts behind modern databases like LevelDB, RocksDB, and Cassandra.

## Overview

This project implements a write-optimized key-value storage engine using LSM Tree architecture. It's designed as a learning project to understand how modern database systems achieve high write throughput while maintaining durability and acceptable read performance.

## Features

- **In-Memory MemTable**: Fast writes using `std::map` for sorted key-value storage
- **Write-Ahead Log (WAL)**: Ensures durability and crash recovery with automatic truncation
- **Block-Based SSTables**: Industry-standard binary format with 4KB blocks and indexing
- **Bloom Filters**: Probabilistic data structure to optimize read performance (~90% reduction in unnecessary I/O)
- **Multi-Level Compaction**: Automatic background compaction with size-tiered strategy
- **Range Queries**: Efficient scan operations with multi-level merging
- **YCSB-Style Benchmarking**: Comprehensive performance testing with 4 workload types
- **JSON Configuration**: Flexible runtime configuration with feature flags
- **Tombstone Markers**: Proper delete semantics without immediate data removal
- **Comprehensive Tests**: 50+ unit and integration tests with Google Test
- **Crash Recovery**: WAL replay with proper handling of edge cases

## Architecture

The engine consists of several key components:

- **LSMEngine**: Main orchestration layer coordinating all components
- **MemTable**: In-memory sorted map for active writes
- **WALWriter**: Write-ahead logging for crash recovery
- **SSTable**: Persistent sorted table on disk
- **BloomFilter**: Reduces unnecessary disk reads
- **SSTableIterator**: Sequential iteration through SSTable files

For detailed architecture information, see [Architecture.md](Architecture.md).

## Build Requirements

- **C++20** compatible compiler (GCC 10+, Clang 12+, or MSVC 2019+)
- **CMake** 3.15 or higher
- **Operating System**: Linux, macOS, or Windows

## Building the Project

```bash
# Clone the repository
git clone <your-repo-url>
cd lsmtree

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make

# The executable will be at: build/lsm_cli
```

## Usage

### Interactive CLI

Run the built executable to start the interactive command-line interface:

```bash
./lsm_cli
```

### Available Commands

```
Commands:
  put <key> <value>     # Insert or update a key-value pair
  get <key>             # Retrieve value for a given key
  delete <key>          # Mark a key for deletion (tombstone)
  scan <start> <end>    # Range query: retrieve all keys in [start, end]
  flush                 # Manually flush MemTable to disk
  compact               # Manually trigger compaction
  config                # Display current configuration
  read <filename> <key> # Read directly from an SSTable file
  exit                  # Shutdown and exit
```

### Example Session

```bash
> put name alice
> put age 25
> put city bangalore
> get name
alice

> put apple red
> put banana yellow
> put cherry red
> scan apple cherry
Found 3 results:
  apple -> red
  banana -> yellow
  cherry -> red

> delete age
> flush
> compact
> config
LSM Configuration:
  Base Directory: data
  Flush Threshold: 100
  Max Levels: 5
  Bloom Filter Size: 1024
> exit
```

## Configuration

The engine is configured via JSON file (`resources/lsm_config.json`):

```json
{
  "baseDir": "data",
  "flushThreshold": 100,
  "maxLevels": 5,
  "bloomFilterSize": 1024
}
```

**Configuration Options:**
- **baseDir**: Directory for data storage (SSTables and WAL)
- **flushThreshold**: Number of entries before MemTable flushes to disk
- **maxLevels**: Depth of SSTable compaction hierarchy
- **bloomFilterSize**: Size of bloom filter bit array (affects false positive rate)

**Using Custom Config:**
```bash
# Use default config
./lsm_cli

# Use custom config
./lsm_cli /path/to/custom_config.json
```

## How It Works

### Write Path
1. Write operation is logged to WAL (durability)
2. Key-value pair is inserted into MemTable (in-memory)
3. When MemTable reaches threshold, flush to SSTable (Level 0)
4. Bloom filter is created for the new SSTable
5. Auto-compaction triggers if level thresholds are exceeded

### Read Path
1. Check MemTable first (most recent data)
2. If not found, search SSTables from newest to oldest
3. Use bloom filters to skip SSTables that definitely don't contain the key
4. Return value or empty string if not found

### Delete Path
1. Delete is logged to WAL
2. Tombstone marker is inserted into MemTable
3. Physical deletion happens during compaction

### Crash Recovery
1. On startup, WAL is replayed
2. MemTable is reconstructed from WAL entries
3. Normal operations resume

## Project Structure

```
lsmtree/
├── src/
│   ├── engine/
│   │   ├── lsm_engine.h/cpp      # Main engine
│   │   ├── memtable.h/cpp        # In-memory table
│   │   ├── sstable.h/cpp         # Persistent table
│   │   ├── sstableiterator.h/cpp # SSTable iterator
│   │   └── wal_writer.h/cpp      # Write-ahead log
│   ├── utils/
│   │   └── bloom_filter.h        # Bloom filter
│   ├── benchmark/
│   │   └── benchmark.cpp         # YCSB-style benchmarks
│   ├── config/
│   │   └── lsm_config.h          # Configuration loader
│   └── cli/
│       └── main.cpp              # CLI application
├── data/                         # Runtime data (created on first run)
│   ├── wal.log                  # Write-ahead log
│   └── ssTables/                # SSTable hierarchy
│       ├── 0/                   # Level 0 (newest)
│       ├── 1/                   # Level 1
│       └── ...
├── tests/                        # Comprehensive test suite
│   ├── memtable_test.cpp        # MemTable unit tests
│   ├── bloom_filter_test.cpp    # Bloom filter tests
│   ├── wal_test.cpp             # WAL tests
│   ├── integration_test.cpp     # End-to-end tests
│   ├── range_query_test.cpp     # Range query tests
│   └── config_test.cpp          # Configuration tests
├── resources/
│   └── lsm_config.json          # Default configuration
├── CMakeLists.txt               # Build configuration
├── README.md                    # This file
└── Architecture.md              # Detailed architecture docs
```

## Performance Benchmarks

The engine includes a comprehensive YCSB-style benchmarking suite with 4 standard workload types.

### Running Benchmarks

```bash
# Build benchmark executable
cd build
cmake ..
make lsm_benchmark

# Run all workloads
./lsm_benchmark

# Run specific workload with custom parameters
./lsm_benchmark -w A -n 10000 -v 100
```

### Benchmark Results

**Test Configuration:**
- Operations: 10,000 per workload
- Value Size: 100 bytes
- Flush Threshold: 100 entries
- Platform: Apple Silicon (M-series)

| Workload | Description | Throughput | p50 Latency | p99 Latency |
|----------|-------------|------------|-------------|-------------|
| **Workload A** | Read-Heavy (50% read, 50% update) | ~34K ops/sec | 0.024 ms | 0.036 ms |
| **Workload B** | Read-Mostly (95% read, 5% update) | ~41K ops/sec | 0.021 ms | 0.032 ms |
| **Workload C** | Write-Heavy (100% insert) | ~28K ops/sec | 0.025 ms | 0.912 ms |
| **Workload D** | Read-Latest (95% read, 5% insert) | ~48K ops/sec | 0.021 ms | 0.029 ms |

**Key Observations:**
- Read-latest workload performs best (~48K ops/sec) due to MemTable cache hits
- Sub-millisecond p99 latencies for read-heavy workloads
- Write-heavy workload shows higher p99 due to flush and compaction overhead
- Bloom filters effectively reduce unnecessary disk I/O by ~90%
- Block-based format with sparse indexing enables efficient binary search

### Complexity Analysis

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| PUT | O(log n) | MemTable uses std::map |
| GET | O(log n + k·m) | May need to check multiple SSTables |
| DELETE | O(log n) | Writes tombstone marker |
| Flush | O(n log n) | Serializing sorted MemTable |
| Compaction | O(k·n log n) | Merging k files with n entries each |

**Space Complexity**: O(total keys) with write amplification during compaction

## Testing

Comprehensive test suite using Google Test framework with 50+ tests.

**Test Coverage:**
- MemTable operations (10 tests)
- Bloom filter functionality (9 tests)
- Write-Ahead Log (8 tests)
- Integration tests (10 tests)
- Range queries (11 tests)
- Configuration loading (7 tests)

```bash
# Build and run all tests
cd build
cmake ..
make
ctest --verbose

# Run specific test suite
./memtable_test
./range_query_test
./config_test
```

## Design Decisions & Trade-offs

This is an educational project with intentional scope boundaries:

**What's Included:**
- ✅ Block-based binary format (industry standard)
- ✅ Size-tiered compaction (simpler than leveled)
- ✅ Write-ahead logging with truncation
- ✅ Comprehensive benchmarking suite
- ✅ Range query support
- ✅ Bloom filter optimization

**Intentionally Excluded (for scope management):**
- ❌ Concurrent access (single-threaded by design)
- ❌ Compression (adds complexity without much learning value)
- ❌ Leveled compaction (size-tiered is sufficient)
- ❌ MVCC/snapshots (very complex, diminishing returns)
- ❌ Block cache (LRU) - future enhancement
- ❌ Complex type system (string keys/values only)

## Future Enhancements

Potential improvements:

- [x] Range query support (`scan(startKey, endKey)`)
- [x] Configuration file support (JSON)
- [x] Comprehensive test coverage (55 tests)
- [x] YCSB-style benchmarking suite with 4 workload types
- [x] Block-based SSTable format (4KB blocks with indexing)
- [ ] Thread-safe concurrent operations
- [ ] Leveled compaction strategy
- [ ] Compression (Snappy, LZ4)
- [ ] MemTable as SkipList
- [ ] Block cache (LRU) for hot data

## Learning Resources

**Recommended Reading:**
- "The Log-Structured Merge-Tree (LSM-Tree)" - O'Neil et al. (1996)
- [LevelDB Documentation](https://github.com/google/leveldb/blob/main/doc/index.md)
- [RocksDB Wiki](https://github.com/facebook/rocksdb/wiki)

**Related Projects:**
- [LevelDB](https://github.com/google/leveldb) - Google's LSM implementation
- [RocksDB](https://github.com/facebook/rocksdb) - Facebook's high-performance fork
- [Apache Cassandra](https://cassandra.apache.org/) - Distributed database using LSM Trees

## Key Achievements

**Technical Highlights:**
- Implemented core LSM Tree architecture from scratch (MemTable, WAL, SSTables, Compaction)
- Built 4KB block-based binary format with sparse indexing, similar to LevelDB/RocksDB
- Achieved ~48K ops/sec read throughput with sub-millisecond p99 latencies
- Created professional benchmarking suite with YCSB-style workloads
- Comprehensive test coverage (55 tests) with crash recovery validation
- Bloom filters reduce unnecessary disk I/O by ~90%

## License

This is an educational project. Feel free to use and modify as needed.

## Author

Devesh Singh - Backend Software Engineer

## Acknowledgments

Inspired by the design of LevelDB, RocksDB, and the original LSM Tree paper.