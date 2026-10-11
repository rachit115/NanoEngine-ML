#include "storage/slotted_page.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace nanoengine::storage {
namespace {
constexpr char kMagic[4] = {'S', 'L', 'P', '1'};
constexpr std::size_t kSlotCountOffset = 4;
constexpr std::size_t kFreeStartOffset = 6;
constexpr std::size_t kFreeEndOffset = 8;
constexpr std::size_t kSlotOffsetOffset = 0;
constexpr std::size_t kSlotLengthOffset = 2;
}  // namespace

std::uint16_t SlottedPage::read_u16(const char* bytes) noexcept {
    const auto low = static_cast<std::uint8_t>(bytes[0]);
    const auto high = static_cast<std::uint8_t>(bytes[1]);
    return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
}

void SlottedPage::write_u16(char* bytes, std::uint16_t value) noexcept {
    bytes[0] = static_cast<char>(value & 0xFF);
    bytes[1] = static_cast<char>((value >> 8) & 0xFF);
}

std::size_t SlottedPage::slot_entry_offset(slot_id_t slot_id) noexcept {
    return HEADER_SIZE + static_cast<std::size_t>(slot_id) * SLOT_SIZE;
}

void SlottedPage::initialize(Page& page) {
    std::memset(page.data(), 0, Page::PAGE_SIZE);
    std::memcpy(page.data(), kMagic, sizeof(kMagic));
    write_u16(page.data() + kSlotCountOffset, 0);
    write_u16(page.data() + kFreeStartOffset, static_cast<std::uint16_t>(HEADER_SIZE));
    write_u16(page.data() + kFreeEndOffset, static_cast<std::uint16_t>(Page::PAGE_SIZE));
}

bool SlottedPage::is_initialized(const Page& page) noexcept {
    return std::memcmp(page.data(), kMagic, sizeof(kMagic)) == 0;
}

bool SlottedPage::valid_layout(const Page& page) noexcept {
    if (!is_initialized(page)) {
        return false;
    }
    const auto count = read_u16(page.data() + kSlotCountOffset);
    const auto free_start = read_u16(page.data() + kFreeStartOffset);
    const auto free_end = read_u16(page.data() + kFreeEndOffset);
    const auto expected_start = HEADER_SIZE + static_cast<std::size_t>(count) * SLOT_SIZE;
    return expected_start == free_start && free_start <= free_end &&
           free_end <= Page::PAGE_SIZE;
}

std::optional<slot_id_t> SlottedPage::insert(Page& page, std::string_view tuple) {
    if (!valid_layout(page) || tuple.empty() ||
        tuple.size() > std::numeric_limits<std::uint16_t>::max()) {
        return std::nullopt;
    }

    const auto count = read_u16(page.data() + kSlotCountOffset);
    const auto free_start = read_u16(page.data() + kFreeStartOffset);
    const auto free_end = read_u16(page.data() + kFreeEndOffset);

    std::optional<slot_id_t> reusable_slot;
    for (std::uint16_t i = 0; i < count; ++i) {
        const auto entry = slot_entry_offset(static_cast<slot_id_t>(i));
        if (read_u16(page.data() + entry + kSlotLengthOffset) == 0) {
            reusable_slot = static_cast<slot_id_t>(i);
            break;
        }
    }

    const std::size_t directory_cost = reusable_slot ? 0 : SLOT_SIZE;
    if (free_end < free_start ||
        tuple.size() + directory_cost > static_cast<std::size_t>(free_end - free_start)) {
        return std::nullopt;
    }
    if (!reusable_slot && count == std::numeric_limits<slot_id_t>::max()) {
        return std::nullopt;
    }

    const auto new_tuple_offset = static_cast<std::uint16_t>(free_end - tuple.size());
    std::memcpy(page.data() + new_tuple_offset, tuple.data(), tuple.size());

    const slot_id_t slot_id = reusable_slot.value_or(static_cast<slot_id_t>(count));
    const auto entry = slot_entry_offset(slot_id);
    write_u16(page.data() + entry + kSlotOffsetOffset, new_tuple_offset);
    write_u16(page.data() + entry + kSlotLengthOffset,
              static_cast<std::uint16_t>(tuple.size()));

    if (!reusable_slot) {
        write_u16(page.data() + kSlotCountOffset, static_cast<std::uint16_t>(count + 1));
        write_u16(page.data() + kFreeStartOffset,
                  static_cast<std::uint16_t>(free_start + SLOT_SIZE));
    }
    write_u16(page.data() + kFreeEndOffset, new_tuple_offset);
    return slot_id;
}

std::optional<std::string> SlottedPage::get(const Page& page, slot_id_t slot_id) {
    if (!valid_layout(page)) {
        return std::nullopt;
    }
    const auto count = read_u16(page.data() + kSlotCountOffset);
    if (slot_id >= count) {
        return std::nullopt;
    }

    const auto entry = slot_entry_offset(slot_id);
    const auto offset = read_u16(page.data() + entry + kSlotOffsetOffset);
    const auto length = read_u16(page.data() + entry + kSlotLengthOffset);
    if (length == 0 || offset < read_u16(page.data() + kFreeEndOffset) ||
        static_cast<std::size_t>(offset) + length > Page::PAGE_SIZE) {
        return std::nullopt;
    }
    return std::string(page.data() + offset, length);
}

bool SlottedPage::erase(Page& page, slot_id_t slot_id) {
    if (!valid_layout(page)) {
        return false;
    }
    const auto count = read_u16(page.data() + kSlotCountOffset);
    if (slot_id >= count) {
        return false;
    }
    const auto entry = slot_entry_offset(slot_id);
    if (read_u16(page.data() + entry + kSlotLengthOffset) == 0) {
        return false;
    }
    write_u16(page.data() + entry + kSlotOffsetOffset, 0);
    write_u16(page.data() + entry + kSlotLengthOffset, 0);
    return true;
}

std::size_t SlottedPage::free_space(const Page& page) {
    if (!valid_layout(page)) {
        throw std::invalid_argument("Page is not an initialized, valid slotted page");
    }
    const auto free_start = read_u16(page.data() + kFreeStartOffset);
    const auto free_end = read_u16(page.data() + kFreeEndOffset);
    return static_cast<std::size_t>(free_end - free_start);
}

std::uint16_t SlottedPage::slot_count(const Page& page) {
    if (!valid_layout(page)) {
        throw std::invalid_argument("Page is not an initialized, valid slotted page");
    }
    return read_u16(page.data() + kSlotCountOffset);
}

}  // namespace nanoengine::storage
