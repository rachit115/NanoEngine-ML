#pragma once

#include <cstdint>
#include <cstddef>

namespace nanoengine {

// Standard database storage page size: 4 KB
static constexpr size_t PAGE_SIZE = 4096;

// Invalid identifiers
static constexpr int32_t INVALID_PAGE_ID = -1;
static constexpr int32_t INVALID_FRAME_ID = -1;
static constexpr int64_t INVALID_LSN = -1;
static constexpr int32_t INVALID_TXN_ID = -1;

// Default Buffer Pool sizing
static constexpr size_t DEFAULT_BUFFER_POOL_SIZE = 10;

// Log buffer size (32 KB)
static constexpr size_t LOG_BUFFER_SIZE = 32 * 1024;

using page_id_t = int32_t;
using frame_id_t = int32_t;
using lsn_t = int64_t;
using txn_id_t = int32_t;
using rid_t = int64_t;

} // namespace nanoengine
