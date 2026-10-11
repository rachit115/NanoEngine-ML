#include "storage/disk_manager.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

using nanoengine::storage::DiskManager;
using nanoengine::storage::Page;

int main() {
    const std::string path = "test_data/storage_test.db";
    std::filesystem::remove(path);

    {
        DiskManager disk(path);
        Page page;
        const std::string message = "Hello NanoEngine storage!";
        std::memcpy(page.data(), message.c_str(), message.size() + 1);
        disk.write_page(0, page);

        Page read_back;
        const bool read_ok = disk.read_page(0, read_back);
        assert(read_ok);
        assert(std::strcmp(read_back.data(), message.c_str()) == 0);

        Page missing;
        assert(!disk.read_page(1, missing));
    }

    std::filesystem::remove(path);
    std::cout << "DiskManager tests passed.\n";
    return 0;
}
