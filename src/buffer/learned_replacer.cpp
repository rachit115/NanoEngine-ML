#include "buffer/learned_replacer.hpp"
#include <cmath>
#include <limits>

namespace nanoengine {

LearnedReplacer::LearnedReplacer(size_t num_frames) : capacity_(num_frames) {}

void LearnedReplacer::SetModelWeights(double w_freq, double w_recency, double w_index, double w_dirty) {
    std::lock_guard<std::mutex> guard(latch_);
    weight_frequency_ = w_freq;
    weight_recency_ = w_recency;
    weight_index_bonus_ = w_index;
    weight_dirty_penalty_ = w_dirty;
}

void LearnedReplacer::SetPageType(page_id_t page_id, bool is_index) {
    std::lock_guard<std::mutex> guard(latch_);
    page_index_types_[page_id] = is_index;
}

void LearnedReplacer::RecordAccess(frame_id_t frame_id, page_id_t page_id, bool is_write) {
    std::lock_guard<std::mutex> guard(latch_);
    auto now = std::chrono::steady_clock::now();
    auto& feat = frame_features_[frame_id];
    
    if (feat.page_id != page_id) {
        // New page in frame
        feat.page_id = page_id;
        feat.access_count = 1;
        feat.ema_frequency = 1.0;
    } else {
        feat.access_count++;
        // Update exponential moving average
        feat.ema_frequency = 0.8 * feat.ema_frequency + 0.2 * feat.access_count;
    }

    feat.last_access = now;
    feat.is_write = is_write;
    
    auto type_it = page_index_types_.find(page_id);
    if (type_it != page_index_types_.end()) {
        feat.is_index_page = type_it->second;
    }
}

double LearnedReplacer::ComputeRetentionScore(const PageFeatures& feat, const std::chrono::steady_clock::time_point& now) const {
    // Delta in milliseconds
    auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - feat.last_access).count();
    
    // Normalized recency: 1.0 / (1.0 + log(1 + delta_ms))
    double recency_score = 1.0 / (1.0 + std::log1p(static_cast<double>(delta_ms)));
    
    // Scan resistance: if access_count == 1 and it's not an index page, discount heavily
    double freq_score = static_cast<double>(feat.access_count);
    if (feat.access_count == 1 && !feat.is_index_page) {
        freq_score = 0.2; // Table scan detection!
    }

    double index_score = feat.is_index_page ? 1.0 : 0.0;
    double dirty_score = feat.is_write ? 1.0 : 0.0;

    return (weight_frequency_ * freq_score) +
           (weight_recency_ * recency_score) +
           (weight_index_bonus_ * index_score) +
           (weight_dirty_penalty_ * dirty_score);
}

bool LearnedReplacer::Victim(frame_id_t* frame_id) {
    std::lock_guard<std::mutex> guard(latch_);
    if (unpinned_frames_.empty()) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    frame_id_t best_victim = INVALID_FRAME_ID;
    double lowest_score = std::numeric_limits<double>::max();

    for (frame_id_t fid : unpinned_frames_) {
        double score = 0.0;
        auto it = frame_features_.find(fid);
        if (it != frame_features_.end()) {
            score = ComputeRetentionScore(it->second, now);
        }

        if (score < lowest_score) {
            lowest_score = score;
            best_victim = fid;
        }
    }

    if (best_victim != INVALID_FRAME_ID) {
        *frame_id = best_victim;
        unpinned_frames_.erase(best_victim);
        frame_features_.erase(best_victim);
        return true;
    }

    return false;
}

void LearnedReplacer::Pin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> guard(latch_);
    unpinned_frames_.erase(frame_id);
}

void LearnedReplacer::Unpin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> guard(latch_);
    unpinned_frames_.insert(frame_id);
}

size_t LearnedReplacer::Size() {
    std::lock_guard<std::mutex> guard(latch_);
    return unpinned_frames_.size();
}

} // namespace nanoengine
