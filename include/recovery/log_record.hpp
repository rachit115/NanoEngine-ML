#pragma once

#include "common/config.hpp"
#include <string>
#include <sstream>

namespace nanoengine {

enum class LogRecordType {
    INVALID = 0,
    BEGIN,
    COMMIT,
    ABORT,
    UPDATE,
    CHECKPOINT
};

struct LogRecord {
    lsn_t lsn{INVALID_LSN};
    lsn_t prev_lsn{INVALID_LSN};
    txn_id_t txn_id{INVALID_TXN_ID};
    LogRecordType type{LogRecordType::INVALID};
    
    // For UPDATE records
    page_id_t page_id{INVALID_PAGE_ID};
    int64_t key{0};
    std::string old_val;
    std::string new_val;

    LogRecord() = default;

    // Helper constructor for transaction lifecycle records
    LogRecord(txn_id_t tid, lsn_t prev, LogRecordType t)
        : prev_lsn(prev), txn_id(tid), type(t) {}

    // Helper constructor for update records
    LogRecord(txn_id_t tid, lsn_t prev, LogRecordType t, page_id_t pid, int64_t k, 
              const std::string& old_v, const std::string& new_v)
        : prev_lsn(prev), txn_id(tid), type(t), page_id(pid), key(k), 
          old_val(old_v), new_val(new_v) {}

    std::string Serialize() const {
        std::stringstream ss;
        ss << lsn << " " << prev_lsn << " " << txn_id << " " 
           << static_cast<int>(type) << " " << page_id << " " 
           << key << " " << old_val.size() << " " << old_val << " "
           << new_val.size() << " " << new_val << "\n";
        return ss.str();
    }

    static bool Deserialize(const std::string& line, LogRecord& record) {
        std::stringstream ss(line);
        int t = 0;
        size_t old_len = 0, new_len = 0;
        if (!(ss >> record.lsn >> record.prev_lsn >> record.txn_id >> t >> record.page_id >> record.key)) {
            return false;
        }
        record.type = static_cast<LogRecordType>(t);
        if (!(ss >> old_len)) return false;
        
        // Skip leading space
        ss.get();
        if (old_len > 0) {
            record.old_val.resize(old_len);
            ss.read(&record.old_val[0], old_len);
        } else {
            record.old_val.clear();
        }

        if (!(ss >> new_len)) return false;
        ss.get();
        if (new_len > 0) {
            record.new_val.resize(new_len);
            ss.read(&record.new_val[0], new_len);
        } else {
            record.new_val.clear();
        }

        return true;
    }
};

} // namespace nanoengine
