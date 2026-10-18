#include "concurrency/lock_manager.hpp"

namespace nanoengine {

bool LockManager::LockShared(Transaction* txn, const std::string& key) {
    if (txn->GetState() != TransactionState::GROWING) {
        return false; // 2PL rule: cannot acquire locks in shrinking/committed/aborted states
    }

    std::unique_lock<std::mutex> global_lock(global_latch_);
    LockHead& head = lock_table_[key];
    global_lock.unlock();

    std::unique_lock<std::mutex> lock(head.latch);
    
    // Check if already holding lock
    if (head.shared_holders.count(txn->GetTxnId()) > 0) {
        return true;
    }

    // Wait if exclusive lock is held by another transaction
    head.request_queue.emplace_back(txn->GetTxnId(), LockMode::SHARED);
    auto it = --head.request_queue.end();

    head.cv.wait_for(lock, std::chrono::milliseconds(200), [&]() {
        if (txn->GetState() == TransactionState::ABORTED) {
            return true;
        }
        return !head.is_exclusive;
    });

    if (txn->GetState() == TransactionState::ABORTED) {
        head.request_queue.erase(it);
        return false;
    }

    it->is_granted = true;
    head.shared_count++;
    head.shared_holders.insert(txn->GetTxnId());
    head.request_queue.erase(it);

    txn->AddSharedLock(key);
    return true;
}

bool LockManager::LockExclusive(Transaction* txn, const std::string& key) {
    if (txn->GetState() != TransactionState::GROWING) {
        return false; // 2PL rule: cannot acquire locks in shrinking/committed/aborted states
    }

    std::unique_lock<std::mutex> global_lock(global_latch_);
    LockHead& head = lock_table_[key];
    global_lock.unlock();

    std::unique_lock<std::mutex> lock(head.latch);

    if (head.is_exclusive && head.exclusive_holder == txn->GetTxnId()) {
        return true;
    }

    head.request_queue.emplace_back(txn->GetTxnId(), LockMode::EXCLUSIVE);
    auto it = --head.request_queue.end();

    head.cv.wait_for(lock, std::chrono::milliseconds(200), [&]() {
        if (txn->GetState() == TransactionState::ABORTED) {
            return true;
        }
        return !head.is_exclusive && head.shared_count == 0;
    });

    if (txn->GetState() == TransactionState::ABORTED) {
        head.request_queue.erase(it);
        return false;
    }

    it->is_granted = true;
    head.is_exclusive = true;
    head.exclusive_holder = txn->GetTxnId();
    head.request_queue.erase(it);

    txn->AddExclusiveLock(key);
    return true;
}

bool LockManager::Unlock(Transaction* txn, const std::string& key) {
    std::unique_lock<std::mutex> global_lock(global_latch_);
    auto map_it = lock_table_.find(key);
    if (map_it == lock_table_.end()) {
        return false;
    }
    LockHead& head = map_it->second;
    global_lock.unlock();

    std::lock_guard<std::mutex> lock(head.latch);

    // Strict 2PL transitions to shrinking if not aborting
    if (txn->GetState() == TransactionState::GROWING) {
        txn->SetState(TransactionState::SHRINKING);
    }

    if (head.is_exclusive && head.exclusive_holder == txn->GetTxnId()) {
        head.is_exclusive = false;
        head.exclusive_holder = INVALID_TXN_ID;
        head.cv.notify_all();
        return true;
    }

    if (head.shared_holders.erase(txn->GetTxnId()) > 0) {
        head.shared_count--;
        head.cv.notify_all();
        return true;
    }

    return false;
}

void LockManager::ReleaseAll(Transaction* txn) {
    // Release all shared locks
    for (const auto& key : txn->GetSharedLockSet()) {
        Unlock(txn, key);
    }
    // Release all exclusive locks
    for (const auto& key : txn->GetExclusiveLockSet()) {
        Unlock(txn, key);
    }
    txn->ClearLocks();

    // Notify any threads waiting across tables
    std::lock_guard<std::mutex> global_lock(global_latch_);
    for (auto& [_, head] : lock_table_) {
        std::lock_guard<std::mutex> lock(head.latch);
        head.cv.notify_all();
    }
}

void LockManager::BuildWaitForGraph() {
    std::lock_guard<std::mutex> global_lock(global_latch_);
    wait_for_graph_.clear();

    for (auto& [key, head] : lock_table_) {
        std::lock_guard<std::mutex> lock(head.latch);
        
        // Find who is waiting
        for (const auto& req : head.request_queue) {
            if (!req.is_granted) {
                if (head.is_exclusive && head.exclusive_holder != INVALID_TXN_ID) {
                    wait_for_graph_[req.txn_id].insert(head.exclusive_holder);
                }
                for (txn_id_t shared_holder : head.shared_holders) {
                    if (shared_holder != req.txn_id) {
                        wait_for_graph_[req.txn_id].insert(shared_holder);
                    }
                }
            }
        }
    }
}

bool LockManager::CheckDeadlockCycle(txn_id_t current, std::unordered_map<txn_id_t, bool>& visited,
                                     std::unordered_map<txn_id_t, bool>& on_stack, txn_id_t* cycle_victim) {
    visited[current] = true;
    on_stack[current] = true;

    for (txn_id_t neighbor : wait_for_graph_[current]) {
        if (!visited[neighbor]) {
            if (CheckDeadlockCycle(neighbor, visited, on_stack, cycle_victim)) {
                return true;
            }
        } else if (on_stack[neighbor]) {
            // Cycle detected! Pick youngest txn as victim
            *cycle_victim = std::max(current, neighbor);
            return true;
        }
    }

    on_stack[current] = false;
    return false;
}

bool LockManager::HasDeadlock(txn_id_t* victim_txn_id) {
    BuildWaitForGraph();

    std::unordered_map<txn_id_t, bool> visited;
    std::unordered_map<txn_id_t, bool> on_stack;

    for (const auto& [txn_id, _] : wait_for_graph_) {
        if (!visited[txn_id]) {
            if (CheckDeadlockCycle(txn_id, visited, on_stack, victim_txn_id)) {
                return true;
            }
        }
    }

    return false;
}

} // namespace nanoengine
