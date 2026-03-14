#include "../engine/lsm_engine.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <algorithm>
#include <iomanip>
#include <filesystem>

// Benchmark configuration
struct BenchmarkConfig {
    int numOperations = 10000;
    int valueSize = 100;  // bytes
    int flushThreshold = 100;
    int maxLevels = 5;
    std::string workload = "all";  // all, A, B, C, D
};

// Metrics collection
struct BenchmarkMetrics {
    std::vector<double> latencies_us;  // microseconds
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;
    int totalOps = 0;

    void recordLatency(double latency_us) {
        latencies_us.push_back(latency_us);
    }

    void start() {
        startTime = std::chrono::high_resolution_clock::now();
    }

    void end() {
        endTime = std::chrono::high_resolution_clock::now();
        totalOps = latencies_us.size();
    }

    double getDurationSeconds() const {
        return std::chrono::duration<double>(endTime - startTime).count();
    }

    double getThroughput() const {
        double duration = getDurationSeconds();
        return duration > 0 ? totalOps / duration : 0;
    }

    double getPercentile(double percentile) const {
        if (latencies_us.empty()) return 0.0;

        std::vector<double> sorted = latencies_us;
        std::sort(sorted.begin(), sorted.end());

        size_t index = static_cast<size_t>(sorted.size() * percentile / 100.0);
        if (index >= sorted.size()) index = sorted.size() - 1;

        return sorted[index];
    }

    double getMin() const {
        return latencies_us.empty() ? 0.0 : *std::min_element(latencies_us.begin(), latencies_us.end());
    }

    double getMax() const {
        return latencies_us.empty() ? 0.0 : *std::max_element(latencies_us.begin(), latencies_us.end());
    }
};

// Utility functions
std::string generateKey(int id) {
    std::ostringstream oss;
    oss << "user" << std::setfill('0') << std::setw(7) << id;
    return oss.str();
}

std::string generateValue(int size, std::mt19937& gen) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    std::string value;
    value.reserve(size);
    for (int i = 0; i < size; ++i) {
        value += charset[dist(gen)];
    }
    return value;
}

void printBenchmarkHeader(const std::string& workloadName, const std::string& description) {
    std::cout << "\n";
    std::cout << "======================================\n";
    std::cout << "Benchmark: " << workloadName << "\n";
    std::cout << description << "\n";
    std::cout << "======================================\n";
}

void printMetrics(const BenchmarkMetrics& metrics) {
    std::cout << "\n";
    std::cout << "Operations: " << metrics.totalOps << "\n";
    std::cout << "Duration: " << std::fixed << std::setprecision(2) << metrics.getDurationSeconds() << "s\n";
    std::cout << "Throughput: " << std::fixed << std::setprecision(0) << metrics.getThroughput() << " ops/sec\n";

    std::cout << "\nLatency Distribution:\n";
    std::cout << "  p50:  " << std::fixed << std::setprecision(3) << metrics.getPercentile(50) / 1000.0 << " ms\n";
    std::cout << "  p95:  " << std::fixed << std::setprecision(3) << metrics.getPercentile(95) / 1000.0 << " ms\n";
    std::cout << "  p99:  " << std::fixed << std::setprecision(3) << metrics.getPercentile(99) / 1000.0 << " ms\n";
    std::cout << "  p999: " << std::fixed << std::setprecision(3) << metrics.getPercentile(99.9) / 1000.0 << " ms\n";
    std::cout << "  min:  " << std::fixed << std::setprecision(3) << metrics.getMin() / 1000.0 << " ms\n";
    std::cout << "  max:  " << std::fixed << std::setprecision(3) << metrics.getMax() / 1000.0 << " ms\n";
}

// Workload A: Read-Heavy (50% read, 50% update)
void runWorkloadA(const BenchmarkConfig& config) {
    printBenchmarkHeader("Workload A", "Read-Heavy (50% read, 50% update)");

    std::string benchDir = "/tmp/lsm_bench_a";
    if (std::filesystem::exists(benchDir)) {
        std::filesystem::remove_all(benchDir);
    }
    std::filesystem::create_directories(benchDir);

    LSMEngine engine(benchDir, config.flushThreshold, config.maxLevels);
    BenchmarkMetrics metrics;
    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<> opDist(0, 1);  // 0 = read, 1 = update

    // Pre-populate with initial data
    std::cout << "Pre-populating with " << config.numOperations / 2 << " keys...\n";
    for (int i = 0; i < config.numOperations / 2; ++i) {
        std::string key = generateKey(i);
        std::string value = generateValue(config.valueSize, gen);
        engine.put(key, value);
    }

    std::cout << "Running benchmark...\n";
    metrics.start();

    std::uniform_int_distribution<> keyDist(0, config.numOperations / 2 - 1);

    for (int i = 0; i < config.numOperations; ++i) {
        auto opStart = std::chrono::high_resolution_clock::now();

        int op = opDist(gen);
        int keyId = keyDist(gen);
        std::string key = generateKey(keyId);

        if (op == 0) {
            // Read
            engine.get(key);
        } else {
            // Update
            std::string value = generateValue(config.valueSize, gen);
            engine.put(key, value);
        }

        auto opEnd = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration<double, std::micro>(opEnd - opStart).count();
        metrics.recordLatency(latency_us);
    }

    metrics.end();
    printMetrics(metrics);

    std::filesystem::remove_all(benchDir);
}

// Workload B: Read-Mostly (95% read, 5% update)
void runWorkloadB(const BenchmarkConfig& config) {
    printBenchmarkHeader("Workload B", "Read-Mostly (95% read, 5% update)");

    std::string benchDir = "/tmp/lsm_bench_b";
    if (std::filesystem::exists(benchDir)) {
        std::filesystem::remove_all(benchDir);
    }
    std::filesystem::create_directories(benchDir);

    LSMEngine engine(benchDir, config.flushThreshold, config.maxLevels);
    BenchmarkMetrics metrics;
    std::mt19937 gen(42);
    std::uniform_int_distribution<> opDist(0, 99);  // 0-94 = read, 95-99 = update

    // Pre-populate
    std::cout << "Pre-populating with " << config.numOperations / 2 << " keys...\n";
    for (int i = 0; i < config.numOperations / 2; ++i) {
        std::string key = generateKey(i);
        std::string value = generateValue(config.valueSize, gen);
        engine.put(key, value);
    }

    std::cout << "Running benchmark...\n";
    metrics.start();

    std::uniform_int_distribution<> keyDist(0, config.numOperations / 2 - 1);

    for (int i = 0; i < config.numOperations; ++i) {
        auto opStart = std::chrono::high_resolution_clock::now();

        int op = opDist(gen);
        int keyId = keyDist(gen);
        std::string key = generateKey(keyId);

        if (op < 95) {
            // Read (95%)
            engine.get(key);
        } else {
            // Update (5%)
            std::string value = generateValue(config.valueSize, gen);
            engine.put(key, value);
        }

        auto opEnd = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration<double, std::micro>(opEnd - opStart).count();
        metrics.recordLatency(latency_us);
    }

    metrics.end();
    printMetrics(metrics);

    std::filesystem::remove_all(benchDir);
}

// Workload C: Write-Heavy (100% insert)
void runWorkloadC(const BenchmarkConfig& config) {
    printBenchmarkHeader("Workload C", "Write-Heavy (100% insert)");

    std::string benchDir = "/tmp/lsm_bench_c";
    if (std::filesystem::exists(benchDir)) {
        std::filesystem::remove_all(benchDir);
    }
    std::filesystem::create_directories(benchDir);

    LSMEngine engine(benchDir, config.flushThreshold, config.maxLevels);
    BenchmarkMetrics metrics;
    std::mt19937 gen(42);

    std::cout << "Running benchmark...\n";
    metrics.start();

    for (int i = 0; i < config.numOperations; ++i) {
        auto opStart = std::chrono::high_resolution_clock::now();

        std::string key = generateKey(i);
        std::string value = generateValue(config.valueSize, gen);
        engine.put(key, value);

        auto opEnd = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration<double, std::micro>(opEnd - opStart).count();
        metrics.recordLatency(latency_us);
    }

    metrics.end();
    printMetrics(metrics);

    std::filesystem::remove_all(benchDir);
}

// Workload D: Read-Latest (95% read recent, 5% insert)
void runWorkloadD(const BenchmarkConfig& config) {
    printBenchmarkHeader("Workload D", "Read-Latest (95% read recent, 5% insert)");

    std::string benchDir = "/tmp/lsm_bench_d";
    if (std::filesystem::exists(benchDir)) {
        std::filesystem::remove_all(benchDir);
    }
    std::filesystem::create_directories(benchDir);

    LSMEngine engine(benchDir, config.flushThreshold, config.maxLevels);
    BenchmarkMetrics metrics;
    std::mt19937 gen(42);
    std::uniform_int_distribution<> opDist(0, 99);

    // Pre-populate
    std::cout << "Pre-populating with " << config.numOperations / 2 << " keys...\n";
    for (int i = 0; i < config.numOperations / 2; ++i) {
        std::string key = generateKey(i);
        std::string value = generateValue(config.valueSize, gen);
        engine.put(key, value);
    }

    std::cout << "Running benchmark...\n";
    metrics.start();

    int currentMaxKey = config.numOperations / 2;

    for (int i = 0; i < config.numOperations; ++i) {
        auto opStart = std::chrono::high_resolution_clock::now();

        int op = opDist(gen);

        if (op < 95) {
            // Read recent keys (95%)
            // Favor keys from the last 20% of inserted keys
            int recentWindow = std::max(100, currentMaxKey / 5);
            std::uniform_int_distribution<> recentDist(std::max(0, currentMaxKey - recentWindow), currentMaxKey - 1);
            int keyId = recentDist(gen);
            std::string key = generateKey(keyId);
            engine.get(key);
        } else {
            // Insert new key (5%)
            std::string key = generateKey(currentMaxKey++);
            std::string value = generateValue(config.valueSize, gen);
            engine.put(key, value);
        }

        auto opEnd = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration<double, std::micro>(opEnd - opStart).count();
        metrics.recordLatency(latency_us);
    }

    metrics.end();
    printMetrics(metrics);

    std::filesystem::remove_all(benchDir);
}

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -w <workload>    Workload to run (A, B, C, D, or all) [default: all]\n";
    std::cout << "  -n <number>      Number of operations [default: 10000]\n";
    std::cout << "  -v <size>        Value size in bytes [default: 100]\n";
    std::cout << "  -h               Show this help message\n";
    std::cout << "\nWorkloads:\n";
    std::cout << "  A: Read-Heavy (50% read, 50% update)\n";
    std::cout << "  B: Read-Mostly (95% read, 5% update)\n";
    std::cout << "  C: Write-Heavy (100% insert)\n";
    std::cout << "  D: Read-Latest (95% read recent, 5% insert)\n";
}

int main(int argc, char* argv[]) {
    BenchmarkConfig config;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-w" && i + 1 < argc) {
            config.workload = argv[++i];
        } else if (arg == "-n" && i + 1 < argc) {
            config.numOperations = std::stoi(argv[++i]);
        } else if (arg == "-v" && i + 1 < argc) {
            config.valueSize = std::stoi(argv[++i]);
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "===================================\n";
    std::cout << "    LSM Tree Benchmark Suite\n";
    std::cout << "===================================\n";
    std::cout << "Configuration:\n";
    std::cout << "  Operations: " << config.numOperations << "\n";
    std::cout << "  Value Size: " << config.valueSize << " bytes\n";
    std::cout << "  Flush Threshold: " << config.flushThreshold << " entries\n";
    std::cout << "  Max Levels: " << config.maxLevels << "\n";

    // Run selected workloads
    if (config.workload == "all" || config.workload == "A") {
        runWorkloadA(config);
    }

    if (config.workload == "all" || config.workload == "B") {
        runWorkloadB(config);
    }

    if (config.workload == "all" || config.workload == "C") {
        runWorkloadC(config);
    }

    if (config.workload == "all" || config.workload == "D") {
        runWorkloadD(config);
    }

    std::cout << "\n===================================\n";
    std::cout << "   Benchmark Complete!\n";
    std::cout << "===================================\n";

    return 0;
}
