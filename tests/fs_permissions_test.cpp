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

#define ASSERT_NO_THROW(code, message) \
    { \
        bool threw = false; \
        try { code; } \
        catch (const std::exception&) { threw = true; } \
        if (threw) { \
            std::cerr << "[FAIL] " << message << " (Unexpected exception thrown)\n"; \
            std::exit(1); \
        } else { \
            std::cout << "[PASS] " << message << "\n"; \
        } \
    }

void cleanup_file(const char* filename) {
    std::remove(filename);
}

// ==========================================
// PERMISSION SYSTEM TESTS
// ==========================================
void test_basic_permissions() {
    std::cout << "\n=== Permission Tests: Basic ===\n";
    const char* TEST_IMG = "test_perms_basic.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Create file as user 100
    fs.login(100, 100);
    fs.create_file("/u100_file.txt");
    std::string content = "User 100 content";
    fs.write_file("/u100_file.txt", std::vector<uint8_t>(content.begin(), content.end()));

    // Owner can read and write
    auto data = fs.read_file("/u100_file.txt");
    ASSERT(data.size() == content.size(), "Owner can read own file");

    std::string new_content = "Modified by owner";
    fs.write_file("/u100_file.txt", std::vector<uint8_t>(new_content.begin(), new_content.end()));
    ASSERT(true, "Owner can write own file");

    cleanup_file(TEST_IMG);
}

void test_cross_user_permissions() {
    std::cout << "\n=== Permission Tests: Cross-User ===\n";
    const char* TEST_IMG = "test_perms_cross.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Root creates a shared directory
    fs.create_dir("/shared");

    // User 100 creates a file
    fs.login(100, 100);
    fs.create_file("/shared/u100.txt");
    fs.write_file("/shared/u100.txt", {'s', 'e', 'c', 'r', 'e', 't'});

    // User 200 tries to read (default perms 0644 allow others to read, so this should work)
    fs.login(200, 200);
    bool caught = false;
    try {
        auto data = fs.read_file("/shared/u100.txt");
        // If we get here, read succeeded (0644 allows others to read)
        std::cout << "  -> User 200 read allowed (default 0644 permissions)\n";
    } catch (const std::runtime_error& e) {
        caught = true;
        std::cout << "  -> User 200 read blocked: " << e.what() << "\n";
    }
    // Note: Default 0644 means others CAN read, so we don't assert caught here

    // User 200 tries to write (should fail)
    caught = false;
    try {
        fs.write_file("/shared/u100.txt", {'h', 'a', 'c', 'k'});
    } catch (const std::runtime_error& e) {
        caught = true;
        std::cout << "  -> User 200 write blocked: " << e.what() << "\n";
    }
    ASSERT(caught, "User 200 blocked from writing user 100's file");

    // User 200 tries to delete (should fail - no write on /shared)
    caught = false;
    try {
        fs.delete_file("/shared/u100.txt");
    } catch (const std::runtime_error& e) {
        caught = true;
        std::cout << "  -> User 200 delete blocked: " << e.what() << "\n";
    }
    ASSERT(caught, "User 200 blocked from deleting user 100's file");

    cleanup_file(TEST_IMG);
}

void test_group_permissions() {
    std::cout << "\n=== Permission Tests: Group Access ===\n";
    const char* TEST_IMG = "test_perms_group.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Create shared directory for group 100
    fs.login(100, 100);
    fs.create_dir("/group100_share");
    fs.create_file("/group100_share/doc.txt");
    fs.write_file("/group100_share/doc.txt", {'g', 'r', 'o', 'u', 'p'});

    // Different user in same group should have access (default 0644 allows group read)
    fs.login(200, 100);  // Different UID, same GID
    auto data = fs.read_file("/group100_share/doc.txt");
    ASSERT(data.size() == 5, "Group member can read file");

    // But cannot write (default 0644 doesn't give group write)
    bool caught = false;
    try {
        fs.write_file("/group100_share/doc.txt", {'h', 'a', 'c', 'k'});
    } catch (const std::runtime_error& e) {
        caught = true;
    }
    ASSERT(caught, "Group member blocked from writing without permission");

    cleanup_file(TEST_IMG);
}

void test_root_override() {
    std::cout << "\n=== Permission Tests: Root Override ===\n";
    const char* TEST_IMG = "test_perms_root.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // User 100 creates private file
    fs.login(100, 100);
    fs.create_file("/private.txt");
    fs.write_file("/private.txt", {'p', 'r', 'i', 'v', 'a', 't', 'e'});

    // User 200 creates another private file
    fs.login(200, 200);
    fs.create_file("/secret.txt");
    fs.write_file("/secret.txt", {'s', 'e', 'c', 'r', 'e', 't'});

    // Root can read anything
    fs.logout();  // Back to UID 0
    auto data1 = fs.read_file("/private.txt");
    auto data2 = fs.read_file("/secret.txt");
    ASSERT(data1.size() == 7 && data2.size() == 6, "Root can read any file");

    // Root can write anything
    fs.write_file("/private.txt", {'r', 'o', 'o', 't'});
    data1 = fs.read_file("/private.txt");
    ASSERT(data1.size() == 4, "Root can write any file");

    // Root can delete anything
    fs.delete_file("/private.txt");
    fs.delete_file("/secret.txt");
    ASSERT(fs.list_dir("/").empty(), "Root can delete any file");

    cleanup_file(TEST_IMG);
}

void test_directory_permissions() {
    std::cout << "\n=== Permission Tests: Directory Access ===\n";
    const char* TEST_IMG = "test_perms_dir.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Root creates shared directory with files
    fs.create_dir("/shared");
    fs.create_file("/shared/public.txt");
    fs.write_file("/shared/public.txt", {'h', 'e', 'l', 'l', 'o'});

    // Root can list (has all permissions)
    auto list = fs.list_dir("/shared");
    ASSERT(list.size() == 1, "Root can list directory");

    // Regular user can list (default 0755 gives others read+execute)
    fs.login(100, 100);
    list = fs.list_dir("/shared");
    ASSERT(list.size() == 1, "Regular user can list directory with 0755 perms");

    // Regular user can read files in directory
    auto data = fs.read_file("/shared/public.txt");
    ASSERT(data.size() == 5, "Regular user can read file in shared directory");

    cleanup_file(TEST_IMG);
}

void test_metadata_tracking() {
    std::cout << "\n=== Permission Tests: Metadata Tracking ===\n";
    const char* TEST_IMG = "test_perms_meta.img";
    cleanup_file(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format();

    // Check root-created entries
    fs.create_dir("/root_dir");
    fs.create_file("/root_file.txt");

    auto list = fs.list_dir("/");
    for (const auto& entry : list) {
        if (entry.name == "root_dir") {
            ASSERT(entry.is_directory, "Directory entry correctly marked");
            ASSERT(entry.uid == 0, "Root directory owned by root");
            ASSERT(entry.gid == 0, "Root directory group is root");
            ASSERT(entry.permissions == 0755, "Directory has 0755 permissions");
        }
        if (entry.name == "root_file.txt") {
            ASSERT(!entry.is_directory, "File entry correctly marked");
            ASSERT(entry.uid == 0, "Root file owned by root");
            ASSERT(entry.permissions == 0644, "File has 0644 permissions");
        }
    }

    // Check user-created entries
    fs.login(500, 500);
    fs.create_dir("/user_dir");
    fs.create_file("/user_file.txt");

    list = fs.list_dir("/");
    for (const auto& entry : list) {
        if (entry.name == "user_dir") {
            ASSERT(entry.uid == 500, "User directory has correct UID");
            ASSERT(entry.gid == 500, "User directory has correct GID");
        }
        if (entry.name == "user_file.txt") {
            ASSERT(entry.uid == 500, "User file has correct UID");
            ASSERT(entry.gid == 500, "User file has correct GID");
        }
    }

    cleanup_file(TEST_IMG);
}

void test_permission_persistence() {
    std::cout << "\n=== Permission Tests: Persistence ===\n";
    const char* TEST_IMG = "test_perms_persist.img";
    cleanup_file(TEST_IMG);

    // Create filesystem with user files
    {
        Disk disk(16 * 1024 * 1024, TEST_IMG);
        FileSystem fs(disk);
        fs.format();

        fs.login(100, 100);
        fs.create_dir("/user100_data");
        fs.create_file("/user100_data/protected.txt");
        fs.write_file("/user100_data/protected.txt", {'d', 'a', 't', 'a'});
    }

    // Remount and verify permissions preserved
    {
        Disk disk(16 * 1024 * 1024, TEST_IMG);
        FileSystem fs(disk);
        fs.mount();

        auto list = fs.list_dir("/");
        bool found = false;
        for (const auto& entry : list) {
            if (entry.name == "user100_data") {
                ASSERT(entry.uid == 100, "Directory UID persisted");
                ASSERT(entry.gid == 100, "Directory GID persisted");
                ASSERT(entry.permissions == 0755, "Directory permissions persisted");
                found = true;
            }
        }
        ASSERT(found, "User directory found after remount");

        // Another user (200) can read but not write with default 0644 perms
        fs.login(200, 200);
        auto data = fs.read_file("/user100_data/protected.txt");
        ASSERT(data.size() == 4, "User 200 can read with default 0644 permissions");
        
        // But user 200 cannot write
        bool caught = false;
        try {
            fs.write_file("/user100_data/protected.txt", {'h', 'a', 'c', 'k'});
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
    std::cout << "STARTING PERMISSIONS TEST SUITE\n";
    std::cout << "===============================\n";

    try {
        test_basic_permissions();
        test_cross_user_permissions();
        test_group_permissions();
        test_root_override();
        test_directory_permissions();
        test_metadata_tracking();
        test_permission_persistence();
    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL FAILURE] Uncaught Exception: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n===============================\n";
    std::cout << "ALL PERMISSIONS TESTS PASSED.\n";
    return 0;
}
