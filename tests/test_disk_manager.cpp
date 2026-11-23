#include "storage/disk_manager.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <filesystem>

int main() {
    std::cout << "[TEST] Running Disk Manager 4KB Page Tests...\n";
    std::string db_file = "test_disk.db";
    std::string log_file = "test_disk.log";

    if (std::filesystem::exists(db_file)) std::filesystem::remove(db_file);
    if (std::filesystem::exists(log_file)) std::filesystem::remove(log_file);

    {
        nanoengine::DiskManager dm(db_file, log_file);

        // Test 1: Allocate pages
        nanoengine::page_id_t p0 = dm.AllocatePage();
        nanoengine::page_id_t p1 = dm.AllocatePage();
        assert(p0 == 0);
        assert(p1 == 1);
        assert(dm.GetNumPages() == 2);

        // Test 2: Write 4KB page
        char write_buf[nanoengine::PAGE_SIZE];
        std::memset(write_buf, 0xAB, nanoengine::PAGE_SIZE);
        std::string sample = "NanoEngine-ML: Storage Page 0 Content Verified!";
        std::memcpy(write_buf, sample.data(), sample.size());
        dm.WritePage(p0, write_buf);

        // Test 3: Read back 4KB page
        char read_buf[nanoengine::PAGE_SIZE];
        std::memset(read_buf, 0, nanoengine::PAGE_SIZE);
        dm.ReadPage(p0, read_buf);
        assert(std::memcmp(write_buf, read_buf, nanoengine::PAGE_SIZE) == 0);
        assert(std::string(read_buf, sample.size()) == sample);

        // Test 4: Write & Read Log
        std::string log_entry = "LOG_RECORD_LSN_101_COMMIT\n";
        dm.WriteLog(log_entry.data(), log_entry.size());
        char log_read[64];
        std::memset(log_read, 0, sizeof(log_read));
        dm.ReadLog(log_read, log_entry.size(), 0);
        assert(std::string(log_read, log_entry.size()) == log_entry);

        std::cout << "  ✓ Page Allocation & 4KB Read/Write: PASSED\n";
        std::cout << "  ✓ Binary WAL Disk Logging: PASSED\n";
    }

    // Clean up
    if (std::filesystem::exists(db_file)) std::filesystem::remove(db_file);
    if (std::filesystem::exists(log_file)) std::filesystem::remove(log_file);

    std::cout << "[PASS] All Disk Manager Tests Passed Successfully!\n";
    return 0;
}
