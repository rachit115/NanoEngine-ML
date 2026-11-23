cmake_minimum_required(VERSION 3.20)
project(NanoEngine-ML VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX- /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Core engine library
set(ENGINE_SOURCES
    src/storage/disk_manager.cpp
    src/buffer/lru_replacer.cpp
    src/buffer/clock_replacer.cpp
    src/buffer/learned_replacer.cpp
    src/buffer/buffer_pool_manager.cpp
    src/concurrency/lock_manager.cpp
    src/concurrency/transaction.cpp
    src/recovery/log_manager.cpp
    src/recovery/aries_recovery.cpp
    src/index/b_plus_tree.cpp
    src/engine/nano_engine.cpp
)

add_library(nanoengine_lib STATIC ${ENGINE_SOURCES})

# Presentation / Demo Apps
add_executable(cli_demo apps/cli_demo.cpp)
target_link_libraries(cli_demo PRIVATE nanoengine_lib)

add_executable(benchmark apps/benchmark.cpp)
target_link_libraries(benchmark PRIVATE nanoengine_lib)

# Testing
enable_testing()

function(add_nano_test TEST_NAME TEST_SRC)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} PRIVATE nanoengine_lib)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

add_nano_test(test_disk_manager tests/test_disk_manager.cpp)
add_nano_test(test_buffer_pool tests/test_buffer_pool.cpp)
add_nano_test(test_btree_concurrent tests/test_btree_concurrent.cpp)
add_nano_test(test_lock_manager tests/test_lock_manager.cpp)
add_nano_test(test_recovery tests/test_recovery.cpp)
