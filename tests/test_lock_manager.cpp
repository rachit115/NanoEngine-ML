#include "concurrency/lock_manager.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

int main() {
    std::cout << "[TEST] Running Strict 2PL Lock Manager & Deadlock Tests...\n";
    nanoengine::LockManager lm;

    // 1. Multiple shared locks allowed simultaneously
    {
        nanoengine::Transaction txn1(1);
        nanoengine::Transaction txn2(2);

        bool s1 = lm.LockShared(&txn1, "row_A");
        bool s2 = lm.LockShared(&txn2, "row_A");

        assert(s1 && s2);
        assert(txn1.GetSharedLockSet().count("row_A") == 1);
        assert(txn2.GetSharedLockSet().count("row_A") == 1);

        lm.ReleaseAll(&txn1);
        lm.ReleaseAll(&txn2);
        std::cout << "  ✓ Shared Lock Multi-Reader Concurrency: PASSED\n";
    }

    // 2. Exclusive lock blocks other transactions
    {
        nanoengine::Transaction txn1(10);
        nanoengine::Transaction txn2(20);

        bool x1 = lm.LockExclusive(&txn1, "row_B");
        assert(x1);

        std::atomic<bool> txn2_acquired{false};
        std::thread t([&]() {
            bool x2 = lm.LockExclusive(&txn2, "row_B");
            if (x2) txn2_acquired = true;
        });

        // Sleep briefly to ensure t is blocked waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        assert(!txn2_acquired);

        // When txn1 commits and releases, txn2 unblocks and gets the lock
        lm.ReleaseAll(&txn1);
        t.join();
        assert(txn2_acquired);
        lm.ReleaseAll(&txn2);
        std::cout << "  ✓ Exclusive Lock Mutual Exclusion: PASSED\n";
    }

    // 3. Deadlock detection via Wait-For Graph
    {
        nanoengine::Transaction txnA(100);
        nanoengine::Transaction txnB(200);

        lm.LockExclusive(&txnA, "res_1");
        lm.LockExclusive(&txnB, "res_2");

        // Spawn threads that attempt cross locks to simulate deadlock
        std::thread t1([&]() {
            lm.LockExclusive(&txnA, "res_2"); // Waits on txnB
        });

        std::thread t2([&]() {
            lm.LockExclusive(&txnB, "res_1"); // Waits on txnA
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(80));

        nanoengine::txn_id_t victim = nanoengine::INVALID_TXN_ID;
        bool deadlock_found = lm.HasDeadlock(&victim);
        assert(deadlock_found);
        assert(victim == 100 || victim == 200);

        std::cout << "  ✓ Wait-For Graph Cycle Deadlock Detection (Victim Txn #" << victim << "): PASSED\n";

        // Abort the victim to resolve deadlock
        if (victim == 100) {
            txnA.SetState(nanoengine::TransactionState::ABORTED);
            lm.ReleaseAll(&txnA);
        } else {
            txnB.SetState(nanoengine::TransactionState::ABORTED);
            lm.ReleaseAll(&txnB);
        }

        t1.join();
        t2.join();

        lm.ReleaseAll(&txnA);
        lm.ReleaseAll(&txnB);
    }

    std::cout << "[PASS] All Strict 2PL and Deadlock Tests Passed Successfully!\n";
    return 0;
}
