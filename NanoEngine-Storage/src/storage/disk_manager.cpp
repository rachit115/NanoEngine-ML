#include "storage/disk_manager.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <stdexcept>
#include <cstring>

namespace nanoengine {

DiskManager::DiskManager(const std::string& db_file, const std::string& log_file)
    : db_file_name_(db_file), log_file_name_(log_file) {
    
    // Ensure parent directory exists
    std::filesystem::path db_path(db_file_name_);
    if (db_path.has_parent_path()) {
        std::filesystem::create_directories(db_path.parent_path());
    }

    // Open/Create db file in binary read/write mode
    db_io_.open(db_file_name_, std::ios::binary | std::ios::in | std::ios::out);
    if (!db_io_.is_open()) {
        db_io_.clear();
        db_io_.open(db_file_name_, std::ios::binary | std::ios::trunc | std::ios::out | std::ios::in);
        if (!db_io_.is_open()) {
            throw std::runtime_error("Failed to open or create db file: " + db_file_name_);
        }
    }

    // Calculate existing number of pages
    db_io_.seekp(0, std::ios::end);
    std::streampos end_pos = db_io_.tellp();
    num_pages_ = static_cast<page_id_t>(end_pos / PAGE_SIZE);

    // Open/Create log file in binary read/write mode
    log_io_.open(log_file_name_, std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
    if (!log_io_.is_open()) {
        log_io_.clear();
        log_io_.open(log_file_name_, std::ios::binary | std::ios::trunc | std::ios::out | std::ios::in);
        if (!log_io_.is_open()) {
            throw std::runtime_error("Failed to open or create log file: " + log_file_name_);
        }
    }
}

DiskManager::~DiskManager() {
    FlushDbFile();
    FlushLogFile();
    if (db_io_.is_open()) db_io_.close();
    if (log_io_.is_open()) log_io_.close();
}

page_id_t DiskManager::AllocatePage() {
    std::lock_guard<std::mutex> guard(db_io_latch_);
    page_id_t new_page_id = num_pages_++;
    
    // Zero-fill new page on disk
    char blank[PAGE_SIZE];
    std::memset(blank, 0, PAGE_SIZE);
    db_io_.seekp(static_cast<std::streamoff>(new_page_id) * PAGE_SIZE);
    db_io_.write(blank, PAGE_SIZE);
    db_io_.flush();
    num_writes_++;
    return new_page_id;
}

void DiskManager::WritePage(page_id_t page_id, const char* page_data) {
    std::lock_guard<std::mutex> guard(db_io_latch_);
    std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;
    
    db_io_.seekp(offset);
    db_io_.write(page_data, PAGE_SIZE);
    if (db_io_.bad()) {
        throw std::runtime_error("DiskManager: Disk I/O error writing page " + std::to_string(page_id));
    }
    db_io_.flush();
    num_writes_++;
}

void DiskManager::ReadPage(page_id_t page_id, char* page_data) {
    std::lock_guard<std::mutex> guard(db_io_latch_);
    std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;

    db_io_.seekg(offset);
    db_io_.read(page_data, PAGE_SIZE);
    std::streamsize bytes_read = db_io_.gcount();
    if (bytes_read < static_cast<std::streamsize>(PAGE_SIZE)) {
        // Zero-out remainder if reading beyond file length
        std::memset(page_data + bytes_read, 0, PAGE_SIZE - bytes_read);
    }
    num_reads_++;
}

void DiskManager::WriteLog(const char* log_data, size_t size) {
    std::lock_guard<std::mutex> guard(log_io_latch_);
    log_io_.seekp(0, std::ios::end);
    log_io_.write(log_data, size);
    log_io_.flush();
}

bool DiskManager::ReadLog(char* log_data, size_t size, size_t offset) {
    std::lock_guard<std::mutex> guard(log_io_latch_);
    log_io_.seekg(offset);
    log_io_.read(log_data, size);
    std::streamsize bytes_read = log_io_.gcount();
    return bytes_read > 0;
}

void DiskManager::FlushDbFile() {
    std::lock_guard<std::mutex> guard(db_io_latch_);
    if (db_io_.is_open()) db_io_.flush();
}

void DiskManager::FlushLogFile() {
    std::lock_guard<std::mutex> guard(log_io_latch_);
    if (log_io_.is_open()) log_io_.flush();
}

} // namespace nanoengine
