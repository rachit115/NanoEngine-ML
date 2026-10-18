#include "engine/nano_engine.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>

int main() {
    std::cout << "[TEST] Running WAL & ARIES Crash Recovery Tests...\n";
    std::string db_file = "test_recovery.db";
    std::string log_file = "test_recovery.log";

    if (std::filesystem::exists(db_file)) std::filesystem::remove(db_file);
    if (std::filesystem::exists(log_file)) std::filesystem::remove(log_file);

    {
        // 1. Start Engine and execute transactions
        nanoengine::NanoEngine engine(db_file, log_file, 5, nanoengine::ReplacerType::LRU);

        // Transaction 1: Committed
        auto txn1 = engine.BeginTransaction();
        engine.Put(txn1, 101, "Account_A_Balance_500");
        engine.Put(txn1, 102, "Account_B_Balance_1000");
        bool c1 = engine.Commit(txn1);
        assert(c1);

        // Transaction 2: Active when crash occurs (Never commits!)
        auto txn2 = engine.BeginTransaction();
        engine.Put(txn2, 101, "CORRUPTED_HACK_UNCOMMITTED");
        engine.Put(txn2, 103, "GHOST_RECORD");

        // Verify values before crash in RAM
        std::string val101;
        engine.Get(txn2, 101, &val101);
        assert(val101 == "CORRUPTED_HACK_UNCOMMITTED");

        std::cout << "  --> Simulating catastrophic power failure / process crash...\n";
        engine.SimulateCrash();

        // 2. Run ARIES Recovery
        nanoengine::RecoveryStats stats = engine.Recover();
        std::cout << "  --> ARIES Recovery Complete in " << stats.recovery_time_ms << " ms\n";
        std::cout << "      Total Log Records: " << stats.total_log_records << "\n";
        std::cout << "      Active Loser Txns: " << stats.active_txns_at_crash << "\n";
        std::cout << "      Redo Operations:   " << stats.redo_operations << "\n";
        std::cout << "      Undo Operations:   " << stats.undo_operations << "\n";

        assert(stats.active_txns_at_crash == 1); // Txn 2 was active
        assert(stats.undo_operations >= 1);     // Rolled back Txn 2

        // 3. Verify consistency after recovery
        auto verify_txn = engine.BeginTransaction();
        std::string rec101, rec102, rec103;
        bool f101 = engine.Get(verify_txn, 101, &rec101);
        bool f102 = engine.Get(verify_txn, 102, &rec102);
        bool f103 = engine.Get(verify_txn, 103, &rec103);

        // Committed transactions must be intact
        assert(f101);
        assert(rec101 == "Account_A_Balance_500");
        assert(f102);
        assert(rec102 == "Account_B_Balance_1000");

        // Uncommitted loser transaction writes must have been rolled back!
        assert(!f103 || rec103.empty());

        engine.Commit(verify_txn);
        std::cout << "  ✓ ARIES Durability of Committed Txn 1: PASSED\n";
        std::cout << "  ✓ ARIES Rollback/Atomicity of Loser Txn 2: PASSED\n";
    }

    if (std::filesystem::exists(db_file)) std::filesystem::remove(db_file);
    if (std::filesystem::exists(log_file)) std::filesystem::remove(log_file);

    std::cout << "[PASS] All WAL & ARIES Recovery Tests Passed Successfully!\n";
    return 0;
}
