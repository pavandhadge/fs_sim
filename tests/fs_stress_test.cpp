#include "fs/filesystem.hpp"
#include "fs/disk.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <random>
#include <chrono>

#define ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "[FAIL] " << message << " (" << #condition << ")\n"; \
        std::exit(1); \
    } else { \
        std::cout << "[PASS] " << message << "\n"; \
    }

#define ASSERT_THROWS(code, message) \
    { \
        bool caught = false; \
        try { code; } \
        catch (const std::exception&) { caught = true; } \
        if (!caught) { \
            std::cerr << "[FAIL] " << message << " (Expected exception but none thrown)\n"; \
            std::exit(1); \
        } else { \
            std::cout << "[PASS] " << message << "\n"; \
        } \
    }

void cleanup_file(const char* filename) {
    std::remove(filename);
}

std::vector<uint8_t> generate_random_data(size_t size, int seed) {
    std::vector<uint8_t> data(size);
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(0, 255);
    for (size_t i = 0; i < size; ++i) data[i] = static_cast<uint8_t>(dis(gen));
    return data;
}

// ==========================================
// STRESS TESTS
// ==========================================
void test_allocation_loop() {
    std::cout << "\n=== Stress Tests: Create/Write/Delete Loop ===\n";
    const char* TEST_IMG = "test_stress_loop.img";
    cleanup_file(TEST_IMG);

    Disk disk(32 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    const int iterations = 100;
    std::cout << "  -> Running " << iterations << " create/write/delete cycles...\n";

    for (int i = 0; i < iterations; i++) {
        std::string name = "/file" + std::to_string(i) + ".txt";
        fs.create_file(name);
        
        auto data = generate_random_data(4096, i);
        fs.write_file(name, data);
        
        auto read = fs.read_file(name);
        ASSERT(read == data, "Cycle " + std::to_string(i) + ": Data integrity");
        
        fs.delete_file(name);
    }

    ASSERT(fs.list_dir("/").empty(), "All files deleted, root empty");

    // Re-allocate to check for leaks
    fs.create_file("/final.txt");
    auto data = generate_random_data(4096, 999);
    fs.write_file("/final.txt", data);
    auto read = fs.read_file("/final.txt");
    ASSERT(read == data, "Post-cycle allocation works (no bitmap leaks)");

    cleanup_file(TEST_IMG);
}

void test_mass_file_creation() {
    std::cout << "\n=== Stress Tests: Mass File Creation ===\n";
    const char* TEST_IMG = "test_stress_mass.img";
    cleanup_file(TEST_IMG);

    Disk disk(64 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    const int file_count = 150;  // Max ~180 entries per directory (12 blocks * 4096 / 264 bytes)
    std::cout << "  -> Creating " << file_count << " files...\n";

    for (int i = 0; i < file_count; i++) {
        std::string name = "/f" + std::to_string(i);
        fs.create_file(name);
    }

    auto entries = fs.list_dir("/");
    ASSERT(entries.size() == file_count, "All files created");

    // Delete half
    std::cout << "  -> Deleting half the files...\n";
    for (int i = 0; i < file_count / 2; i++) {
        std::string name = "/f" + std::to_string(i);
        fs.delete_file(name);
    }

    entries = fs.list_dir("/");
    ASSERT(entries.size() == file_count / 2, "Half the files deleted");

    cleanup_file(TEST_IMG);
}

void test_disk_full_scenarios() {
    std::cout << "\n=== Stress Tests: Disk Full Scenarios ===\n";
    const char* TEST_IMG = "test_stress_full.img";
    cleanup_file(TEST_IMG);

    // Small disk to fill up quickly
    Disk disk(2 * 1024 * 1024, TEST_IMG);  // 2MB = 512 blocks
    FileSystem fs(disk);
    fs.format();

    // Fill up with files
    int count = 0;
    bool full = false;
    while (!full) {
        try {
            std::string name = "/f" + std::to_string(count);
            fs.create_file(name);
            fs.write_file(name, generate_random_data(4096, count));
            count++;
        } catch (const std::runtime_error& e) {
            full = true;
            std::cout << "  -> Disk full after " << count << " files\n";
        }
    }

    ASSERT(count > 0, "Disk filled up successfully");

    // Delete all to free space
    for (int i = 0; i < count; i++) {
        std::string name = "/f" + std::to_string(i);
        fs.delete_file(name);
    }

    ASSERT(fs.list_dir("/").empty(), "All files deleted after filling disk");

    // Should be able to create new file
    fs.create_file("/newfile.txt");
    fs.write_file("/newfile.txt", {'a', 'f', 't', 'e', 'r', ' ', 'f', 'u', 'l', 'l'});
    auto data = fs.read_file("/newfile.txt");
    ASSERT(data.size() == 10, "Can create file after freeing space");

    cleanup_file(TEST_IMG);
}

void test_multiple_block_groups() {
    std::cout << "\n=== Stress Tests: Multiple Block Groups ===\n";
    const char* TEST_IMG = "test_stress_groups.img";
    cleanup_file(TEST_IMG);

    // Large disk that will create multiple block groups
    Disk disk(128 * 1024 * 1024, TEST_IMG);  // 128MB
    FileSystem fs(disk);
    fs.format();

    // Create many files across multiple subdirectories to use multiple block groups
    // (root directory limited to ~180 entries, so use subdirectories)
    const int dirs = 5;
    const int files_per_dir = 100;
    const int total_files = dirs * files_per_dir;
    std::cout << "  -> Creating " << total_files << " files across " << dirs << " subdirectories...\n";

    for (int d = 0; d < dirs; d++) {
        std::string dir_name = "/dir" + std::to_string(d);
        fs.create_dir(dir_name);
        for (int f = 0; f < files_per_dir; f++) {
            std::string file_name = dir_name + "/file" + std::to_string(f);
            fs.create_file(file_name);
            fs.write_file(file_name, generate_random_data(1024, d * files_per_dir + f));
        }
    }

    // Verify all
    for (int d = 0; d < dirs; d++) {
        std::string dir_name = "/dir" + std::to_string(d);
        for (int f = 0; f < files_per_dir; f++) {
            std::string file_name = dir_name + "/file" + std::to_string(f);
            auto expected = generate_random_data(1024, d * files_per_dir + f);
            auto actual = fs.read_file(file_name);
            int idx = d * files_per_dir + f;
            ASSERT(actual == expected, "File " + std::to_string(idx) + " data correct");
        }
    }

    // Delete all
    for (int d = 0; d < dirs; d++) {
        std::string dir_name = "/dir" + std::to_string(d);
        fs.delete_dir(dir_name);
    }

    ASSERT(fs.list_dir("/").empty(), "All directories deleted from multiple block groups");

    cleanup_file(TEST_IMG);
}

// ==========================================
// EDGE CASE TESTS
// ==========================================
void test_empty_operations() {
    std::cout << "\n=== Edge Case Tests: Empty Operations ===\n";
    const char* TEST_IMG = "test_edge_empty.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Empty file
    fs.create_file("/empty.txt");
    auto data = fs.read_file("/empty.txt");
    ASSERT(data.empty(), "Empty file returns empty vector");

    // Empty directory
    fs.create_dir("/emptydir");
    auto entries = fs.list_dir("/emptydir");
    ASSERT(entries.empty(), "Empty directory returns empty listing");

    // List empty root
    entries = fs.list_dir("/");
    ASSERT(entries.size() == 2, "Root has 2 entries (empty.txt and emptydir)");

    cleanup_file(TEST_IMG);
}

void test_boundary_conditions() {
    std::cout << "\n=== Edge Case Tests: Boundary Conditions ===\n";
    const char* TEST_IMG = "test_edge_boundaries.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // File at exact block boundary (4096 bytes)
    fs.create_file("/exact_block.txt");
    fs.write_file("/exact_block.txt", generate_random_data(4096, 1));
    auto data = fs.read_file("/exact_block.txt");
    ASSERT(data.size() == 4096, "Exact block size file");

    // File at max boundary (49152 bytes = 12 blocks)
    fs.create_file("/max_file.txt");
    fs.write_file("/max_file.txt", generate_random_data(49152, 2));
    data = fs.read_file("/max_file.txt");
    ASSERT(data.size() == 49152, "Max size file (48KB)");

    // One byte over max
    std::vector<uint8_t> too_big(49153);
    ASSERT_THROWS(fs.write_file("/max_file.txt", too_big), "Cannot write 49153 bytes");

    cleanup_file(TEST_IMG);
}

void test_special_filenames() {
    std::cout << "\n=== Edge Case Tests: Filename Variations ===\n";
    const char* TEST_IMG = "test_edge_names.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Various filenames
    fs.create_file("/a");
    fs.create_file("/long_filename_12345.txt");
    fs.create_file("/with.dots.multiple.txt");
    fs.create_file("/UPPERCASE.TXT");
    fs.create_file("/MiXeD_CaSe.TxT");

    auto entries = fs.list_dir("/");
    ASSERT(entries.size() == 5, "All special filenames created");

    // Write and read
    fs.write_file("/a", {'s', 'h', 'o', 'r', 't'});
    fs.write_file("/long_filename_12345.txt", {'l', 'o', 'n', 'g'});

    auto a = fs.read_file("/a");
    ASSERT(a.size() == 5, "Short filename works");

    auto longf = fs.read_file("/long_filename_12345.txt");
    ASSERT(longf.size() == 4, "Long filename works");

    cleanup_file(TEST_IMG);
}

void test_consecutive_operations() {
    std::cout << "\n=== Edge Case Tests: Consecutive Operations ===\n";
    const char* TEST_IMG = "test_edge_consecutive.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Rapid create-delete cycles
    for (int i = 0; i < 50; i++) {
        fs.create_file("/temp.txt");
        fs.write_file("/temp.txt", {'i', 't', 'e', 'r', static_cast<uint8_t>('0' + (i % 10))});
        fs.delete_file("/temp.txt");
    }

    ASSERT(fs.list_dir("/").empty(), "Rapid create-delete leaves no traces");

    // Overwrite same file many times
    fs.create_file("/overwritten.txt");
    for (int i = 0; i < 20; i++) {
        fs.write_file("/overwritten.txt", generate_random_data(1024 * (i % 12 + 1), i));
    }
    
    // Verify final content
    auto expected = generate_random_data(1024 * (19 % 12 + 1), 19);
    auto actual = fs.read_file("/overwritten.txt");
    ASSERT(actual == expected, "Multiple overwrites preserve final data");

    cleanup_file(TEST_IMG);
}

void test_deep_nesting() {
    std::cout << "\n=== Edge Case Tests: Deep Nesting ===\n";
    const char* TEST_IMG = "test_edge_deep.img";
    cleanup_file(TEST_IMG);

    Disk disk(32 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Create deep path
    std::string path = "";
    int depth = 15;
    for (int i = 0; i < depth; i++) {
        path += "/d" + std::to_string(i);
        fs.create_dir(path);
    }

    // Create file at deepest level
    fs.create_file(path + "/bottom.txt");
    fs.write_file(path + "/bottom.txt", {'d', 'e', 'e', 'p'});

    // Read from deep level
    auto data = fs.read_file(path + "/bottom.txt");
    ASSERT(data.size() == 4, "File at depth " + std::to_string(depth) + " accessible");

    // Delete recursively
    fs.delete_dir("/d0");
    ASSERT(fs.list_dir("/").empty(), "Deep tree deleted recursively");

    cleanup_file(TEST_IMG);
}

// ==========================================
// MAIN
// ==========================================
int main() {
    std::cout << "STARTING STRESS AND EDGE CASE TEST SUITE\n";
    std::cout << "========================================\n";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        test_allocation_loop();
        test_mass_file_creation();
        test_disk_full_scenarios();
        test_multiple_block_groups();
        test_empty_operations();
        test_boundary_conditions();
        test_special_filenames();
        test_consecutive_operations();
        test_deep_nesting();
    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL FAILURE] Uncaught Exception: " << e.what() << "\n";
        return 1;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    std::cout << "\n========================================\n";
    std::cout << "ALL STRESS AND EDGE CASE TESTS PASSED.\n";
    std::cout << "Total time: " << duration.count() << " seconds\n";
    return 0;
}
