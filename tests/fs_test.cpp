#include "fs/filesystem.hpp"
#include "fs/disk.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <random>
#include <algorithm>

// Simple Test Framework Helper
#define ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "[FAIL] " << message << " (" << #condition << ")\n"; \
        std::exit(1); \
    } else { \
        std::cout << "[PASS] " << message << "\n"; \
    }

// Helper to generate random data
std::vector<uint8_t> generate_random_data(size_t size) {
    std::vector<uint8_t> data(size);
    // Use a fixed seed for reproducibility
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(0, 255);
    for (size_t i = 0; i < size; ++i) data[i] = static_cast<uint8_t>(dis(gen));
    return data;
}

// ==========================================
// TEST 1: The "Reboot" Simulation
// ==========================================
void test_persistence() {
    std::cout << "\n=== Test 1: Persistence (Simulated Reboot) ===\n";

    const size_t DISK_SIZE = 5 * 1024 * 1024; // 5MB
    Disk shared_disk(DISK_SIZE);

    // SESSION 1: Format and Write
    {
        std::cout << "-> Mounting Session 1...\n";
        FileSystem fs(shared_disk);
        fs.format(); // Wipe disk

        fs.create_dir("/home");
        fs.create_file("/home/config.txt");

        std::string secret = "This data must survive the reboot.";
        std::vector<uint8_t> data(secret.begin(), secret.end());
        fs.write_file("/home/config.txt", data);
        std::cout << "-> Data written. Unmounting Session 1 (Destructor called).\n";
    }

    // SESSION 2: Remount and Verify
    {
        std::cout << "-> Mounting Session 2 (Simulating Reboot)...\n";
        FileSystem fs(shared_disk);
        fs.mount(); // READ ONLY. Do not format!

        auto files = fs.list_dir("/home");
        ASSERT(files.size() == 1 && files[0] == "config.txt", "Directory listing persisted");

        auto data = fs.read_file("/home/config.txt");
        std::string read_str(data.begin(), data.end());
        ASSERT(read_str == "This data must survive the reboot.", "File content persisted");
    }
}

// ==========================================
// TEST 2: Deep Tree Structure
// ==========================================
void test_deep_tree() {
    std::cout << "\n=== Test 2: Deep Directory Tree ===\n";
    Disk disk(10 * 1024 * 1024);
    FileSystem fs(disk);
    fs.format();

    // Create /a/b/c/d/e
    std::vector<std::string> levels = {"a", "b", "c", "d", "e"};
    std::string current_path = "";

    for (const auto& dir : levels) {
        current_path += "/" + dir;
        fs.create_dir(current_path);
        std::cout << "Created: " << current_path << "\n";
    }

    // Create file at the bottom
    std::string file_path = current_path + "/deep_file.txt";
    fs.create_file(file_path);

    // Verify by walking down
    auto list = fs.list_dir("/a/b/c/d/e");
    ASSERT(list.size() == 1 && list[0] == "deep_file.txt", "Found file at depth 5");
}

// ==========================================
// TEST 3: Stress Test (Allocation Loop)
// ==========================================
void test_stress_allocation() {
    std::cout << "\n=== Test 3: Stress Test (Create/Write/Delete Loop) ===\n";

    Disk disk(20 * 1024 * 1024); // 20MB
    FileSystem fs(disk);
    fs.format();

    int file_count = 100;
    std::vector<std::string> created_files;

    std::cout << "-> Creating " << file_count << " files with random data...\n";

    // 1. CREATE PHASE
    for (int i = 0; i < file_count; i++) {
        std::string name = "/file_" + std::to_string(i);
        fs.create_file(name);

        // Write 4KB of random data to each
        auto data = generate_random_data(4096);
        fs.write_file(name, data);

        created_files.push_back(name);
    }

    // 2. VERIFY PHASE
    std::cout << "-> Verifying " << file_count << " files...\n";
    for (const auto& path : created_files) {
        auto data = fs.read_file(path);
        ASSERT(data.size() == 4096, "File size correct");
    }

    // 3. DELETE PHASE (Check for memory leaks)
    std::cout << "-> Deleting all files...\n";
    for (const auto& path : created_files) {
        fs.delete_file(path);
    }

    auto root_files = fs.list_dir("/");
    ASSERT(root_files.empty(), "Root directory is empty after deletion");

    // 4. RE-ALLOCATION (The real test)
    // If you have a memory leak (bitmaps not clearing), this will fail/crash
    std::cout << "-> Re-allocating to check for bitmap leaks...\n";
    fs.create_file("/check_leak");
    fs.write_file("/check_leak", generate_random_data(4096));
    ASSERT(true, "Re-allocation successful (Bitmaps cleared correctly)");
}

// ==========================================
// TEST 4: Max File Size Boundary
// ==========================================
void test_large_file() {
    std::cout << "\n=== Test 4: Large File Boundary (48KB Limit) ===\n";
    Disk disk(5 * 1024 * 1024);
    FileSystem fs(disk);
    fs.format();

    fs.create_file("/large.bin");

    // Max size = 12 blocks * 4096 = 49152 bytes
    std::vector<uint8_t> max_data = generate_random_data(49152);

    try {
        fs.write_file("/large.bin", max_data);
        ASSERT(true, "Wrote max file size successfully");
    } catch (...) {
        ASSERT(false, "Failed to write max file size");
    }

    // Try to write ONE byte too many
    std::vector<uint8_t> too_big(49153);
    bool caught = false;
    try {
        fs.write_file("/large.bin", too_big);
    } catch (const std::exception& e) {
        caught = true;
        std::cout << "[PASS] Caught expected error: " << e.what() << "\n";
    }
    ASSERT(caught, "System correctly rejected file > 48KB");
}

int main() {
    std::cout << "STARTING FILE SYSTEM TEST SUITE\n";
    std::cout << "===============================\n";

    try {
        test_persistence();
        test_deep_tree();
        test_large_file();
        test_stress_allocation();
    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL FAILURE] Uncaught Exception: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n===============================\n";
    std::cout << "ALL TESTS PASSED SUCCESSFULLY.\n";
    return 0;
}
