cmake_minimum_required(VERSION 3.20)
project(NanoEngineStorage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nanoengine_storage
    src/storage/page.cpp
    src/storage/disk_manager.cpp
    src/storage/slotted_page.cpp
)

target_include_directories(nanoengine_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(nanoengine_storage PRIVATE /W4)
else()
    target_compile_options(nanoengine_storage PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(storage_test tests/storage_test.cpp)
    target_link_libraries(storage_test PRIVATE nanoengine_storage)
    add_test(NAME storage_test COMMAND storage_test)

    add_executable(slotted_page_test tests/slotted_page_test.cpp)
    target_link_libraries(slotted_page_test PRIVATE nanoengine_storage)
    add_test(NAME slotted_page_test COMMAND slotted_page_test)
endif()
