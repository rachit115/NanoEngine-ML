#include "storage/page.h"

#include <cstring>

namespace nanoengine::storage {

Page::Page() noexcept {
    std::memset(data_, 0, PAGE_SIZE);
}

char* Page::data() noexcept {
    return data_;
}

const char* Page::data() const noexcept {
    return data_;
}

}  // namespace nanoengine::storage
