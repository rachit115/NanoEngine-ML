#include "storage/disk_manager.h"
#include "storage/slotted_page.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

using nanoengine::storage::DiskManager;
using nanoengine::storage::Page;
using nanoengine::storage::SlottedPage;

int main() {
    Page page;
    SlottedPage::initialize(page);
    assert(SlottedPage::is_initialized(page));
    assert(SlottedPage::slot_count(page) == 0);

    const auto first = SlottedPage::insert(page, "student:101:Aman:85");
    const auto second = SlottedPage::insert(page, "student:102:Riya:92");
    assert(first.has_value() && second.has_value());
    assert(*first == 0 && *second == 1);
    assert(SlottedPage::get(page, *first).value() == "student:101:Aman:85");
    assert(SlottedPage::get(page, *second).value() == "student:102:Riya:92");

    assert(SlottedPage::erase(page, *first));
    assert(!SlottedPage::get(page, *first).has_value());
    const auto replacement = SlottedPage::insert(page, "student:103:Karan:78");
    assert(replacement.has_value() && *replacement == *first);
    assert(SlottedPage::get(page, *replacement).value() == "student:103:Karan:78");

    const std::string path = "test_data/slotted_page_test.db";
    std::filesystem::remove(path);
    {
        DiskManager disk(path);
        disk.write_page(0, page);
        Page loaded;
        assert(disk.read_page(0, loaded));
        assert(SlottedPage::get(loaded, *second).value() == "student:102:Riya:92");
        assert(SlottedPage::get(loaded, *replacement).value() == "student:103:Karan:78");
    }
    std::filesystem::remove(path);

    std::cout << "SlottedPage tests passed.\n";
    return 0;
}
