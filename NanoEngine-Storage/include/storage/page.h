#pragma once

#include <cstddef>

namespace nanoengine::storage {

// A fixed-size database page. The page is intentionally just raw bytes;
// higher-level code decides how those bytes are structured.
class Page {
public:
    static constexpr std::size_t PAGE_SIZE = 4096;

    Page() noexcept;

    char* data() noexcept;
    const char* data() const noexcept;

private:
    char data_[PAGE_SIZE];
};

}  // namespace nanoengine::storage
