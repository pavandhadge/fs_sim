cmake_minimum_required(VERSION 3.10)
project(fs_sim)

# Modern C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generates compile_commands.json for Zed automatically
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==========================================
# 1. CORE LIBRARY (The "Brain")
# ==========================================
# We collect all source files EXCEPT main.cpp.
# This prevents "Multiple Definition of Main" errors when testing.
file(GLOB_RECURSE LIB_SOURCES
    "src/fs/*.cpp"
    "src/util/*.cpp"
)

# Create a library named 'fs_core'
add_library(fs_core ${LIB_SOURCES})

# PUBLIC means: Anyone linking 'fs_core' also gets the 'include' path.
target_include_directories(fs_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ==========================================
# 2. MAIN APP (The CLI)
# ==========================================
# This is your actual runnable program.
add_executable(fs_sim src/main.cpp)

# Link it to your core library
target_link_libraries(fs_sim PRIVATE fs_core)

# ==========================================
# 3. TESTS
# ==========================================
enable_testing()

# Test 1: FS Logic
add_executable(fs_test tests/fs_test.cpp)
target_link_libraries(fs_test PRIVATE fs_core)
add_test(NAME FileSystemTest COMMAND fs_test)

# Test 2: Path Logic
add_executable(path_test tests/path_test.cpp)
target_link_libraries(path_test PRIVATE fs_core)
add_test(NAME PathTest COMMAND path_test)
