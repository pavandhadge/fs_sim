#include "fs/disk.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cassert>

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
// DISK TESTS
// ==========================================
void test_disk_creation() {
    std::cout << "\n=== Disk Tests: Creation ===\n";
    const char* TEST_IMG = "test_disk_creation.img";
    cleanup_file(TEST_IMG);

    // Valid creation
    {
        Disk disk(16 * 1024 * 1024, TEST_IMG);
        ASSERT(disk.get_block_size() == 4096, "Block size is 4096");
        ASSERT(disk.get_block_count() == 4096, "16MB disk has 4096 blocks");
    }

    // Invalid size (not multiple of 4096)
    ASSERT_THROWS(Disk disk(1000, TEST_IMG), "Disk creation fails with non-aligned size");

    cleanup_file(TEST_IMG);
}

void test_disk_read_write() {
    std::cout << "\n=== Disk Tests: Read/Write ===\n";
    const char* TEST_IMG = "test_disk_rw.img";
    cleanup_file(TEST_IMG);

    Disk disk(4 * 1024 * 1024, TEST_IMG);

    // Write test pattern to block 1
    std::vector<uint8_t> write_buffer(4096);
    for (int i = 0; i < 4096; i++) {
        write_buffer[i] = static_cast<uint8_t>(i % 256);
    }
    disk.write_block(1, write_buffer.data());

    // Read it back
    std::vector<uint8_t> read_buffer(4096);
    disk.read_block(1, read_buffer.data());

    // Verify
    bool match = true;
    for (int i = 0; i < 4096; i++) {
        if (read_buffer[i] != write_buffer[i]) {
            match = false;
            break;
        }
    }
    ASSERT(match, "Written data matches read data");

    // Test out of bounds
    ASSERT_THROWS(disk.read_block(-1, read_buffer.data()), "Read negative block throws");
    ASSERT_THROWS(disk.read_block(4096, read_buffer.data()), "Read beyond block count throws");
    ASSERT_THROWS(disk.write_block(-1, write_buffer.data()), "Write negative block throws");
    ASSERT_THROWS(disk.write_block(4096, write_buffer.data()), "Write beyond block count throws");

    cleanup_file(TEST_IMG);
}

void test_disk_persistence() {
    std::cout << "\n=== Disk Tests: Persistence ===\n";
    const char* TEST_IMG = "test_disk_persist.img";
    cleanup_file(TEST_IMG);

    // Write data
    {
        Disk disk(4 * 1024 * 1024, TEST_IMG);
        std::vector<uint8_t> data(4096, 0xAB);
        disk.write_block(10, data.data());
    }

    // Read data back in new instance
    {
        Disk disk(4 * 1024 * 1024, TEST_IMG);
        std::vector<uint8_t> read_buffer(4096);
        disk.read_block(10, read_buffer.data());
        
        bool all_ab = true;
        for (int i = 0; i < 4096; i++) {
            if (read_buffer[i] != 0xAB) {
                all_ab = false;
                break;
            }
        }
        ASSERT(all_ab, "Data persists across disk instances");
    }

    cleanup_file(TEST_IMG);
}

void test_disk_get_ptr() {
    std::cout << "\n=== Disk Tests: Direct Pointer Access ===\n";
    const char* TEST_IMG = "test_disk_ptr.img";
    cleanup_file(TEST_IMG);

    Disk disk(4 * 1024 * 1024, TEST_IMG);

    // Write via pointer
    uint8_t* ptr = disk.get_ptr(5);
    std::memset(ptr, 0xCD, 4096);

    // Read via read_block
    std::vector<uint8_t> read_buffer(4096);
    disk.read_block(5, read_buffer.data());

    bool all_cd = true;
    for (int i = 0; i < 4096; i++) {
        if (read_buffer[i] != 0xCD) {
            all_cd = false;
            break;
        }
    }
    ASSERT(all_cd, "Direct pointer write matches read_block read");

    // Test out of bounds
    ASSERT_THROWS(disk.get_ptr(-1), "Get pointer for negative block throws");
    ASSERT_THROWS(disk.get_ptr(4096), "Get pointer for out of bounds block throws");

    cleanup_file(TEST_IMG);
}

// ==========================================
// MAIN
// ==========================================
int main() {
    std::cout << "STARTING DISK TEST SUITE\n";
    std::cout << "========================\n";

    try {
        test_disk_creation();
        test_disk_read_write();
        test_disk_persistence();
        test_disk_get_ptr();
    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL FAILURE] Uncaught Exception: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n========================\n";
    std::cout << "ALL DISK TESTS PASSED.\n";
    return 0;
}
