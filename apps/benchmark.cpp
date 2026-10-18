#include "engine/nano_engine.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <filesystem>

struct BenchmarkResult {
    std::string policy_name;
    double duration_ms;
    uint64_t hits;
    uint64_t misses;
    double hit_ratio;
    uint64_t disk_reads;
    uint64_t disk_writes;
};

BenchmarkResult RunWorkload(nanoengine::ReplacerType type, const std::string& name) {
    std::string db_file = "bench_" + name + ".db";
    std::string log_file = "bench_" + name + ".log";
    if (std::filesystem::exists(db_file)) std::filesystem::remove(db_file);
    if (std::filesystem::exists(log_file)) std::filesystem::remove(log_file);

    nanoengine::EngineStats stats;
    double duration = 0.0;

    {
        const size_t BUFFER_POOL_SIZE = 5; // Constrained memory to force page replacement
        nanoengine::NanoEngine engine(db_file, log_file, BUFFER_POOL_SIZE, type);

        const int NUM_OPERATIONS = 5000;
        const int HOT_KEYS = 8;
        const int COLD_KEYS = 100;

        // Pre-populate data
        auto init_txn = engine.BeginTransaction();
        for (int i = 0; i < COLD_KEYS; ++i) {
            engine.Put(init_txn, i, "record_val_" + std::to_string(i));
        }
        engine.Commit(init_txn);
        engine.ResetMetrics();

        std::mt19937 rng(42);
        std::uniform_int_distribution<int> hot_dist(0, HOT_KEYS - 1);
        std::uniform_int_distribution<int> cold_dist(HOT_KEYS, COLD_KEYS - 1);
        std::uniform_int_distribution<int> skew_dist(1, 100);

        auto start = std::chrono::steady_clock::now();

        for (int op = 0; op < NUM_OPERATIONS; ++op) {
            auto txn = engine.BeginTransaction();

            // Every 200 operations, perform a sequential scan loop (Scan Pollution)
            if (op % 200 == 0) {
                for (int scan_k = 0; scan_k < 30; ++scan_k) {
                    std::string res;
                    engine.Get(txn, scan_k, &res);
                }
            } else {
                // Skewed Zipfian-like access: 80% to hot keys, 20% to cold keys
                int key = (skew_dist(rng) <= 80) ? hot_dist(rng) : cold_dist(rng);
                if (op % 5 == 0) {
                    // 20% writes
                    engine.Put(txn, key, "updated_val_" + std::to_string(op));
                } else {
                    // 80% reads
                    std::string res;
                    engine.Get(txn, key, &res);
                }
            }

            engine.Commit(txn);
        }

        auto end = std::chrono::steady_clock::now();
        duration = std::chrono::duration<double, std::milli>(end - start).count();

        stats = engine.GetStats();
    } // engine destroyed here, files closed

    std::error_code ec;
    std::filesystem::remove(db_file, ec);
    std::filesystem::remove(log_file, ec);

    return BenchmarkResult{
        name, duration, stats.buffer_hits, stats.buffer_misses,
        stats.cache_hit_ratio, stats.disk_reads, stats.disk_writes
    };
}

int main() {
    std::cout << "\033[1;36m";
    std::cout << "========================================================================\n";
    std::cout << "       NANOENGINE-ML: BUFFER POOL REPLACEMENT POLICY BENCHMARK          \n";
    std::cout << "========================================================================\n";
    std::cout << "\033[0m";
    std::cout << "Workload: 5,000 Operations (80/20 Skewed Read/Write + Periodic Scans)\n";
    std::cout << "Memory: Constrained Buffer Pool (5 Frames) with Fixed 4 KB Pages\n\n";

    std::cout << "--> [1/2] Running Traditional LRU Policy Benchmark...\n";
    auto lru_res = RunWorkload(nanoengine::ReplacerType::LRU, "LRU");

    std::cout << "--> [2/2] Running Learned Feature-Weighted Policy (NanoEngine-ML)...\n";
    auto ml_res = RunWorkload(nanoengine::ReplacerType::LEARNED, "Learned-ML");

    std::cout << "\n\033[1;32m========================= BENCHMARK RESULTS =========================\033[0m\n";
    std::cout << std::left << std::setw(15) << "Metric"
              << std::setw(20) << "Standard LRU"
              << std::setw(20) << "NanoEngine-ML"
              << "Advantage\n";
    std::cout << "---------------------------------------------------------------------\n";

    std::cout << std::left << std::setw(15) << "Cache Hit Ratio"
              << std::setw(20) << (std::to_string(lru_res.hit_ratio).substr(0, 5) + "%")
              << std::setw(20) << (std::to_string(ml_res.hit_ratio).substr(0, 5) + "%");
    if (ml_res.hit_ratio > lru_res.hit_ratio) {
        std::cout << "\033[1;32m+" << std::fixed << std::setprecision(1) 
                  << (ml_res.hit_ratio - lru_res.hit_ratio) << "% Higher\033[0m\n";
    } else {
        std::cout << "Comparable\n";
    }

    std::cout << std::left << std::setw(15) << "Buffer Hits"
              << std::setw(20) << lru_res.hits
              << std::setw(20) << ml_res.hits
              << "\n";

    std::cout << std::left << std::setw(15) << "Disk I/O Reads"
              << std::setw(20) << lru_res.disk_reads
              << std::setw(20) << ml_res.disk_reads;
    if (ml_res.disk_reads < lru_res.disk_reads) {
        double red = ((double)(lru_res.disk_reads - ml_res.disk_reads) / lru_res.disk_reads) * 100.0;
        std::cout << "\033[1;32m-" << std::fixed << std::setprecision(1) << red << "% Less Disk Reads\033[0m\n";
    } else {
        std::cout << "\n";
    }

    std::cout << std::left << std::setw(15) << "Execution Time"
              << std::setw(20) << (std::to_string(lru_res.duration_ms).substr(0, 6) + " ms")
              << std::setw(20) << (std::to_string(ml_res.duration_ms).substr(0, 6) + " ms")
              << "\n";
    std::cout << "=====================================================================\n\n";

    return 0;
}
