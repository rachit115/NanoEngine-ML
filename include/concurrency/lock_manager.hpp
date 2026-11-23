#pragma once

#include "common/config.hpp"
#include "concurrency/transaction.hpp"
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>

namespace nanoengine {

enum class LockMode {
    SHARED,
    EXCLUSIVE
};

struct LockRequest {
    txn_id_t txn_id;
    LockMode lock_mode;
    bool is_granted{false};

    LockRequest(txn_id_t tid, LockMode mode)
        : txn_id(tid), lock_mode(mode) {}
};

struct LockHead {
    std::mutex latch;
    std::condition_variable cv;
    std::list<LockRequest> request_queue;
    int shared_count{0};
    bool is_exclusive{false};
    txn_id_t exclusive_holder{INVALID_TXN_ID};
    std::unordered_set<txn_id_t> shared_holders;
};

class LockManager {
public:
    LockManager() = default;
    ~LockManager() = default;

    // Strict 2PL locking
    bool LockShared(Transaction* txn, const std::string& key);
    bool LockExclusive(Transaction* txn, const std::string& key);
    bool Unlock(Transaction* txn, const std::string& key);
    void ReleaseAll(Transaction* txn);

    // Deadlock detection using Wait-For Graph (WFG)
    bool HasDeadlock(txn_id_t* victim_txn_id);
    void BuildWaitForGraph();

private:
    bool CheckDeadlockCycle(txn_id_t current, std::unordered_map<txn_id_t, bool>& visited,
                            std::unordered_map<txn_id_t, bool>& on_stack, txn_id_t* cycle_victim);

    std::mutex global_latch_;
    std::unordered_map<std::string, LockHead> lock_table_;
    
    // Wait-For Graph: txn_id -> set of txn_ids it is waiting for
    std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> wait_for_graph_;
};

} // namespace nanoengine
