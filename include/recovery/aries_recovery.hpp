#pragma once

#include "common/config.hpp"
#include "storage/disk_manager.hpp"
#include "buffer/buffer_pool_manager.hpp"
#include "recovery/log_record.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace nanoengine {

struct RecoveryStats {
    size_t total_log_records{0};
    size_t active_txns_at_crash{0};
    size_t redo_operations{0};
    size_t undo_operations{0};
    double recovery_time_ms{0.0};
};

class AriesRecoveryEngine {
public:
    AriesRecoveryEngine(DiskManager* disk_manager, BufferPoolManager* buffer_pool_manager,
                        const std::string& log_file_name);
    ~AriesRecoveryEngine() = default;

    // Executes the 3 phases of ARIES: Analysis, Redo, Undo
    RecoveryStats Recover();

    std::vector<LogRecord> ReadAllLogRecords();

private:
    void AnalysisPhase(const std::vector<LogRecord>& records,
                       std::unordered_set<txn_id_t>& active_txns,
                       std::unordered_map<txn_id_t, lsn_t>& txn_last_lsn);

    void RedoPhase(const std::vector<LogRecord>& records);

    void UndoPhase(const std::vector<LogRecord>& records,
                   const std::unordered_set<txn_id_t>& active_txns,
                   const std::unordered_map<txn_id_t, lsn_t>& txn_last_lsn);

    DiskManager* disk_manager_;
    BufferPoolManager* buffer_pool_manager_;
    std::string log_file_name_;
};

} // namespace nanoengine
