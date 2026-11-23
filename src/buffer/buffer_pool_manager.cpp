#include "buffer/buffer_pool_manager.hpp"
#include "buffer/lru_replacer.hpp"
#include "buffer/clock_replacer.hpp"
#include "buffer/learned_replacer.hpp"
#include "recovery/log_manager.hpp"
#include <algorithm>

namespace nanoengine {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager* disk_manager, 
                                     LogManager* log_manager, ReplacerType replacer_type)
    : pool_size_(pool_size), disk_manager_(disk_manager), log_manager_(log_manager) {
    
    pages_ = std::make_unique<Page[]>(pool_size_);
    
    // Populate initial free list
    for (size_t i = 0; i < pool_size_; ++i) {
        free_list_.push_back(static_cast<frame_id_t>(i));
    }

    SetReplacerType(replacer_type);
}

BufferPoolManager::~BufferPoolManager() {
    FlushAllPages();
}

void BufferPoolManager::SetReplacerType(ReplacerType type) {
    std::lock_guard<std::mutex> guard(latch_);
    switch (type) {
        case ReplacerType::CLOCK:
            replacer_ = std::make_unique<ClockReplacer>(pool_size_);
            break;
        case ReplacerType::LEARNED:
            replacer_ = std::make_unique<LearnedReplacer>(pool_size_);
            break;
        case ReplacerType::LRU:
        default:
            replacer_ = std::make_unique<LRUReplacer>(pool_size_);
            break;
    }
}

bool BufferPoolManager::FindAvailableFrame(frame_id_t* frame_id) {
    // 1. Try free list first
    if (!free_list_.empty()) {
        *frame_id = free_list_.front();
        free_list_.pop_front();
        return true;
    }

    // 2. Try replacer victim
    if (replacer_->Victim(frame_id)) {
        Page* victim_page = &pages_[*frame_id];
        if (victim_page->IsDirty()) {
            // WAL rule: Flush log before writing dirty page to disk!
            if (log_manager_ != nullptr && victim_page->GetLSN() > log_manager_->GetFlushedLSN()) {
                log_manager_->Flush(victim_page->GetLSN());
            }
            disk_manager_->WritePage(victim_page->GetPageId(), victim_page->GetData());
            victim_page->is_dirty_ = false;
        }
        page_table_.erase(victim_page->GetPageId());
        return true;
    }

    return false; // All frames are pinned!
}

Page* BufferPoolManager::FetchPage(page_id_t page_id, bool is_write) {
    std::lock_guard<std::mutex> guard(latch_);

    // Check if page is already in buffer pool (Hit)
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        frame_id_t frame_id = it->second;
        Page* page = &pages_[frame_id];
        page->pin_count_++;
        replacer_->Pin(frame_id);
        replacer_->RecordAccess(frame_id, page_id, is_write);
        buffer_hits_++;
        return page;
    }

    // Miss: Need to load from disk
    buffer_misses_++;
    frame_id_t frame_id = INVALID_FRAME_ID;
    if (!FindAvailableFrame(&frame_id)) {
        return nullptr; // No frames available to evict
    }

    Page* page = &pages_[frame_id];
    page->ResetMemory();
    page->page_id_ = page_id;
    page->pin_count_ = 1;
    page->is_dirty_ = false;

    disk_manager_->ReadPage(page_id, page->GetData());
    page_table_[page_id] = frame_id;
    replacer_->Pin(frame_id);
    replacer_->RecordAccess(frame_id, page_id, is_write);

    return page;
}

bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
    std::lock_guard<std::mutex> guard(latch_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return false;
    }

    frame_id_t frame_id = it->second;
    Page* page = &pages_[frame_id];

    if (page->GetPinCount() <= 0) {
        return false;
    }

    if (is_dirty) {
        page->is_dirty_ = true;
    }

    page->pin_count_--;
    if (page->pin_count_ == 0) {
        replacer_->Unpin(frame_id);
    }

    return true;
}

bool BufferPoolManager::FlushPage(page_id_t page_id) {
    std::lock_guard<std::mutex> guard(latch_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return false;
    }

    frame_id_t frame_id = it->second;
    Page* page = &pages_[frame_id];

    if (page->IsDirty()) {
        if (log_manager_ != nullptr && page->GetLSN() > log_manager_->GetFlushedLSN()) {
            log_manager_->Flush(page->GetLSN());
        }
        disk_manager_->WritePage(page->GetPageId(), page->GetData());
        page->is_dirty_ = false;
    }

    return true;
}

void BufferPoolManager::FlushAllPages() {
    std::lock_guard<std::mutex> guard(latch_);
    for (auto& [page_id, frame_id] : page_table_) {
        Page* page = &pages_[frame_id];
        if (page->IsDirty()) {
            if (log_manager_ != nullptr && page->GetLSN() > log_manager_->GetFlushedLSN()) {
                log_manager_->Flush(page->GetLSN());
            }
            disk_manager_->WritePage(page->GetPageId(), page->GetData());
            page->is_dirty_ = false;
        }
    }
}

Page* BufferPoolManager::NewPage(page_id_t* page_id) {
    std::lock_guard<std::mutex> guard(latch_);
    frame_id_t frame_id = INVALID_FRAME_ID;
    if (!FindAvailableFrame(&frame_id)) {
        return nullptr;
    }

    *page_id = disk_manager_->AllocatePage();
    Page* page = &pages_[frame_id];
    page->ResetMemory();
    page->page_id_ = *page_id;
    page->pin_count_ = 1;
    page->is_dirty_ = true; // New page is marked dirty so it gets saved

    page_table_[*page_id] = frame_id;
    replacer_->Pin(frame_id);
    replacer_->RecordAccess(frame_id, *page_id, true);

    return page;
}

bool BufferPoolManager::DeletePage(page_id_t page_id) {
    std::lock_guard<std::mutex> guard(latch_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return true;
    }

    frame_id_t frame_id = it->second;
    Page* page = &pages_[frame_id];

    if (page->GetPinCount() > 0) {
        return false; // Cannot delete a pinned page
    }

    page_table_.erase(page_id);
    replacer_->Pin(frame_id); // Remove from replacer
    page->ResetMemory();
    free_list_.push_back(frame_id);

    return true;
}

std::vector<FrameTableEntry> BufferPoolManager::GetFrameTableSnapshot() {
    std::lock_guard<std::mutex> guard(latch_);
    std::vector<FrameTableEntry> snapshot;
    snapshot.reserve(pool_size_);

    for (size_t i = 0; i < pool_size_; ++i) {
        FrameTableEntry entry;
        entry.frame_id = static_cast<frame_id_t>(i);
        entry.page_id = pages_[i].GetPageId();
        entry.pin_count = pages_[i].GetPinCount();
        entry.is_dirty = pages_[i].IsDirty();
        entry.lsn = pages_[i].GetLSN();
        snapshot.push_back(entry);
    }

    return snapshot;
}

} // namespace nanoengine
