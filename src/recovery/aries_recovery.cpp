#include "recovery/aries_recovery.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <chrono>

namespace nanoengine {

AriesRecoveryEngine::AriesRecoveryEngine(DiskManager* disk_manager, 
                                         BufferPoolManager* buffer_pool_manager,
                                         const std::string& log_file_name)
    : disk_manager_(disk_manager), 
      buffer_pool_manager_(buffer_pool_manager),
      log_file_name_(log_file_name) {}

std::vector<LogRecord> AriesRecoveryEngine::ReadAllLogRecords() {
    std::vector<LogRecord> records;
    std::ifstream file(log_file_name_, std::ios::binary);
    if (!file.is_open()) {
        return records;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        LogRecord rec;
        if (LogRecord::Deserialize(line, rec)) {
            records.push_back(rec);
        }
    }
    return records;
}

void AriesRecoveryEngine::AnalysisPhase(const std::vector<LogRecord>& records,
                                        std::unordered_set<txn_id_t>& active_txns,
                                        std::unordered_map<txn_id_t, lsn_t>& txn_last_lsn) {
    for (const auto& rec : records) {
        if (rec.type == LogRecordType::BEGIN) {
            active_txns.insert(rec.txn_id);
            txn_last_lsn[rec.txn_id] = rec.lsn;
        } else if (rec.type == LogRecordType::COMMIT || rec.type == LogRecordType::ABORT) {
            active_txns.erase(rec.txn_id);
            txn_last_lsn[rec.txn_id] = rec.lsn;
        } else if (rec.type == LogRecordType::UPDATE) {
            active_txns.insert(rec.txn_id);
            txn_last_lsn[rec.txn_id] = rec.lsn;
        }
    }
}

void AriesRecoveryEngine::RedoPhase(const std::vector<LogRecord>& records) {
    // Redo all UPDATE operations forward to repeat history
    for (const auto& rec : records) {
        if (rec.type == LogRecordType::UPDATE && rec.page_id != INVALID_PAGE_ID) {
            Page* page = buffer_pool_manager_->FetchPage(rec.page_id, true);
            if (page != nullptr) {
                // Apply update if record LSN is greater than page LSN
                if (rec.lsn > page->GetLSN()) {
                    uint32_t offset = static_cast<uint32_t>((rec.key * 32) % (PAGE_SIZE - 64));
                    if (offset + rec.new_val.size() <= PAGE_SIZE) {
                        std::memcpy(page->GetData() + offset, rec.new_val.data(), rec.new_val.size());
                    }
                    page->SetLSN(rec.lsn);
                }
                buffer_pool_manager_->UnpinPage(rec.page_id, true);
            }
        }
    }
}

void AriesRecoveryEngine::UndoPhase(const std::vector<LogRecord>& records,
                                    const std::unordered_set<txn_id_t>& active_txns,
                                    const std::unordered_map<txn_id_t, lsn_t>& txn_last_lsn) {
    (void)txn_last_lsn;
    // Scan backward and undo all modifications from transactions that never committed (loser transactions)
    for (auto it = records.rbegin(); it != records.rend(); ++it) {
        const auto& rec = *it;
        if (active_txns.count(rec.txn_id) > 0 && rec.type == LogRecordType::UPDATE) {
            Page* page = buffer_pool_manager_->FetchPage(rec.page_id, true);
            if (page != nullptr) {
                // Restore old value
                uint32_t offset = static_cast<uint32_t>((rec.key * 32) % (PAGE_SIZE - 64));
                if (offset + rec.old_val.size() <= PAGE_SIZE) {
                    std::memcpy(page->GetData() + offset, rec.old_val.data(), rec.old_val.size());
                }
                buffer_pool_manager_->UnpinPage(rec.page_id, true);
            }
        }
    }
}

RecoveryStats AriesRecoveryEngine::Recover() {
    auto start = std::chrono::steady_clock::now();
    RecoveryStats stats;

    std::vector<LogRecord> records = ReadAllLogRecords();
    stats.total_log_records = records.size();

    std::unordered_set<txn_id_t> active_txns;
    std::unordered_map<txn_id_t, lsn_t> txn_last_lsn;

    // 1. Analysis Phase
    AnalysisPhase(records, active_txns, txn_last_lsn);
    stats.active_txns_at_crash = active_txns.size();

    // 2. Redo Phase
    RedoPhase(records);
    for (const auto& r : records) {
        if (r.type == LogRecordType::UPDATE) stats.redo_operations++;
    }

    // 3. Undo Phase
    UndoPhase(records, active_txns, txn_last_lsn);
    for (const auto& r : records) {
        if (active_txns.count(r.txn_id) > 0 && r.type == LogRecordType::UPDATE) {
            stats.undo_operations++;
        }
    }

    // Flush all restored pages to disk
    buffer_pool_manager_->FlushAllPages();

    auto end = std::chrono::steady_clock::now();
    stats.recovery_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return stats;
}

} // namespace nanoengine
