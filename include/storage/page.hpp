#pragma once

#include "common/config.hpp"
#include <cstring>
#include <shared_mutex>

namespace nanoengine {

class Page {
    friend class BufferPoolManager;
public:
    Page() {
        ResetMemory();
    }
    ~Page() = default;

    // Disallow copies, allow moves
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;

    char* GetData() { return data_; }
    const char* GetData() const { return data_; }

    page_id_t GetPageId() const { return page_id_; }
    int GetPinCount() const { return pin_count_; }
    bool IsDirty() const { return is_dirty_; }
    lsn_t GetLSN() const { return page_lsn_; }

    void SetLSN(lsn_t lsn) { page_lsn_ = lsn; }

    // Concurrency latches (Read/Write)
    void RLock() { rwlatch_.lock_shared(); }
    void RUnlock() { rwlatch_.unlock_shared(); }
    void WLock() { rwlatch_.lock(); }
    void WUnlock() { rwlatch_.unlock(); }

private:
    void ResetMemory() {
        std::memset(data_, 0, PAGE_SIZE);
        page_id_ = INVALID_PAGE_ID;
        pin_count_ = 0;
        is_dirty_ = false;
        page_lsn_ = INVALID_LSN;
    }

    char data_[PAGE_SIZE];
    page_id_t page_id_{INVALID_PAGE_ID};
    int pin_count_{0};
    bool is_dirty_{false};
    lsn_t page_lsn_{INVALID_LSN};
    std::shared_mutex rwlatch_;
};

} // namespace nanoengine
