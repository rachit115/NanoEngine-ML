# NanoEngine — Storage Layer

This repository contains **only the storage-layer contribution** for NanoEngine: fixed-size pages, binary disk I/O, a basic slotted-page layout, and focused tests. It is designed as a small C++20 module that can later be integrated into the team's larger NanoEngine repository.

## Scope

### Implemented in this module

- **`Page`** — a zero-initialized, fixed-size 4 KiB byte buffer.
- **`DiskManager`** — reads and writes exactly one 4 KiB page at `page_id * PAGE_SIZE` in a binary database file. It creates the parent directory/file if needed and serializes access to its file stream with a mutex.
- **`SlottedPage`** — stores a small header and slot directory at the beginning of a page while tuple bytes grow backward from the end. Supports initialization, insert, read, delete/mark-empty, slot reuse, and free-space inspection.
- **Tests** — checks basic page I/O, missing-page behavior, slotted-page insert/read/delete/slot reuse, and persistence of slotted-page bytes through the disk manager.

### Not implemented here

This module does **not** implement the buffer pool, LRU or learned eviction, B+ Tree, SQL/query execution, Strict 2PL, WAL/ARIES recovery, or a complete database. Those belong to other NanoEngine modules.

## Project structure

```text
NanoEngine-Storage/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── include/
│   └── storage/
│       ├── page.h
│       ├── disk_manager.h
│       └── slotted_page.h
├── src/
│   └── storage/
│       ├── page.cpp
│       ├── disk_manager.cpp
│       └── slotted_page.cpp
└── tests/
    ├── storage_test.cpp
    └── slotted_page_test.cpp
```

## Requirements

- CMake 3.20 or newer
- A C++20 compiler (GCC, Clang, or a recent MSVC)

No third-party libraries are required.

## Build and run tests

From the repository root:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

You can also run the test executables directly after building:

```bash
./build/storage_test
./build/slotted_page_test
```

On Windows, executables are typically under `build/Debug/` or `build/Release/`, depending on the generator.

## Design notes

### Page addressing

Every page is exactly 4096 bytes. The disk offset is calculated as:

```text
offset = page_id * 4096
```

This assumes pages are stored consecutively in a single database file. `read_page` returns `false` if a full page is not available. `write_page` throws a `std::runtime_error` if the write fails.

### Slotted-page layout

The current on-page layout is:

```text
+-------------------------------+ offset 0
| Magic: "SLP1" (4 bytes)       |
| Slot count (2 bytes)          |
| Free-space start (2 bytes)    |
| Free-space end (2 bytes)      |
+-------------------------------+ offset 10
| Slot directory (4 bytes/slot) |
|       free space              |
|       tuple bytes             |
+-------------------------------+ offset 4096
```

Each slot stores a 16-bit tuple offset and a 16-bit tuple length. Header integers are encoded little-endian. Slot IDs are zero-based. Deletion marks a slot empty; insertion can reuse that slot, but this initial implementation does not compact tuple bytes or reclaim the deleted tuple's hole. `free_space()` reports the contiguous gap between the directory and tuple area, not all fragmented free bytes.

## Important limitations

- This is an educational milestone, not a production storage engine.
- There is no checksum, page versioning, free-page map, page allocation metadata, or crash-recovery protocol.
- `flush()` on the C++ stream is not by itself a guarantee of durability against every power-loss scenario.
- The slotted-page format is a small project-specific format and may need to be aligned with the team's final shared page format before integration.
- The `DiskManager` mutex protects concurrent calls made through one `DiskManager` instance; it does not coordinate separate instances opening the same file.

## Suggested Git commit

```text
feat(storage): add page, disk manager, and slotted-page primitives
```
