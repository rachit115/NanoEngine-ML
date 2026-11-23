#pragma once

#include "buffer/replacer.hpp"
#include <list>
#include <unordered_map>
#include <mutex>

namespace nanoengine {

class LRUReplacer : public Replacer {
public:
    explicit LRUReplacer(size_t num_frames);
    ~LRUReplacer() override = default;

    bool Victim(frame_id_t* frame_id) override;
    void Pin(frame_id_t frame_id) override;
    void Unpin(frame_id_t frame_id) override;
    size_t Size() override;

private:
    std::mutex latch_;
    size_t capacity_;
    std::list<frame_id_t> lru_list_;
    std::unordered_map<frame_id_t, std::list<frame_id_t>::iterator> lru_map_;
};

} // namespace nanoengine
