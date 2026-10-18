#include "recovery/log_manager.hpp"

namespace nanoengine {

LogManager::LogManager(DiskManager* disk_manager)
    : disk_manager_(disk_manager) {}

LogManager::~LogManager() {
    Flush(INVALID_LSN);
}

lsn_t LogManager::AppendLogRecord(LogRecord& record) {
    std::lock_guard<std::mutex> guard(latch_);
    record.lsn = next_lsn_++;
    std::string serialized = record.Serialize();
    log_buffer_ += serialized;

    if (log_buffer_.size() >= LOG_BUFFER_SIZE) {
        disk_manager_->WriteLog(log_buffer_.data(), log_buffer_.size());
        flushed_lsn_ = next_lsn_ - 1;
        log_buffer_.clear();
    }

    return record.lsn;
}

void LogManager::Flush(lsn_t target_lsn) {
    std::lock_guard<std::mutex> guard(latch_);
    if (target_lsn != INVALID_LSN && target_lsn <= flushed_lsn_) {
        return; // Already durable
    }

    if (!log_buffer_.empty()) {
        disk_manager_->WriteLog(log_buffer_.data(), log_buffer_.size());
        flushed_lsn_ = next_lsn_ - 1;
        log_buffer_.clear();
    }
}

} // namespace nanoengine
