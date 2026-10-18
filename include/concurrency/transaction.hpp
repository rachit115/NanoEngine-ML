#pragma once

#include "common/config.hpp"
#include <unordered_set>
#include <string>
#include <mutex>

namespace nanoengine {

enum class TransactionState {
    GROWING,
    SHRINKING,
    COMMITTED,
    ABORTED
};

class Transaction {
public:
    explicit Transaction(txn_id_t txn_id)
        : txn_id_(txn_id), state_(TransactionState::GROWING) {}

    txn_id_t GetTxnId() const { return txn_id_; }
    TransactionState GetState() const { return state_; }
    void SetState(TransactionState state) { state_ = state; }

    lsn_t GetPrevLSN() const { return prev_lsn_; }
    void SetPrevLSN(lsn_t lsn) { prev_lsn_ = lsn; }

    // Lock tracking for Strict 2PL
    void AddSharedLock(const std::string& key) {
        std::lock_guard<std::mutex> guard(latch_);
        shared_lock_set_.insert(key);
    }

    void AddExclusiveLock(const std::string& key) {
        std::lock_guard<std::mutex> guard(latch_);
        exclusive_lock_set_.insert(key);
    }

    const std::unordered_set<std::string>& GetSharedLockSet() const { return shared_lock_set_; }
    const std::unordered_set<std::string>& GetExclusiveLockSet() const { return exclusive_lock_set_; }

    void ClearLocks() {
        std::lock_guard<std::mutex> guard(latch_);
        shared_lock_set_.clear();
        exclusive_lock_set_.clear();
    }

private:
    txn_id_t txn_id_;
    TransactionState state_;
    lsn_t prev_lsn_{INVALID_LSN};
    std::mutex latch_;
    std::unordered_set<std::string> shared_lock_set_;
    std::unordered_set<std::string> exclusive_lock_set_;
};

} // namespace nanoengine
