#pragma once

#include "common/config.hpp"
#include "storage/disk_manager.hpp"
#include "storage/page.hpp"
#include "buffer/replacer.hpp"
#include <unordered_map>
#include <list>
#include <mutex>
#include <memory>
#include <atomic>
#include <vector>

namespace nanoengine {

class LogManager; // Forward declaration for WAL enforcement

struct FrameTableEntry {
    frame_id_t frame_id{INVALID_FRAME_ID};
    page_id_t page_id{INVALID_PAGE_ID};
    int pin_count{0};
    bool is_dirty{false};
    lsn_t lsn{INVALID_LSN};
};

class BufferPoolManager {
public:
    BufferPoolManager(size_t pool_size, DiskManager* disk_manager, 
                      LogManager* log_manager = nullptr,
                      ReplacerType replacer_type = ReplacerType::LRU);
    ~BufferPoolManager();

    // Disable copies
    BufferPoolManager(const BufferPoolManager&) = delete;
    BufferPoolManager& operator=(const BufferPoolManager&) = delete;

    Page* FetchPage(page_id_t page_id, bool is_write = false);
    bool UnpinPage(page_id_t page_id, bool is_dirty);
    bool FlushPage(page_id_t page_id);
    void FlushAllPages();
    Page* NewPage(page_id_t* page_id);
    bool DeletePage(page_id_t page_id);

    // Replacer switcher (LRU vs CLOCK vs LEARNED)
    void SetReplacerType(ReplacerType type);

    // Performance & Evaluation Metrics (for Presentation & Benchmarks)
    uint64_t GetBufferHits() const { return buffer_hits_.load(); }
    uint64_t GetBufferMisses() const { return buffer_misses_.load(); }
    double GetCacheHitRatio() const {
        uint64_t hits = buffer_hits_.load();
        uint64_t misses = buffer_misses_.load();
        if (hits + misses == 0) return 0.0;
        return (static_cast<double>(hits) / static_cast<double>(hits + misses)) * 100.0;
    }
    void ResetMetrics() {
        buffer_hits_.store(0);
        buffer_misses_.store(0);
    }

    size_t GetPoolSize() const { return pool_size_; }
    std::vector<FrameTableEntry> GetFrameTableSnapshot();

private:
    bool FindAvailableFrame(frame_id_t* frame_id);

    size_t pool_size_;
    DiskManager* disk_manager_;
    LogManager* log_manager_;
    std::unique_ptr<Page[]> pages_;
    std::unique_ptr<Replacer> replacer_;
    std::unordered_map<page_id_t, frame_id_t> page_table_;
    std::list<frame_id_t> free_list_;
    std::mutex latch_;

    std::atomic<uint64_t> buffer_hits_{0};
    std::atomic<uint64_t> buffer_misses_{0};
};

} // namespace nanoengine
