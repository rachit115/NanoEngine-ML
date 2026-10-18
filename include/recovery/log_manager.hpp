#pragma once

#include "common/config.hpp"
#include "storage/disk_manager.hpp"
#include "recovery/log_record.hpp"
#include <mutex>
#include <string>
#include <atomic>
#include <vector>

namespace nanoengine {

class LogManager {
public:
    explicit LogManager(DiskManager* disk_manager);
    ~LogManager();

    // Append log record to memory buffer; assigns next LSN
    lsn_t AppendLogRecord(LogRecord& record);

    // Flushes log records up to specified LSN (or all if target_lsn == INVALID_LSN)
    void Flush(lsn_t target_lsn = INVALID_LSN);

    lsn_t GetFlushedLSN() const { return flushed_lsn_.load(); }
    lsn_t GetNextLSN() const { return next_lsn_.load(); }

    void SetFlushedLSN(lsn_t lsn) { flushed_lsn_.store(lsn); }
    void SetNextLSN(lsn_t lsn) { next_lsn_.store(lsn); }

private:
    DiskManager* disk_manager_;
    std::mutex latch_;
    std::string log_buffer_;
    std::atomic<lsn_t> next_lsn_{0};
    std::atomic<lsn_t> flushed_lsn_{INVALID_LSN};
};

} // namespace nanoengine
