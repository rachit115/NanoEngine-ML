#pragma once

#include "buffer/replacer.hpp"
#include <vector>
#include <mutex>

namespace nanoengine {

class ClockReplacer : public Replacer {
public:
    explicit ClockReplacer(size_t num_frames);
    ~ClockReplacer() override = default;

    bool Victim(frame_id_t* frame_id) override;
    void Pin(frame_id_t frame_id) override;
    void Unpin(frame_id_t frame_id) override;
    size_t Size() override;

private:
    struct ClockEntry {
        bool in_replacer{false};
        bool ref_bit{false};
    };

    std::mutex latch_;
    size_t capacity_;
    size_t clock_hand_{0};
    size_t size_{0};
    std::vector<ClockEntry> entries_;
};

} // namespace nanoengine
