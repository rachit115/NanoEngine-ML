#pragma once

#include "common/config.hpp"
#include "storage/disk_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "concurrency/lock_manager.hpp"
#include "concurrency/transaction.hpp"
#include "recovery/log_manager.hpp"
#include "recovery/aries_recovery.hpp"
#include "index/b_plus_tree.hpp"
#include <memory>
#include <string>
#include <atomic>

namespace nanoengine {

struct EngineStats {
    uint64_t buffer_hits{0};
    uint64_t buffer_misses{0};
    double cache_hit_ratio{0.0};
    uint64_t disk_reads{0};
    uint64_t disk_writes{0};
    size_t index_num_keys{0};
};

class NanoEngine {
public:
    NanoEngine(const std::string& db_file = "nano_data.db",
               const std::string& log_file = "nano_wal.log",
               size_t buffer_pool_size = DEFAULT_BUFFER_POOL_SIZE,
               ReplacerType replacer_type = ReplacerType::LEARNED);
    ~NanoEngine();

    // Transaction Management
    std::shared_ptr<Transaction> BeginTransaction();
    bool Commit(std::shared_ptr<Transaction> txn);
    bool Abort(std::shared_ptr<Transaction> txn);

    // CRUD Storage Operations
    bool Put(std::shared_ptr<Transaction> txn, int64_t key, const std::string& value);
    bool Get(std::shared_ptr<Transaction> txn, int64_t key, std::string* result);
    bool Delete(std::shared_ptr<Transaction> txn, int64_t key);
    std::vector<std::pair<int64_t, std::string>> Scan(std::shared_ptr<Transaction> txn, 
                                                      int64_t min_key, int64_t max_key);

    // Replacer switcher for performance comparisons
    void SetReplacerType(ReplacerType type);

    // Crash Simulation & ARIES Recovery
    void SimulateCrash();
    RecoveryStats Recover();

    // Metrics & Frame Table
    EngineStats GetStats();
    std::vector<FrameTableEntry> GetFrameTableSnapshot();
    void ResetMetrics();

    // Module accessors
    DiskManager* GetDiskManager() { return disk_manager_.get(); }
    BufferPoolManager* GetBufferPoolManager() { return buffer_pool_manager_.get(); }
    LockManager* GetLockManager() { return lock_manager_.get(); }
    LogManager* GetLogManager() { return log_manager_.get(); }
    ConcurrentBPlusTree* GetIndex() { return index_.get(); }

private:
    std::string db_file_name_;
    std::string log_file_name_;
    size_t buffer_pool_size_;

    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
    std::unique_ptr<LockManager> lock_manager_;
    std::unique_ptr<ConcurrentBPlusTree> index_;
    std::unique_ptr<AriesRecoveryEngine> recovery_engine_;

    std::atomic<txn_id_t> next_txn_id_{1};
    std::unordered_map<txn_id_t, std::shared_ptr<Transaction>> active_transactions_;
    std::mutex txn_map_latch_;
};

} // namespace nanoengine
