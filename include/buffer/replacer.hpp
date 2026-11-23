#pragma once

#include "common/config.hpp"
#include <cstddef>

namespace nanoengine {

enum class ReplacerType {
    LRU,
    CLOCK,
    LEARNED
};

class Replacer {
public:
    virtual ~Replacer() = default;

    // Pick a victim frame to evict. Returns true if victim found.
    virtual bool Victim(frame_id_t* frame_id) = 0;

    // Pin a frame (tell replacer this frame is now in-use and cannot be evicted)
    virtual void Pin(frame_id_t frame_id) = 0;

    // Unpin a frame (tell replacer this frame is no longer in-use and can be evicted)
    virtual void Unpin(frame_id_t frame_id) = 0;

    // Returns number of frames eligible for eviction
    virtual size_t Size() = 0;

    // Record access for learned/adaptive replacers
    virtual void RecordAccess(frame_id_t frame_id, page_id_t page_id, bool is_write = false) {
        (void)frame_id;
        (void)page_id;
        (void)is_write;
    }
};

} // namespace nanoengine
