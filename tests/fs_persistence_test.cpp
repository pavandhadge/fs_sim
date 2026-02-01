#include "fs/filesystem.hpp"
#include "fs/disk.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <random>

#define ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "[FAIL] " << message << " (" << #condition << ")\n"; \
        std::exit(1); \
    } else { \
        std::cout << "[PASS] " << message << "\n"; \
    }

void cleanup_file(const char* filename) {
    std::remove(filename);
}

std::vector<uint8_t> generate_random_data(size_t size, int seed = 42) {
    std::vector<uint8_t> data(size);
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(0, 255);
    for (size_t i = 0; i < size; ++i) data[i] = static_cast<uint8_t>(dis(gen));
    return data;
}

// ==========================================
// PERSISTENCE TESTS
// ==========================================
void test_basic_persistence() {
    std::cout << "\n=== Persistence Tests: Basic ===\n";
    const char* TEST_IMG = "test_persist_basic.img";
    cleanup_file(TEST_IMG);

    const size_t DISK_SIZE = 16 * 1024 * 1024;

    // Session 1: Format and create
    {
        std::cout << "  -> Session 1: Creating filesystem...\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.format();

        fs.create_dir("/home");
        fs.create_dir("/home/user");
        fs.create_file("/home/user/config.txt");
        
        std::string secret = "This must survive reboot!";
        fs.write_file("/home/user/config.txt", 
                      std::vector<uint8_t>(secret.begin(), secret.end()));
        
        std::cout << "  -> Session 1: Data written. Powering down...\n";
    }

    // Session 2: Remount and verify
    {
        std::cout << "  -> Session 2: Remounting...\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.mount();

        auto entries = fs.list_dir("/home/user");
        ASSERT(entries.size() == 1 && entries[0].name == "config.txt", 
               "Directory structure persisted");

        auto data = fs.read_file("/home/user/config.txt");
        std::string read_str(data.begin(), data.end());
        ASSERT(read_str == "This must survive reboot!", 
               "File content persisted");
    }

    cleanup_file(TEST_IMG);
}

void test_large_file_persistence() {
    std::cout << "\n=== Persistence Tests: Large Files ===\n";
    const char* TEST_IMG = "test_persist_large.img";
    cleanup_file(TEST_IMG);

    const size_t DISK_SIZE = 32 * 1024 * 1024;
    const size_t FILE_SIZE = 49152; // 48KB max

    // Session 1: Write large file
    {
        std::cout << "  -> Session 1: Writing large file...\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.format();

        fs.create_file("/bigdata.bin");
        auto data = generate_random_data(FILE_SIZE, 12345);
        fs.write_file("/bigdata.bin", data);
    }

    // Session 2: Verify large file
    {
        std::cout << "  -> Session 2: Verifying large file...\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.mount();

        auto data = fs.read_file("/bigdata.bin");
        auto expected = generate_random_data(FILE_SIZE, 12345);
        ASSERT(data == expected, "Large file data persisted correctly");
    }

    cleanup_file(TEST_IMG);
}

void test_complex_tree_persistence() {
    std::cout << "\n=== Persistence Tests: Complex Tree ===\n";
    const char* TEST_IMG = "test_persist_tree.img";
    cleanup_file(TEST_IMG);

    const size_t DISK_SIZE = 64 * 1024 * 1024;

    // Session 1: Create complex structure
    {
        std::cout << "  -> Session 1: Creating complex tree...\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.format();

        // Create tree
        fs.create_dir("/bin");
        fs.create_dir("/etc");
        fs.create_dir("/home");
        fs.create_dir("/home/alice");
        fs.create_dir("/home/bob");
        fs.create_dir("/var");
        fs.create_dir("/var/log");

        // Create files
        fs.create_file("/bin/ls");
        fs.create_file("/bin/cat");
        fs.create_file("/etc/passwd");
        fs.create_file("/home/alice/profile");
        fs.create_file("/home/bob/profile");
        fs.create_file("/var/log/syslog");

        // Write some content
        fs.write_file("/etc/passwd", {'u', 's', 'e', 'r', 's'});
        fs.write_file("/home/alice/profile", {'A', 'L', 'I', 'C', 'E'});
        fs.write_file("/var/log/syslog", {'l', 'o', 'g', 's'});
    }

    // Session 2: Verify structure
    {
        std::cout << "  -> Session 2: Verifying tree structure...\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.mount();

        ASSERT(fs.list_dir("/").size() == 4, "Root has 4 entries (bin, etc, home, var)");
        ASSERT(fs.list_dir("/bin").size() == 2, "bin has 2 files");
        ASSERT(fs.list_dir("/home").size() == 2, "home has 2 users");
        ASSERT(fs.list_dir("/home/alice").size() == 1, "alice has 1 file");
        ASSERT(fs.list_dir("/var").size() == 1, "var has 1 subdirectory");
        ASSERT(fs.list_dir("/var/log").size() == 1, "log has 1 file");

        auto data = fs.read_file("/etc/passwd");
        ASSERT(data.size() == 5, "File content correct");
    }

    cleanup_file(TEST_IMG);
}

void test_multi_session_operations() {
    std::cout << "\n=== Persistence Tests: Multi-Session ===\n";
    const char* TEST_IMG = "test_persist_multi.img";
    cleanup_file(TEST_IMG);

    const size_t DISK_SIZE = 32 * 1024 * 1024;

    // Session 1: Initial setup
    {
        std::cout << "  -> Session 1: Initial setup\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.format();

        fs.create_dir("/data");
        fs.create_file("/data/v1.txt");
        fs.write_file("/data/v1.txt", {'v', '1'});
    }

    // Session 2: Add more data
    {
        std::cout << "  -> Session 2: Adding data\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.mount();

        fs.create_file("/data/v2.txt");
        fs.write_file("/data/v2.txt", {'v', '2'});
        fs.write_file("/data/v1.txt", {'u', 'p', 'd', 'a', 't', 'e', 'd'});
    }

    // Session 3: Verify all changes
    {
        std::cout << "  -> Session 3: Verifying all changes\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.mount();

        auto entries = fs.list_dir("/data");
        ASSERT(entries.size() == 2, "Both files exist");

        auto v1 = fs.read_file("/data/v1.txt");
        std::string v1_str(v1.begin(), v1.end());
        ASSERT(v1_str == "updated", "v1.txt updated correctly");

        auto v2 = fs.read_file("/data/v2.txt");
        ASSERT(v2.size() == 2 && v2[0] == 'v', "v2.txt created correctly");
    }

    // Session 4: Delete and modify
    {
        std::cout << "  -> Session 4: Deleting v1.txt\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.mount();

        fs.delete_file("/data/v1.txt");
    }

    // Session 5: Final verification
    {
        std::cout << "  -> Session 5: Final check\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.mount();

        auto entries = fs.list_dir("/data");
        ASSERT(entries.size() == 1 && entries[0].name == "v2.txt", 
               "Only v2.txt remains after deletion");
    }

    cleanup_file(TEST_IMG);
}

void test_metadata_persistence() {
    std::cout << "\n=== Persistence Tests: Metadata ===\n";
    const char* TEST_IMG = "test_persist_meta.img";
    cleanup_file(TEST_IMG);

    const size_t DISK_SIZE = 16 * 1024 * 1024;

    // Session 1: Create with specific user
    {
        std::cout << "  -> Session 1: Creating files as user 500\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.format();

        fs.login(500, 500);
        fs.create_dir("/user500");
        fs.create_file("/user500/private.txt");
        fs.write_file("/user500/private.txt", {'p', 'r', 'i', 'v'});
    }

    // Session 2: Verify metadata and permissions
    {
        std::cout << "  -> Session 2: Verifying metadata\n";
        Disk disk(DISK_SIZE, TEST_IMG);
        FileSystem fs(disk);
        fs.mount();

        auto entries = fs.list_dir("/");
        for (const auto& entry : entries) {
            if (entry.name == "user500") {
                ASSERT(entry.uid == 500, "Directory UID persisted");
                ASSERT(entry.gid == 500, "Directory GID persisted");
                ASSERT(entry.permissions == 0755, "Directory permissions persisted");
            }
        }

        // Verify another user (501) can read but not write with default 0644 perms
        fs.login(501, 501);
        auto data = fs.read_file("/user500/private.txt");
        ASSERT(data.size() == 4, "User 501 can read with default 0644 permissions");
        
        // But user 501 cannot write
        bool caught = false;
        try {
            fs.write_file("/user500/private.txt", {'h', 'a', 'c', 'k'});
        } catch (const std::runtime_error&) {
            caught = true;
        }
        ASSERT(caught, "Write permissions enforced after remount");
    }

    cleanup_file(TEST_IMG);
}

// ==========================================
// MAIN
// ==========================================
int main() {
    std::cout << "STARTING PERSISTENCE TEST SUITE\n";
    std::cout << "===============================\n";

    try {
        test_basic_persistence();
        test_large_file_persistence();
        test_complex_tree_persistence();
        test_multi_session_operations();
        test_metadata_persistence();
    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL FAILURE] Uncaught Exception: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n===============================\n";
    std::cout << "ALL PERSISTENCE TESTS PASSED.\n";
    return 0;
}
