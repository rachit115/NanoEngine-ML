#include "storage/disk_manager.h"

#include <filesystem>
#include <limits>
#include <stdexcept>
#include <utility>

namespace nanoengine::storage {

DiskManager::DiskManager(std::string file_name)
    : file_name_(std::move(file_name)) {
    open_or_create_file();
}

DiskManager::~DiskManager() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void DiskManager::open_or_create_file() {
    const std::filesystem::path path(file_name_);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    file_.open(file_name_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        // std::fstream does not create a missing file when opened with both
        // std::ios::in and std::ios::out, so create it first.
        std::ofstream create_file(file_name_, std::ios::out | std::ios::binary);
        if (!create_file) {
            throw std::runtime_error("Could not create database file: " + file_name_);
        }
        create_file.close();
        file_.clear();
        file_.open(file_name_, std::ios::in | std::ios::out | std::ios::binary);
    }

    if (!file_.is_open()) {
        throw std::runtime_error("Could not open database file: " + file_name_);
    }
}

std::streamoff DiskManager::page_offset(page_id_t page_id) const {
    constexpr auto max_offset = std::numeric_limits<std::streamoff>::max();
    if (page_id > static_cast<page_id_t>(max_offset / Page::PAGE_SIZE)) {
        throw std::overflow_error("Page ID is too large to map to a file offset");
    }
    return static_cast<std::streamoff>(page_id * Page::PAGE_SIZE);
}

bool DiskManager::read_page(page_id_t page_id, Page& page) {
    std::lock_guard<std::mutex> lock(file_mutex_);
    file_.clear();
    file_.seekg(page_offset(page_id), std::ios::beg);
    if (!file_) {
        file_.clear();
        return false;
    }

    file_.read(page.data(), static_cast<std::streamsize>(Page::PAGE_SIZE));
    const bool complete_page =
        file_.gcount() == static_cast<std::streamsize>(Page::PAGE_SIZE);
    if (!complete_page) {
        file_.clear();
    }
    return complete_page;
}

void DiskManager::write_page(page_id_t page_id, const Page& page) {
    std::lock_guard<std::mutex> lock(file_mutex_);
    file_.clear();
    file_.seekp(page_offset(page_id), std::ios::beg);
    if (!file_) {
        throw std::runtime_error("Could not seek to requested page in database file");
    }

    file_.write(page.data(), static_cast<std::streamsize>(Page::PAGE_SIZE));
    file_.flush();
    if (!file_) {
        throw std::runtime_error("Failed to write database page to: " + file_name_);
    }
}

const std::string& DiskManager::file_name() const noexcept {
    return file_name_;
}

}  // namespace nanoengine::storage
