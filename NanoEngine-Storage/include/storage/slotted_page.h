#pragma once

#include "storage/page.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nanoengine::storage {

using slot_id_t = std::uint16_t;

// A compact slotted-page layout stored inside a 4 KiB Page.
// Header: magic(4), slot_count(2), free_start(2), free_end(2).
// Slot entry: tuple_offset(2), tuple_length(2). Tuple bytes grow backwards
// from the end of the page while the slot directory grows forwards.
class SlottedPage {
public:
    static constexpr std::size_t HEADER_SIZE = 10;
    static constexpr std::size_t SLOT_SIZE = 4;

    static void initialize(Page& page);
    static bool is_initialized(const Page& page) noexcept;

    // Returns the slot ID, or nullopt if the tuple cannot fit.
    static std::optional<slot_id_t> insert(Page& page, std::string_view tuple);

    // Returns nullopt for an invalid or deleted slot.
    static std::optional<std::string> get(const Page& page, slot_id_t slot_id);

    // Marks a slot deleted. Tuple bytes are not compacted/reclaimed by erase.
    static bool erase(Page& page, slot_id_t slot_id);

    // Number of bytes between the slot directory and tuple area.
    static std::size_t free_space(const Page& page);
    static std::uint16_t slot_count(const Page& page);

private:
    static std::uint16_t read_u16(const char* bytes) noexcept;
    static void write_u16(char* bytes, std::uint16_t value) noexcept;
    static std::size_t slot_entry_offset(slot_id_t slot_id) noexcept;
    static bool valid_layout(const Page& page) noexcept;
};

}  // namespace nanoengine::storage
