#include "engine/nano_engine.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

void PrintBanner() {
    std::cout << "\033[1;36m";
    std::cout << "========================================================================\n";
    std::cout << "        NANOENGINE-ML: RELATIONAL STORAGE ENGINE CONSOLE DEMO           \n";
    std::cout << "========================================================================\n";
    std::cout << "\033[0m";
    std::cout << "Available Commands:\n";
    std::cout << "  put <key> <value>    - Execute transactional insert/update with Strict 2PL\n";
    std::cout << "  get <key>            - Query record via B+ Tree index with Shared lock\n";
    std::cout << "  delete <key>         - Delete record and write WAL update\n";
    std::cout << "  scan <min> <max>     - Range scan on B+ Tree\n";
    std::cout << "  frames               - Display real-time Buffer Pool frame table\n";
    std::cout << "  stats                - Display Cache Hit Ratio and Disk I/O metrics\n";
    std::cout << "  policy <lru|learned> - Toggle page eviction policy\n";
    std::cout << "  crash                - Simulate sudden power outage (wipe RAM)\n";
    std::cout << "  recover              - Run ARIES 3-phase Crash Recovery (Analysis/Redo/Undo)\n";
    std::cout << "  help                 - Show this help menu\n";
    std::cout << "  exit                 - Flush database and exit\n\n";
}

void PrintFrameTable(nanoengine::NanoEngine& engine) {
    auto frames = engine.GetFrameTableSnapshot();
    std::cout << "\n+-------+---------+-----------+---------+-----------+\n";
    std::cout << "| Frame | Page ID | Pin Count | Dirty?  | Page LSN  |\n";
    std::cout << "+-------+---------+-----------+---------+-----------+\n";
    for (const auto& f : frames) {
        std::cout << "| " << std::setw(5) << f.frame_id << " | "
                  << std::setw(7) << (f.page_id == nanoengine::INVALID_PAGE_ID ? "EMPTY" : std::to_string(f.page_id)) << " | "
                  << std::setw(9) << f.pin_count << " | "
                  << std::setw(7) << (f.is_dirty ? "\033[1;31mYES\033[0m" : "NO ") << " | "
                  << std::setw(9) << f.lsn << " |\n";
    }
    std::cout << "+-------+---------+-----------+---------+-----------+\n\n";
}

int main() {
    PrintBanner();
    nanoengine::NanoEngine engine("demo_data.db", "demo_wal.log", 6, nanoengine::ReplacerType::LEARNED);

    std::string line;
    while (true) {
        std::cout << "\033[1;32mnanoengine>\033[0m ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if (cmd == "exit" || cmd == "quit") {
            std::cout << "Shutting down engine cleanly...\n";
            break;
        } else if (cmd == "help") {
            PrintBanner();
        } else if (cmd == "put") {
            int64_t key;
            std::string val;
            if (ss >> key >> val) {
                auto txn = engine.BeginTransaction();
                bool ok = engine.Put(txn, key, val);
                if (ok) engine.Commit(txn);
                std::cout << (ok ? "✓ [COMMITTED] " : "✗ [ABORTED] ")
                          << "Key " << key << " => '" << val << "' (Txn #" << txn->GetTxnId() << ")\n";
            } else {
                std::cout << "Usage: put <key> <value>\n";
            }
        } else if (cmd == "get") {
            int64_t key;
            if (ss >> key) {
                auto txn = engine.BeginTransaction();
                std::string val;
                bool ok = engine.Get(txn, key, &val);
                engine.Commit(txn);
                if (ok) {
                    std::cout << "✓ [FOUND] Key " << key << " => '" << val << "'\n";
                } else {
                    std::cout << "✗ Key " << key << " not found in B+ Tree\n";
                }
            } else {
                std::cout << "Usage: get <key>\n";
            }
        } else if (cmd == "delete") {
            int64_t key;
            if (ss >> key) {
                auto txn = engine.BeginTransaction();
                bool ok = engine.Delete(txn, key);
                engine.Commit(txn);
                std::cout << (ok ? "✓ Key deleted\n" : "✗ Key not found\n");
            }
        } else if (cmd == "scan") {
            int64_t min_k, max_k;
            if (ss >> min_k >> max_k) {
                auto txn = engine.BeginTransaction();
                auto res = engine.Scan(txn, min_k, max_k);
                engine.Commit(txn);
                std::cout << "Found " << res.size() << " records in range [" << min_k << ", " << max_k << "]:\n";
                for (const auto& [k, v] : res) {
                    std::cout << "  " << k << " => " << v << "\n";
                }
            } else {
                std::cout << "Usage: scan <min_key> <max_key>\n";
            }
        } else if (cmd == "frames") {
            PrintFrameTable(engine);
        } else if (cmd == "stats") {
            auto stats = engine.GetStats();
            std::cout << "\n--- Engine Performance Metrics ---\n";
            std::cout << "  Buffer Hits:     " << stats.buffer_hits << "\n";
            std::cout << "  Buffer Misses:   " << stats.buffer_misses << "\n";
            std::cout << "  Cache Hit Ratio: " << std::fixed << std::setprecision(2) << stats.cache_hit_ratio << "%\n";
            std::cout << "  Disk Page Reads: " << stats.disk_reads << "\n";
            std::cout << "  Disk Page Writes:" << stats.disk_writes << "\n";
            std::cout << "  Indexed Keys:    " << stats.index_num_keys << "\n\n";
        } else if (cmd == "policy") {
            std::string pol;
            if (ss >> pol) {
                if (pol == "lru") {
                    engine.SetReplacerType(nanoengine::ReplacerType::LRU);
                    std::cout << "Switched to Standard LRU Policy.\n";
                } else if (pol == "learned") {
                    engine.SetReplacerType(nanoengine::ReplacerType::LEARNED);
                    std::cout << "Switched to Learned Feature-Weighted Policy.\n";
                }
            }
        } else if (cmd == "crash") {
            std::cout << "\033[1;31m[CRASH INJECTION]\033[0m Power interrupted! Unflushed in-memory data lost.\n";
            engine.SimulateCrash();
        } else if (cmd == "recover") {
            std::cout << "\033[1;33m[ARIES RECOVERY]\033[0m Initiating 3-Phase Crash Recovery...\n";
            auto stats = engine.Recover();
            std::cout << "✓ Analysis Phase: Discovered " << stats.active_txns_at_crash << " active uncommitted transactions\n";
            std::cout << "✓ Redo Phase:     Replayed " << stats.redo_operations << " committed operations to repeat history\n";
            std::cout << "✓ Undo Phase:     Rolled back " << stats.undo_operations << " loser modifications\n";
            std::cout << "✓ ARIES Recovery Completed in " << stats.recovery_time_ms << " ms. Storage consistent!\n";
        } else {
            std::cout << "Unknown command: '" << cmd << "'. Type 'help' for commands.\n";
        }
    }

    return 0;
}
