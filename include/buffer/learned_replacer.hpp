#pragma once

#include "buffer/replacer.hpp"
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <mutex>
#include <vector>

namespace nanoengine {

struct PageFeatures {
    page_id_t page_id{INVALID_PAGE_ID};
    uint32_t access_count{0};
    std::chrono::steady_clock::time_point last_access;
    bool is_index_page{false};
    bool is_write{false};
    double ema_frequency{0.0}; // Exponential moving average of access rate
};

class LearnedReplacer : public Replacer {
public:
    explicit LearnedReplacer(size_t num_frames);
    ~LearnedReplacer() override = default;

    bool Victim(frame_id_t* frame_id) override;
    void Pin(frame_id_t frame_id) override;
    void Unpin(frame_id_t frame_id) override;
    size_t Size() override;

    void RecordAccess(frame_id_t frame_id, page_id_t page_id, bool is_write = false) override;
    void SetPageType(page_id_t page_id, bool is_index);

    // Dynamic model weight tuning
    void SetModelWeights(double w_freq, double w_recency, double w_index, double w_dirty);

private:
    double ComputeRetentionScore(const PageFeatures& feat, const std::chrono::steady_clock::time_point& now) const;

    std::mutex latch_;
    size_t capacity_;
    std::unordered_set<frame_id_t> unpinned_frames_;
    std::unordered_map<frame_id_t, PageFeatures> frame_features_;
    std::unordered_map<page_id_t, bool> page_index_types_;

    // Learned Model Feature Weights
    double weight_frequency_{2.5};
    double weight_recency_{1.8};
    double weight_index_bonus_{5.0};
    double weight_dirty_penalty_{-0.5}; // Slightly prefer evicting clean over dirty to save disk I/O
};

} // namespace nanoengine
