cmake_minimum_required(VERSION 3.10)
project(fs_sim)

# Modern C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==========================================
# 1. CORE LIBRARY
# ==========================================
file(GLOB_RECURSE LIB_SOURCES "src/fs/*.cpp" "src/util/*.cpp")
add_library(fs_core ${LIB_SOURCES})
target_include_directories(fs_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ==========================================
# 2. MAIN APP
# ==========================================
add_executable(fs_sim src/main.cpp)
target_link_libraries(fs_sim PRIVATE fs_core)

# ==========================================
# 3. TESTS (Behind a Flag)
# ==========================================
# Default is ON. You can run 'cmake -DBUILD_TESTS=OFF ..' to disable them.
option(BUILD_TESTS "Build the test suite" ON)

if(BUILD_TESTS)
    enable_testing()
    message(STATUS "Test Suite: ENABLED")

    # Check if test file exists
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/fs_test.cpp")
        add_executable(fs_test tests/fs_test.cpp)
        target_link_libraries(fs_test PRIVATE fs_core)

        # Standard CTest integration
        add_test(NAME FullSystemTest COMMAND fs_test)

        # Custom Target: 'make check' will build AND run the tests immediately
        # This is the "Magic Command" you are looking for.
        add_custom_target(check
            COMMAND fs_test
            DEPENDS fs_test
            COMMENT "Running File System Tests..."
            USES_TERMINAL
        )
    endif()
endif()
