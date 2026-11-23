#include "buffer/buffer_pool_manager.hpp"
#include "buffer/lru_replacer.hpp"
#include "buffer/clock_replacer.hpp"
#include "buffer/learned_replacer.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <filesystem>

int main() {
    std::cout << "[TEST] Running Buffer Pool Manager & Replacer Tests...\n";
    std::string db_file = "test_bpm.db";
    std::string log_file = "test_bpm.log";

    if (std::filesystem::exists(db_file)) std::filesystem::remove(db_file);
    if (std::filesystem::exists(log_file)) std::filesystem::remove(log_file);

    // 1. LRU Replacer basic test
    {
        nanoengine::LRUReplacer lru(3);
        lru.Unpin(0);
        lru.Unpin(1);
        lru.Unpin(2);
        assert(lru.Size() == 3);

        nanoengine::frame_id_t victim;
        assert(lru.Victim(&victim) && victim == 0);
        assert(lru.Victim(&victim) && victim == 1);
        lru.Pin(2);
        assert(lru.Size() == 0);
        std::cout << "  ✓ LRU Replacer Eviction & Pin: PASSED\n";
    }

    // 2. CLOCK Replacer test (Second-Chance)
    {
        nanoengine::ClockReplacer clock(3);
        clock.Unpin(0);
        clock.Unpin(1);
        clock.Unpin(2);
        nanoengine::frame_id_t victim;
        // First sweep clears reference bits or selects victim
        assert(clock.Victim(&victim));
        std::cout << "  ✓ CLOCK Second-Chance Replacer: PASSED\n";
    }

    // 3. Learned Replacer test
    {
        nanoengine::LearnedReplacer learned(3);
        learned.SetPageType(10, true); // Index page (high priority)
        learned.RecordAccess(0, 10);
        learned.RecordAccess(1, 20); // Normal page

        learned.Unpin(0);
        learned.Unpin(1);

        nanoengine::frame_id_t victim;
        assert(learned.Victim(&victim));
        // Normal page should be evicted before index page
        assert(victim == 1);
        std::cout << "  ✓ Learned Replacer Feature-Based Priority: PASSED\n";
    }

    // 4. Integrated Buffer Pool Manager test with 4KB pages & eviction
    {
        nanoengine::DiskManager dm(db_file, log_file);
        // Small buffer pool of 3 frames
        nanoengine::BufferPoolManager bpm(3, &dm, nullptr, nanoengine::ReplacerType::LRU);

        nanoengine::page_id_t p0, p1, p2, p3;
        nanoengine::Page* page0 = bpm.NewPage(&p0);
        nanoengine::Page* page1 = bpm.NewPage(&p1);
        nanoengine::Page* page2 = bpm.NewPage(&p2);

        assert(page0 != nullptr);
        assert(page1 != nullptr);
        assert(page2 != nullptr);

        // Write sample data to page0
        std::strcpy(page0->GetData(), "Data on Page 0");

        // Unpin page0 and mark dirty
        bpm.UnpinPage(p0, true);
        bpm.UnpinPage(p1, false);
        bpm.UnpinPage(p2, false);

        // Fetching page3 should trigger eviction of page0
        nanoengine::Page* page3 = bpm.NewPage(&p3);
        assert(page3 != nullptr);
        bpm.UnpinPage(p3, false);

        // Fetch page0 back from disk to verify dirty page was written during eviction
        page0 = bpm.FetchPage(p0);
        assert(page0 != nullptr);
        assert(std::strcmp(page0->GetData(), "Data on Page 0") == 0);
        bpm.UnpinPage(p0, false);

        std::cout << "  ✓ Buffer Pool Eviction, Pin Tracking & Dirty Flush: PASSED\n";
    }

    if (std::filesystem::exists(db_file)) std::filesystem::remove(db_file);
    if (std::filesystem::exists(log_file)) std::filesystem::remove(log_file);

    std::cout << "[PASS] All Buffer Pool Tests Passed Successfully!\n";
    return 0;
}
