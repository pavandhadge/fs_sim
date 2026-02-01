#include "fs/filesystem.hpp"
#include "fs/disk.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <random>
#include <algorithm>

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
        std::string caught_msg; \
        try { code; } \
        catch (const std::exception& e) { caught = true; caught_msg = e.what(); } \
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

std::vector<uint8_t> generate_data(size_t size, uint8_t fill = 0) {
    std::vector<uint8_t> data(size);
    if (fill == 0) {
        std::mt19937 gen(42);
        std::uniform_int_distribution<> dis(0, 255);
        for (size_t i = 0; i < size; ++i) data[i] = static_cast<uint8_t>(dis(gen));
    } else {
        std::fill(data.begin(), data.end(), fill);
    }
    return data;
}

// ==========================================
// FILESYSTEM OPERATIONS TESTS
// ==========================================
void test_format_and_mount() {
    std::cout << "\n=== FS Tests: Format and Mount ===\n";
    const char* TEST_IMG = "test_fs_format.img";
    cleanup_file(TEST_IMG);

    // Format new filesystem
    {
        Disk disk(16 * 1024 * 1024, TEST_IMG);
        FileSystem fs(disk);
        fs.format();
        
        // Root should be empty
        auto root_list = fs.list_dir("/");
        ASSERT(root_list.empty(), "Newly formatted filesystem has empty root");
    }

    // Mount existing filesystem
    {
        Disk disk(16 * 1024 * 1024, TEST_IMG);
        FileSystem fs(disk);
        fs.mount();
        
        auto root_list = fs.list_dir("/");
        ASSERT(root_list.empty(), "Mounted filesystem has empty root");
    }

    // Mount without format should fail on new disk
    {
        cleanup_file(TEST_IMG);
        Disk disk(16 * 1024 * 1024, TEST_IMG);
        FileSystem fs(disk);
        ASSERT_THROWS(fs.mount(), "Mount fails on unformatted disk");
    }

    cleanup_file(TEST_IMG);
}

void test_file_creation() {
    std::cout << "\n=== FS Tests: File Creation ===\n";
    const char* TEST_IMG = "test_fs_create.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Create single file
    fs.create_file("/test.txt");
    auto list = fs.list_dir("/");
    ASSERT(list.size() == 1 && list[0].name == "test.txt", "Single file created");
    ASSERT(!list[0].is_directory, "File is not a directory");
    ASSERT(list[0].uid == 0, "File owned by root");
    ASSERT(list[0].permissions == 0644, "File has default permissions 0644");

    // Create multiple files
    fs.create_file("/file2.txt");
    fs.create_file("/file3.txt");
    list = fs.list_dir("/");
    ASSERT(list.size() == 3, "Three files created");

    // Duplicate file should fail
    ASSERT_THROWS(fs.create_file("/test.txt"), "Duplicate file creation fails");

    // Create file in non-existent directory should fail
    ASSERT_THROWS(fs.create_file("/nonexistent/file.txt"), "File creation in non-existent dir fails");

    cleanup_file(TEST_IMG);
}

void test_file_write_read() {
    std::cout << "\n=== FS Tests: File Write/Read ===\n";
    const char* TEST_IMG = "test_fs_write.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Test empty file
    fs.create_file("/empty.txt");
    auto data = fs.read_file("/empty.txt");
    ASSERT(data.empty(), "Empty file read returns empty data");

    // Test small file (1 byte)
    fs.create_file("/small.txt");
    fs.write_file("/small.txt", {'X'});
    data = fs.read_file("/small.txt");
    ASSERT(data.size() == 1 && data[0] == 'X', "Single byte file written and read correctly");

    // Test medium file (1 block)
    fs.create_file("/block.txt");
    auto block_data = generate_data(4096);
    fs.write_file("/block.txt", block_data);
    data = fs.read_file("/block.txt");
    ASSERT(data == block_data, "Full block file written and read correctly");

    // Test multi-block file
    fs.create_file("/multiblock.txt");
    auto multi_data = generate_data(8192);
    fs.write_file("/multiblock.txt", multi_data);
    data = fs.read_file("/multiblock.txt");
    ASSERT(data == multi_data, "Multi-block file (2 blocks) written and read correctly");

    // Test overwrite (shrink)
    std::vector<uint8_t> abc_data = {'A', 'B', 'C'};
    fs.write_file("/multiblock.txt", abc_data);
    data = fs.read_file("/multiblock.txt");
    ASSERT(data.size() == 3 && data == abc_data, "File shrink overwrite works");

    // Test overwrite (grow)
    auto large_data = generate_data(12288); // 3 blocks
    fs.write_file("/multiblock.txt", large_data);
    data = fs.read_file("/multiblock.txt");
    ASSERT(data == large_data, "File grow overwrite works");

    // Read non-existent file should fail
    ASSERT_THROWS(fs.read_file("/nonexistent.txt"), "Read non-existent file fails");

    cleanup_file(TEST_IMG);
}

void test_file_deletion() {
    std::cout << "\n=== FS Tests: File Deletion ===\n";
    const char* TEST_IMG = "test_fs_delete.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Create and delete file
    fs.create_file("/temp.txt");
    ASSERT(fs.list_dir("/").size() == 1, "File created");
    
    fs.delete_file("/temp.txt");
    ASSERT(fs.list_dir("/").empty(), "File deleted");

    // Delete non-existent file should fail
    ASSERT_THROWS(fs.delete_file("/nonexistent.txt"), "Delete non-existent file fails");

    // Note: delete_file doesn't currently verify target is a file vs directory
    // The filesystem implementation treats them similarly at the entry level

    // Create, write, delete, recreate cycle
    fs.create_file("/cycle.txt");
    fs.write_file("/cycle.txt", generate_data(8192));
    fs.delete_file("/cycle.txt");
    fs.create_file("/cycle.txt");
    auto data = fs.read_file("/cycle.txt");
    ASSERT(data.empty(), "Recreated file after delete is empty");

    cleanup_file(TEST_IMG);
}

void test_max_file_size() {
    std::cout << "\n=== FS Tests: Max File Size (48KB) ===\n";
    const char* TEST_IMG = "test_fs_maxsize.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    fs.create_file("/maxfile.bin");

    // Max size = 12 blocks * 4096 = 49152 bytes
    auto max_data = generate_data(49152);
    fs.write_file("/maxfile.bin", max_data);
    auto data = fs.read_file("/maxfile.bin");
    ASSERT(data == max_data, "Max size file (48KB) written and read correctly");

    // Try to write one byte too many
    auto too_big = generate_data(49153);
    ASSERT_THROWS(fs.write_file("/maxfile.bin", too_big), "Write beyond 48KB limit fails");

    cleanup_file(TEST_IMG);
}

void test_directory_creation() {
    std::cout << "\n=== FS Tests: Directory Creation ===\n";
    const char* TEST_IMG = "test_fs_mkdir.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Create single directory
    fs.create_dir("/mydir");
    auto list = fs.list_dir("/");
    ASSERT(list.size() == 1 && list[0].name == "mydir", "Single directory created");
    ASSERT(list[0].is_directory, "Directory is marked as directory");
    ASSERT(list[0].permissions == 0755, "Directory has default permissions 0755");

    // Create nested directory
    fs.create_dir("/mydir/subdir");
    list = fs.list_dir("/mydir");
    ASSERT(list.size() == 1 && list[0].name == "subdir", "Nested directory created");

    // Duplicate directory should fail
    ASSERT_THROWS(fs.create_dir("/mydir"), "Duplicate directory creation fails");

    // Create directory as file should fail
    fs.create_file("/file.txt");
    ASSERT_THROWS(fs.create_dir("/file.txt"), "Create directory over file fails");

    cleanup_file(TEST_IMG);
}

void test_directory_listing() {
    std::cout << "\n=== FS Tests: Directory Listing ===\n";
    const char* TEST_IMG = "test_fs_ls.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // List empty root
    auto list = fs.list_dir("/");
    ASSERT(list.empty(), "Empty root directory listing is empty");

    // List root with entries
    fs.create_file("/file1.txt");
    fs.create_dir("/dir1");
    fs.create_file("/file2.txt");
    
    list = fs.list_dir("/");
    ASSERT(list.size() == 3, "Root has 3 entries");

    // List non-existent directory should fail
    ASSERT_THROWS(fs.list_dir("/nonexistent"), "List non-existent directory fails");

    // List file as directory should fail
    ASSERT_THROWS(fs.list_dir("/file1.txt"), "List file as directory fails");

    cleanup_file(TEST_IMG);
}

void test_directory_deletion() {
    std::cout << "\n=== FS Tests: Directory Deletion ===\n";
    const char* TEST_IMG = "test_fs_rmdir.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Create and delete empty directory
    fs.create_dir("/emptydir");
    ASSERT(fs.list_dir("/").size() == 1, "Directory created");
    
    fs.delete_dir("/emptydir");
    ASSERT(fs.list_dir("/").empty(), "Empty directory deleted");

    // Delete non-existent directory should fail
    ASSERT_THROWS(fs.delete_dir("/nonexistent"), "Delete non-existent directory fails");

    // Delete file as directory should fail
    fs.create_file("/file.txt");
    ASSERT_THROWS(fs.delete_dir("/file.txt"), "Delete file as directory fails");

    // Delete non-empty directory (recursive)
    fs.create_dir("/parent");
    fs.create_file("/parent/child.txt");
    fs.create_dir("/parent/subdir");
    fs.create_file("/parent/subdir/grandchild.txt");
    
    ASSERT(fs.list_dir("/parent").size() == 2, "Parent has 2 children");
    ASSERT(fs.list_dir("/parent/subdir").size() == 1, "Subdir has 1 child");
    
    fs.delete_dir("/parent");
    ASSERT(fs.list_dir("/").size() == 1, "Non-empty directory deleted recursively, file.txt remains");

    cleanup_file(TEST_IMG);
}

void test_mixed_operations() {
    std::cout << "\n=== FS Tests: Mixed Operations ===\n";
    const char* TEST_IMG = "test_fs_mixed.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Create complex structure
    fs.create_dir("/home");
    fs.create_dir("/home/user");
    fs.create_dir("/home/user/docs");
    fs.create_file("/home/user/docs/readme.txt");
    fs.write_file("/home/user/docs/readme.txt", {'H', 'e', 'l', 'l', 'o'});
    
    fs.create_dir("/var");
    fs.create_file("/var/log.txt");
    
    // Verify structure
    ASSERT(fs.list_dir("/").size() == 2, "Root has 2 entries");
    ASSERT(fs.list_dir("/home/user/docs").size() == 1, "Docs has 1 entry");
    
    auto data = fs.read_file("/home/user/docs/readme.txt");
    std::string content(data.begin(), data.end());
    ASSERT(content == "Hello", "File content correct");

    // Delete and verify
    fs.delete_dir("/home");
    ASSERT(fs.list_dir("/").size() == 1, "Home deleted, only var remains");
    ASSERT(fs.list_dir("/var").size() == 1, "Var still has log.txt");

    fs.delete_file("/var/log.txt");
    ASSERT(fs.list_dir("/var").empty(), "Log deleted, var empty");

    cleanup_file(TEST_IMG);
}

// ==========================================
// MAIN
// ==========================================
int main() {
    std::cout << "STARTING FILESYSTEM OPERATIONS TEST SUITE\n";
    std::cout << "=========================================\n";

    try {
        test_format_and_mount();
        test_file_creation();
        test_file_write_read();
        test_file_deletion();
        test_max_file_size();
        test_directory_creation();
        test_directory_listing();
        test_directory_deletion();
        test_mixed_operations();
    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL FAILURE] Uncaught Exception: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n=========================================\n";
    std::cout << "ALL FILESYSTEM OPERATIONS TESTS PASSED.\n";
    return 0;
}
