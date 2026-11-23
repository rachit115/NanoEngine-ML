#include "index/b_plus_tree.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

int main() {
    std::cout << "[TEST] Running Concurrent B+ Tree Indexing Tests...\n";
    nanoengine::ConcurrentBPlusTree btree;

    // 1. Single-threaded insert & split test
    const int N = 100;
    for (int i = 0; i < N; ++i) {
        bool ok = btree.Insert(i, "val_" + std::to_string(i));
        assert(ok);
    }
    assert(btree.GetNumKeys() == N);

    // Verify all keys are present
    for (int i = 0; i < N; ++i) {
        std::string val;
        bool found = btree.GetValue(i, &val);
        assert(found);
        assert(val == "val_" + std::to_string(i));
    }
    std::cout << "  ✓ B+ Tree Insertion, Splitting, and Point Lookups: PASSED\n";

    // 2. Range scan test
    auto range = btree.ScanRange(20, 25);
    assert(range.size() == 6);
    assert(range[0].first == 20 && range[5].first == 25);
    std::cout << "  ✓ B+ Tree Leaf-Linked Range Scan: PASSED\n";

    // 3. Multi-threaded Concurrent Insertions (Stress Test with Latches)
    std::cout << "  --> Running 4 concurrent threads inserting 1000 keys...\n";
    std::vector<std::thread> threads;
    const int THREADS = 4;
    const int KEYS_PER_THREAD = 250;

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&btree, t]() {
            for (int k = 0; k < KEYS_PER_THREAD; ++k) {
                int64_t key = 1000 + (t * KEYS_PER_THREAD) + k;
                btree.Insert(key, "concurrent_" + std::to_string(key));
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Verify concurrent insertions succeeded without data races
    for (int t = 0; t < THREADS; ++t) {
        for (int k = 0; k < KEYS_PER_THREAD; ++k) {
            int64_t key = 1000 + (t * KEYS_PER_THREAD) + k;
            std::string res;
            bool found = btree.GetValue(key, &res);
            assert(found);
            assert(res == "concurrent_" + std::to_string(key));
        }
    }
    std::cout << "  ✓ Multi-Threaded Latch Crabbing Synchronization: PASSED\n";

    std::cout << "[PASS] All B+ Tree Concurrency Tests Passed Successfully!\n";
    return 0;
}
