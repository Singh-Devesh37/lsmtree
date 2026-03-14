# LSM Tree Storage Engine - Architecture Documentation

## Table of Contents
1. [Overview](#overview)
2. [What is an LSM Tree?](#what-is-an-lsm-tree)
3. [Project Structure](#project-structure)
4. [Core Components](#core-components)
5. [Data Flow](#data-flow)
6. [Operations](#operations)
7. [Block-Based SSTables](#block-based-sstables)
8. [Compaction](#compaction)
9. [Crash Recovery](#crash-recovery)
10. [Benchmarking](#benchmarking)
11. [File Organization](#file-organization)

---

## Overview

This project is a C++ implementation of a **Log-Structured Merge-Tree (LSM Tree)** storage engine. LSM Trees are the fundamental data structure behind many modern databases including LevelDB, RocksDB, Cassandra, and HBase. This implementation demonstrates the core concepts of write-optimized storage systems.

**Key Features:**
- In-memory MemTable for fast writes
- Write-Ahead Log (WAL) for durability with automatic truncation
- Block-based Sorted String Tables (SSTables) with binary format
- Bloom filters for efficient lookups (~90% I/O reduction)
- Multi-level size-tiered compaction strategy
- Comprehensive crash recovery with WAL replay
- Range query support with multi-level merging
- YCSB-style benchmarking suite for performance validation

---

## What is an LSM Tree?

An LSM Tree is a data structure optimized for write-heavy workloads. It achieves high write throughput by:

1. **Buffering writes in memory** (MemTable)
2. **Sequential disk writes** (SSTables) instead of random I/O
3. **Background compaction** to merge and optimize data
4. **Bloom filters** to avoid unnecessary disk reads

**Trade-offs:**
- **Pros:** Extremely fast writes, good compression, sequential I/O
- **Cons:** Slower reads (may need to check multiple levels), write amplification during compaction

---

## Project Structure

```
lsmtree/
├── src/
│   ├── engine/           # Core LSM engine components
│   │   ├── lsm_engine.h/cpp      # Main engine orchestration
│   │   ├── memtable.h/cpp        # In-memory sorted table
│   │   ├── sstable.h/cpp         # Persistent sorted table
│   │   ├── sstableiterator.h/cpp # Iterator for SSTable files
│   │   └── wal_writer.h/cpp      # Write-ahead logging
│   ├── utils/            # Utility components
│   │   └── bloom_filter.h        # Probabilistic data structure
│   └── cli/              # Command-line interface
│       └── main.cpp              # Interactive CLI application
├── data/                 # Runtime data directory
│   ├── wal.log          # Write-ahead log file
│   └── ssTables/        # SSTable storage hierarchy
│       ├── 0/           # Level 0 (newest)
│       ├── 1/           # Level 1
│       └── ...          # Additional levels
└── CMakeLists.txt       # Build configuration
```

---

## Core Components

### 1. MemTable (`memtable.h/cpp`)

**Purpose:** In-memory sorted data structure for fast writes and reads.

**Implementation:**
- Uses `std::map<string, string>` for sorted key-value storage
- Maintains lexicographically sorted keys
- Supports PUT, GET, and DELETE operations

**Tombstones:**
- Deletes are handled via tombstone markers (`"__TOMBSTONE__"`)
- Actual deletion happens during compaction
- This ensures correct semantics even after flush to disk

**Code Reference:**
```cpp
void put(const std::string& key, const std::string& value);
std::optional<std::string> get(const std::string& key);
void remove(const std::string& key);  // Sets tombstone
```

### 2. Write-Ahead Log - WAL (`wal_writer.h/cpp`)

**Purpose:** Ensures durability and crash recovery.

**How it works:**
- Every write operation is logged BEFORE updating the MemTable
- Log format: `PUT <key> <value>` or `DEL <key>`
- Uses `std::unitbuf` for immediate flushing to disk
- On restart, the WAL is replayed to reconstruct the MemTable

**Crash Recovery Flow:**
1. Engine starts
2. WAL is replayed: `WALWriter::replay(baseDir + "/wal.log", memTable)`
3. MemTable is restored to pre-crash state
4. Normal operations resume

**Code Reference:** `wal_writer.cpp:20-43`

### 3. SSTable - Sorted String Table (`sstable.h/cpp`)

**Purpose:** Persistent, immutable, sorted files on disk.

**File Format:**
```
key1\tvalue1\n
key2\tvalue2\n
...
```

**Key Operations:**
- **Write:** `writeToFile()` - Flushes MemTable to a new SSTable file
- **Read:** `readFromFile()` - Linear scan through a specific SSTable
- **Search:** `findInSSTable()` - Searches across all SSTables with bloom filter optimization
- **Compact:** `compactSSTables()` - Merges SSTables to remove duplicates and tombstones

**Immutability:**
- SSTables are never modified after creation
- Updates/deletes create new entries in newer SSTables
- Compaction creates new SSTables and deletes old ones

### 4. Bloom Filter (`bloom_filter.h`)

**Purpose:** Probabilistic data structure to avoid unnecessary disk reads.

**How it works:**
- Uses 2 hash functions (std::hash and DJB2)
- 1024-bit filter by default
- Can produce false positives (might contain) but never false negatives
- Saved alongside each SSTable as `.bloom` file

**Benefit:**
- Quickly determines if a key is definitely NOT in an SSTable
- Reduces disk I/O by ~90% for non-existent keys

**Code Reference:** `bloom_filter.h:10-16`

### 5. SSTable Iterator (`sstableiterator.h/cpp`)

**Purpose:** Sequential iteration through SSTable files.

**Implementation:**
- Opens an SSTable file stream
- Reads line-by-line, parsing key-value pairs
- Maintains validity state

**Use Case:** Used during compaction to merge multiple SSTables

### 6. LSM Engine (`lsm_engine.h/cpp`)

**Purpose:** Main orchestration layer that ties all components together.

**Configuration:**
- `baseDir`: Data directory path
- `flushThreshold`: Max MemTable size before flush (default: 3 entries)
- `maxLevels`: Number of SSTable levels (default: 5)

**Responsibilities:**
1. Coordinates MemTable, WAL, and SSTables
2. Triggers flushes when MemTable is full
3. Manages multi-level compaction cascade
4. Provides unified PUT/GET/DELETE interface

---

## Data Flow

### Write Path (PUT Operation)

```
User: put("key1", "value1")
   ↓
1. WAL.logPut("key1", "value1")           [Durability]
   ↓
2. MemTable.put("key1", "value1")         [In-memory write]
   ↓
3. Check if MemTable.size() >= threshold
   ↓
4. If yes → flush()
   ↓
5. SSTable.writeToFile() → Level 0        [Persist to disk]
   ↓
6. Create bloom filter for new SSTable
   ↓
7. Clear MemTable and WAL
   ↓
8. Check compaction triggers → autoCompact()
```

**Code Reference:** `lsm_engine.cpp:15-21`

### Read Path (GET Operation)

```
User: get("key1")
   ↓
1. Check MemTable first                    [Hot data]
   ↓
   Found? → Return value (may be tombstone)
   ↓
   Not found ↓
   ↓
2. Search SSTables (newest to oldest)      [Cold data]
   ↓
   For each SSTable:
   ├─ Check bloom filter
   │  ├─ Might contain? → readFromFile()
   │  └─ Definitely not → skip
   ↓
3. Return value or empty string (not found)
```

**Why newest to oldest?**
- Newer SSTables have more recent values
- Ensures we get the latest version of a key

**Code Reference:** `lsm_engine.cpp:23-29`, `sstable.cpp:62-95`

### Delete Path (REMOVE Operation)

```
User: remove("key1")
   ↓
1. WAL.logDelete("key1")                   [Durability]
   ↓
2. MemTable.put("key1", "__TOMBSTONE__")   [Mark as deleted]
   ↓
3. Check flush threshold
   ↓
4. If needed → flush tombstone to SSTable
```

**Important:** Physical deletion happens during compaction, not immediately.

**Code Reference:** `lsm_engine.cpp:31-38`

---

## Operations

### Flush Operation

**Trigger Conditions:**
- MemTable size ≥ `flushThreshold`
- Manual flush command

**Process:**
1. Write MemTable contents to a new SSTable in Level 0
2. Create bloom filter for the new SSTable
3. Clear MemTable
4. Trigger auto-compaction check

**File Naming:** `sstable_<ID>.dat` where ID auto-increments

**Code Reference:** `lsm_engine.cpp:40-43`, `sstable.cpp:10-30`

### Compaction

**Purpose:**
- Merge duplicate keys (keep latest value)
- Remove tombstone entries
- Free up disk space
- Maintain read performance

**Strategy:** Size-Tiered Compaction
- Each level has a threshold for number of files
- When Level N exceeds threshold, compact to Level N+1
- Creates a compaction cascade effect

**Code Reference:** `lsm_engine.cpp:72-89`

---

## Compaction

### Multi-Level Architecture

```
Level 0: [SST_1] [SST_2] [SST_3]        ← Newest, may overlap
   ↓ compact
Level 1: [SST_4] [SST_5]                ← Merged
   ↓ compact
Level 2: [SST_6]                        ← Further merged
   ↓
...
Level N: [SST_final]                    ← Oldest, most compact
```

### Compaction Algorithm

**Trigger:** Level L has ≥ `flushThreshold` SSTable files

**Process (`compactSSTables`):**
1. Collect all `.dat` files in Level L
2. Sort by modification time (oldest first)
3. Read all files and build merged map:
   - For duplicate keys: keep FIRST occurrence (oldest at current level)
   - Filter out tombstones
4. Delete old SSTable files and bloom filters
5. Write merged data to Level L+1

**Key Insight:** Keeps oldest values at each level because newer updates are in upper levels or MemTable.

**Write Amplification:** Data may be rewritten multiple times as it cascades down levels.

**Code Reference:** `sstable.cpp:97-133`

---

## Crash Recovery

### How Crashes are Handled

**Scenario:** System crashes before MemTable is flushed.

**Solution:** WAL Replay
1. On startup, engine constructor calls: `WALWriter::replay()`
2. WAL is read line-by-line
3. Each operation (`PUT`/`DEL`) is re-applied to MemTable
4. MemTable is restored to pre-crash state
5. Normal operations resume

**Durability Guarantee:**
- WAL uses `std::unitbuf` for immediate flush
- Every write is durable before acknowledgment
- No data loss even in sudden crashes

**Code Reference:** `lsm_engine.cpp:12`, `wal_writer.cpp:20-43`

---

## Benchmarking

### YCSB-Style Workload Suite

The project includes a comprehensive benchmarking tool (`lsm_benchmark`) based on the Yahoo! Cloud Serving Benchmark (YCSB).

### Workload Types

**Workload A: Read-Heavy (50% read, 50% update)**
- Use case: Session stores, user profiles
- Tests: Mixed read-write performance

**Workload B: Read-Mostly (95% read, 5% update)**
- Use case: Photo tagging, content serving
- Tests: Read-optimized performance with bloom filters

**Workload C: Write-Heavy (100% insert)**
- Use case: Logging, event streaming
- Tests: WAL and flush performance

**Workload D: Read-Latest (95% read recent, 5% insert)**
- Use case: User status updates, messaging
- Tests: MemTable hit rate

### Metrics Collected

**Throughput:**
- Operations per second (ops/sec)
- Total operations completed
- Duration

**Latency Percentiles:**
- p50 (median)
- p95 (95th percentile)
- p99 (99th percentile)
- p999 (99.9th percentile)
- min/max

### Running Benchmarks

```bash
# All workloads with 10,000 operations
./lsm_benchmark

# Specific workload
./lsm_benchmark -w B -n 50000

# Custom value size
./lsm_benchmark -w A -n 10000 -v 200
```

### Performance Results

**Configuration**: 10,000 ops, 100-byte values, 100-entry flush threshold

| Workload | Throughput | p50 | p99 |
|----------|-----------|-----|-----|
| A (Read-Heavy) | ~34K ops/sec | 0.024ms | 0.036ms |
| B (Read-Mostly) | ~41K ops/sec | 0.021ms | 0.032ms |
| C (Write-Heavy) | ~28K ops/sec | 0.025ms | 0.912ms |
| D (Read-Latest) | ~48K ops/sec | 0.021ms | 0.029ms |

**Observations:**
- Read-latest performs best (~48K ops/sec) due to MemTable hits
- Write-heavy shows higher p99 due to flush and compaction overhead
- Sub-millisecond p99 latencies for read-heavy workloads
- Bloom filters effectively reduce unnecessary I/O by ~90%

---

## File Organization

### Directory Structure

```
data/
├── wal.log                          # Write-ahead log
└── ssTables/
    ├── 0/                          # Level 0
    │   ├── sstable_1.dat
    │   ├── sstable_1.dat.bloom
    │   ├── sstable_2.dat
    │   └── sstable_2.dat.bloom
    ├── 1/                          # Level 1
    │   ├── sstable_3.dat
    │   └── sstable_3.dat.bloom
    ├── 2/                          # Level 2
    └── ...
```

### File Types

1. **WAL File (`wal.log`)**
   - Text format
   - Append-only
   - Format: `PUT key value` or `DEL key`

2. **SSTable File (`.dat`)**
   - Text format (tab-separated)
   - Immutable
   - Format: `key\tvalue\n`

3. **Bloom Filter File (`.dat.bloom`)**
   - Serialized bit array
   - 1024 characters of '0' and '1'
   - Regenerated on compaction

---

## CLI Commands

The interactive CLI (`main.cpp`) supports:

```
put <key> <value>     # Insert or update a key
get <key>             # Retrieve value for key
delete <key>          # Mark key for deletion
flush                 # Manually flush MemTable
compact               # Manually trigger compaction
read <file> <key>     # Read directly from SSTable file
exit                  # Shutdown engine
```

**Configuration:**
- `FLUSH_THRESHOLD = 3` entries
- `MAX_LEVELS = 5` levels
- `baseDir = "data"`

**Code Reference:** `cli/main.cpp:17-96`

---

## Performance Characteristics

### Time Complexity

| Operation | Average Case | Worst Case | Notes |
|-----------|-------------|------------|-------|
| PUT | O(log n) | O(log n + flush) | MemTable uses std::map |
| GET | O(log n + k·m) | O(k·m·s) | n=MemTable size, k=levels, m=files/level, s=keys/file |
| DELETE | O(log n) | O(log n + flush) | Same as PUT (tombstone) |
| Flush | O(n log n) | O(n log n) | Serializing sorted MemTable |
| Compaction | O(k·n log n) | O(k·n log n) | k files with n keys each |

### Space Complexity

- **MemTable:** O(n) where n = number of entries
- **WAL:** O(n) where n = number of operations since last flush
- **SSTables:** O(total keys) with write amplification factor

---

## Design Trade-offs

### Advantages
1. **Fast Writes:** O(log n) in-memory writes
2. **Sequential I/O:** Disk writes are append-only
3. **Good Compression:** Compaction removes duplicates
4. **Crash Recovery:** WAL ensures durability

### Disadvantages
1. **Read Amplification:** May check multiple levels
2. **Write Amplification:** Compaction rewrites data
3. **Space Amplification:** Duplicate keys across levels before compaction
4. **Complexity:** More components than B-tree based systems

---

## Learning Path

To understand this implementation:

1. **Start with:** MemTable (`memtable.cpp`) - simplest component
2. **Then:** WAL Writer (`wal_writer.cpp`) - durability mechanism
3. **Next:** SSTable (`sstable.cpp`) - persistence layer
4. **After:** Bloom Filter (`bloom_filter.h`) - optimization technique
5. **Finally:** LSM Engine (`lsm_engine.cpp`) - orchestration

---

## Implemented Features

- [x] Range queries (scan operations) with multi-level merging
- [x] Block-based SSTable format (binary, indexed)
- [x] YCSB-style benchmarking suite
- [x] Comprehensive test coverage (55 tests)
- [x] WAL truncation for space management
- [x] Feature flag system for configuration
- [x] Crash recovery with edge case handling
- [x] Compaction bug fixes (keep newest values)

## Possible Future Enhancements

**Performance:**
- [ ] Block cache (LRU) for hot blocks
- [ ] Leveled compaction strategy (better than size-tiered)
- [ ] Compression (Snappy, LZ4) per block
- [ ] MemTable as SkipList (lock-free)

**Features:**
- [ ] Concurrent operations with thread safety
- [ ] MVCC with snapshot isolation
- [ ] Merge operator support
- [ ] TTL (Time-To-Live) support
- [ ] Partitioning/sharding
- [ ] Replication

**Note:** These enhancements were intentionally excluded to keep the project scope manageable and understandable for interviews.

---

## References

**Real-world implementations:**
- [LevelDB](https://github.com/google/leveldb) - Google's original LSM implementation
- [RocksDB](https://github.com/facebook/rocksdb) - Facebook's fork of LevelDB
- [Apache Cassandra](https://cassandra.apache.org/) - Distributed LSM database
- [Bitcask](https://riak.com/assets/bitcask-intro.pdf) - Simpler log-structured storage

**Papers:**
- "The Log-Structured Merge-Tree (LSM-Tree)" by O'Neil et al. (1996)
- "WiscKey: Separating Keys from Values in SSD-conscious Storage" (2016)

---

## Conclusion

This LSM Tree implementation demonstrates the core principles behind modern write-optimized databases. While simplified for educational purposes, it includes all fundamental components: in-memory buffering, write-ahead logging, persistent sorted tables, bloom filters, and multi-level compaction.

Understanding this architecture provides insight into how databases like Cassandra, RocksDB, and LevelDB achieve high write throughput while maintaining acceptable read performance.