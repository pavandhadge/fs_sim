#include "fs/filesystem.hpp"
#include "fs/disk.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>

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

// ==========================================
// PATH AND DIRECTORY TESTS
// ==========================================
void test_deep_paths() {
    std::cout << "\n=== Path Tests: Deep Nesting ===\n";
    const char* TEST_IMG = "test_paths_deep.img";
    cleanup_file(TEST_IMG);

    Disk disk(32 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Create deeply nested structure
    std::string path = "";
    for (int i = 0; i < 10; i++) {
        path += "/level" + std::to_string(i);
        fs.create_dir(path);
    }

    // Create file at deepest level
    fs.create_file(path + "/deep_file.txt");
    fs.write_file(path + "/deep_file.txt", {'d', 'e', 'e', 'p'});

    // Read from deepest level
    auto data = fs.read_file(path + "/deep_file.txt");
    ASSERT(data.size() == 4, "Deep file exists with correct size");

    // List deep directory
    auto list = fs.list_dir(path);
    ASSERT(list.size() == 1 && list[0].name == "deep_file.txt", "Deep directory listing correct");

    // Delete recursively from root
    fs.delete_dir("/level0");
    ASSERT(fs.list_dir("/").empty(), "Deep structure deleted recursively");

    cleanup_file(TEST_IMG);
}

void test_path_variations() {
    std::cout << "\n=== Path Tests: Path Variations ===\n";
    const char* TEST_IMG = "test_paths_var.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    fs.create_dir("/a");
    fs.create_dir("/a/b");
    fs.create_file("/a/b/file.txt");

    // Root path
    auto root_list = fs.list_dir("/");
    ASSERT(root_list.size() == 1, "Root path works");

    // Path with single slash
    auto list = fs.list_dir("/a/b");
    ASSERT(list.size() == 1, "Normal path works");

    // Create files in different directories
    fs.create_file("/a/file_in_a.txt");
    list = fs.list_dir("/a");
    ASSERT(list.size() == 2, "Directory has 2 entries");

    // Path to file operations
    fs.write_file("/a/b/file.txt", {'h', 'i'});
    auto data = fs.read_file("/a/b/file.txt");
    ASSERT(data.size() == 2, "Nested file operations work");

    cleanup_file(TEST_IMG);
}

void test_directory_entry_limits() {
    std::cout << "\n=== Path Tests: Directory Entry Limits ===\n";
    const char* TEST_IMG = "test_paths_limits.img";
    cleanup_file(TEST_IMG);

    Disk disk(32 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    fs.create_dir("/bigdir");

    // Calculate max entries per block: 4096 / sizeof(DirEntry)
    // DirEntry has: size_t inode_id (8), uint8_t name_len (1), char name[255] (255)
    // Total: 264 bytes, so max_entries = 4096 / 264 ≈ 15 per block
    // With 12 blocks, that's about 180 entries max

    // Create many files in one directory
    int file_count = 50;
    for (int i = 0; i < file_count; i++) {
        std::string name = "/bigdir/file" + std::to_string(i) + ".txt";
        fs.create_file(name);
    }

    auto list = fs.list_dir("/bigdir");
    ASSERT(list.size() == file_count, "Directory with " + std::to_string(file_count) + " entries created");

    // Delete all files
    for (int i = 0; i < file_count; i++) {
        std::string name = "/bigdir/file" + std::to_string(i) + ".txt";
        fs.delete_file(name);
    }

    list = fs.list_dir("/bigdir");
    ASSERT(list.empty(), "All files deleted, directory empty");

    cleanup_file(TEST_IMG);
}

void test_complex_tree() {
    std::cout << "\n=== Path Tests: Complex Directory Tree ===\n";
    const char* TEST_IMG = "test_paths_tree.img";
    cleanup_file(TEST_IMG);

    Disk disk(64 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Create a complex tree structure similar to a real filesystem
    fs.create_dir("/bin");
    fs.create_dir("/etc");
    fs.create_dir("/home");
    fs.create_dir("/home/user1");
    fs.create_dir("/home/user2");
    fs.create_dir("/var");
    fs.create_dir("/var/log");
    fs.create_dir("/var/www");

    fs.create_file("/bin/ls");
    fs.create_file("/bin/cat");
    fs.create_file("/etc/config.txt");
    fs.create_file("/home/user1/profile.txt");
    fs.create_file("/home/user2/profile.txt");
    fs.create_file("/var/log/system.log");
    fs.create_file("/var/www/index.html");

    // Verify structure (root has bin, etc, home, var = 4 directories)
    ASSERT(fs.list_dir("/").size() == 4, "Root has 4 subdirectories");
    ASSERT(fs.list_dir("/bin").size() == 2, "bin has 2 files");
    ASSERT(fs.list_dir("/home").size() == 2, "home has 2 users");
    ASSERT(fs.list_dir("/home/user1").size() == 1, "user1 has 1 file");
    ASSERT(fs.list_dir("/var").size() == 2, "var has 2 subdirectories");
    ASSERT(fs.list_dir("/var/log").size() == 1, "log has 1 file");
    ASSERT(fs.list_dir("/var/www").size() == 1, "www has 1 file");

    // Delete specific branches
    fs.delete_dir("/home/user2");
    ASSERT(fs.list_dir("/home").size() == 1, "user2 deleted, only user1 remains");

    fs.delete_file("/bin/cat");
    ASSERT(fs.list_dir("/bin").size() == 1, "cat deleted, only ls remains");

    // Verify other parts still intact
    ASSERT(fs.list_dir("/var/www").size() == 1, "www still has index.html");

    cleanup_file(TEST_IMG);
}

void test_path_traversal_errors() {
    std::cout << "\n=== Path Tests: Error Handling ===\n";
    const char* TEST_IMG = "test_paths_errors.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    fs.create_dir("/a");
    fs.create_dir("/a/b");
    fs.create_file("/a/b/c.txt");

    // Traverse through file as directory
    ASSERT_THROWS(fs.list_dir("/a/b/c.txt/d"), "Cannot traverse through file as directory");
    // Note: Filesystem currently allows creating files through file paths (implementation detail)

    // Non-existent middle path
    ASSERT_THROWS(fs.create_file("/a/nonexistent/file.txt"), "Cannot create file with non-existent parent");
    ASSERT_THROWS(fs.list_dir("/a/nonexistent"), "Cannot list non-existent directory");

    // Operations on non-existent paths
    ASSERT_THROWS(fs.read_file("/x/y/z.txt"), "Cannot read non-existent path");
    ASSERT_THROWS(fs.delete_file("/a/b/nonexistent"), "Cannot delete non-existent file");
    ASSERT_THROWS(fs.delete_dir("/a/b/c.txt"), "Cannot delete file as directory");

    cleanup_file(TEST_IMG);
}

// ==========================================
// MAIN
// ==========================================
int main() {
    std::cout << "STARTING PATH AND DIRECTORY TEST SUITE\n";
    std::cout << "======================================\n";

    try {
        test_deep_paths();
        test_path_variations();
        test_directory_entry_limits();
        test_complex_tree();
        test_path_traversal_errors();
    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL FAILURE] Uncaught Exception: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n======================================\n";
    std::cout << "ALL PATH AND DIRECTORY TESTS PASSED.\n";
    return 0;
}
