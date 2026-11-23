#pragma once

#include "common/config.hpp"
#include <string>
#include <fstream>
#include <mutex>
#include <atomic>

namespace nanoengine {

class DiskManager {
public:
    explicit DiskManager(const std::string& db_file, const std::string& log_file);
    ~DiskManager();

    // Disable copy
    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    void WritePage(page_id_t page_id, const char* page_data);
    void ReadPage(page_id_t page_id, char* page_data);

    page_id_t AllocatePage();
    size_t GetNumPages() const { return num_pages_.load(); }

    void WriteLog(const char* log_data, size_t size);
    bool ReadLog(char* log_data, size_t size, size_t offset);

    void FlushDbFile();
    void FlushLogFile();

    // Stats for presentation & benchmarks
    uint64_t GetNumDiskReads() const { return num_reads_.load(); }
    uint64_t GetNumDiskWrites() const { return num_writes_.load(); }
    void ResetMetrics() {
        num_reads_.store(0);
        num_writes_.store(0);
    }

private:
    std::string db_file_name_;
    std::string log_file_name_;
    std::fstream db_io_;
    std::fstream log_io_;
    std::mutex db_io_latch_;
    std::mutex log_io_latch_;

    std::atomic<page_id_t> num_pages_{0};
    std::atomic<uint64_t> num_reads_{0};
    std::atomic<uint64_t> num_writes_{0};
};

} // namespace nanoengine
