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

    # Comprehensive test suite
    set(TEST_FILES
        disk_test
        fs_operations_test
        fs_directory_test
        fs_permissions_test
        fs_persistence_test
        fs_stress_test
    )

    foreach(test_name ${TEST_FILES})
        set(test_source "tests/${test_name}.cpp")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_source}")
            add_executable(${test_name} ${test_source})
            target_link_libraries(${test_name} PRIVATE fs_core)
            add_test(NAME ${test_name} COMMAND ${test_name})
        endif()
    endforeach()

    # Legacy test files (maintained for compatibility)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/fs_test.cpp")
        add_executable(fs_test tests/fs_test.cpp)
        target_link_libraries(fs_test PRIVATE fs_core)
        add_test(NAME LegacyFsTest COMMAND fs_test)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/permission_test.cpp")
        add_executable(permission_test tests/permission_test.cpp)
        target_link_libraries(permission_test PRIVATE fs_core)
        add_test(NAME LegacyPermissionTest COMMAND permission_test)
    endif()

    # Custom Target: 'make check' will build AND run all tests
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMENT "Running All File System Tests..."
        USES_TERMINAL
    )

    # Custom target to run comprehensive test suite
    add_custom_target(check-comprehensive
        COMMAND ${CMAKE_CTEST_COMMAND} -R "disk_test|fs_operations_test|fs_directory_test|fs_permissions_test|fs_persistence_test|fs_stress_test" --output-on-failure
        COMMENT "Running Comprehensive Test Suite..."
        USES_TERMINAL
    )
endif()
