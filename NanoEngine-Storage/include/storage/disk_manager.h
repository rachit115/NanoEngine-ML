#pragma once

#include "storage/page.h"

#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>

namespace nanoengine::storage {

using page_id_t = std::uint64_t;

// Reads and writes fixed-size pages in a single binary database file.
// This class is the low-level disk layer; it is not a buffer pool.
class DiskManager {
public:
    explicit DiskManager(std::string file_name);
    ~DiskManager();

    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    // Returns false if the requested page is not fully present in the file.
    bool read_page(page_id_t page_id, Page& page);

    // Writes exactly one page at page_id * Page::PAGE_SIZE.
    // Throws std::runtime_error if the write fails.
    void write_page(page_id_t page_id, const Page& page);

    const std::string& file_name() const noexcept;

private:
    std::streamoff page_offset(page_id_t page_id) const;
    void open_or_create_file();

    std::string file_name_;
    std::fstream file_;
    std::mutex file_mutex_;
};

}  // namespace nanoengine::storage
