#include "buffer/clock_replacer.hpp"

namespace nanoengine {

ClockReplacer::ClockReplacer(size_t num_frames)
    : capacity_(num_frames), entries_(num_frames) {}

bool ClockReplacer::Victim(frame_id_t* frame_id) {
    std::lock_guard<std::mutex> guard(latch_);
    if (size_ == 0) {
        return false;
    }

    size_t scanned = 0;
    while (scanned < (capacity_ * 2)) {
        if (entries_[clock_hand_].in_replacer) {
            if (entries_[clock_hand_].ref_bit) {
                // Give second chance, reset bit
                entries_[clock_hand_].ref_bit = false;
            } else {
                // Found victim
                *frame_id = static_cast<frame_id_t>(clock_hand_);
                entries_[clock_hand_].in_replacer = false;
                size_--;
                clock_hand_ = (clock_hand_ + 1) % capacity_;
                return true;
            }
        }
        clock_hand_ = (clock_hand_ + 1) % capacity_;
        scanned++;
    }

    // Fallback: pick any frame currently in replacer
    for (size_t i = 0; i < capacity_; ++i) {
        if (entries_[i].in_replacer) {
            *frame_id = static_cast<frame_id_t>(i);
            entries_[i].in_replacer = false;
            entries_[i].ref_bit = false;
            size_--;
            return true;
        }
    }

    return false;
}

void ClockReplacer::Pin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> guard(latch_);
    if (static_cast<size_t>(frame_id) >= capacity_) return;

    if (entries_[frame_id].in_replacer) {
        entries_[frame_id].in_replacer = false;
        entries_[frame_id].ref_bit = false;
        size_--;
    }
}

void ClockReplacer::Unpin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> guard(latch_);
    if (static_cast<size_t>(frame_id) >= capacity_) return;

    if (!entries_[frame_id].in_replacer) {
        entries_[frame_id].in_replacer = true;
        entries_[frame_id].ref_bit = true;
        size_++;
    }
}

size_t ClockReplacer::Size() {
    std::lock_guard<std::mutex> guard(latch_);
    return size_;
}

} // namespace nanoengine
