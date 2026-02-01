#include "fs/filesystem.hpp"
#include "fs/disk.hpp"
#include <iostream>
#include <vector>
#include <string>

// Reusing your ASSERT macro
#define ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "[FAIL] " << message << " (" << #condition << ")\n"; \
        std::exit(1); \
    } else { \
        std::cout << "[PASS] " << message << "\n"; \
    }

void test_permissions() {
    std::cout << "\n=== Test 5: Multi-User Permissions ===\n";
    const char* TEST_IMG = "test_perms.img";
    std::remove(TEST_IMG);

    Disk disk(16 * 1024 * 1024, TEST_IMG);
    FileSystem fs(disk);
    fs.format(); // Root (0) is active by default

    // 1. Setup: Root creates a shared folder
    fs.create_dir("/shared");
    
    // 2. User 100 creates a private file
    fs.login(100, 100);
    fs.create_file("/shared/u100.txt");
    std::string secret = "User 100 Secret";
    fs.write_file("/shared/u100.txt", {secret.begin(), secret.end()});
    
    // 3. User 200 tries to overwrite User 100's file (Should Fail)
    fs.login(200, 200);
    bool caught_write = false;
    try {
        std::string hack = "Hacked!";
        fs.write_file("/shared/u100.txt", {hack.begin(), hack.end()});
    } catch (const std::runtime_error& e) {
        caught_write = true;
        std::cout << "-> Correctly blocked User 200 write: " << e.what() << "\n";
    }
    ASSERT(caught_write, "Permission denied on unauthorized write");

    // 4. User 200 tries to delete User 100's file (Should Fail)
    // Deletion requires write permission on the parent directory (/shared)
    bool caught_delete = false;
    try {
        fs.delete_file("/shared/u100.txt");
    } catch (const std::runtime_error& e) {
        caught_delete = true;
        std::cout << "-> Correctly blocked User 200 delete: " << e.what() << "\n";
    }
    ASSERT(caught_delete, "Permission denied on unauthorized delete");

    // 5. Root Override Test (Should Pass)
    fs.logout(); // Back to UID 0
    try {
        fs.delete_file("/shared/u100.txt");
        ASSERT(true, "Root successfully bypassed permissions to delete file");
    } catch (...) {
        ASSERT(false, "Root was incorrectly blocked from deleting a file");
    }

    // 6. Metadata Verification
    fs.create_file("/root_file");
    auto list = fs.list_dir("/");
    bool found_root_file = false;
    for(const auto& entry : list) {
        if(entry.name == "root_file") {
            ASSERT(entry.uid == 0, "Root file UID is 0");
            ASSERT(entry.permissions == 0644, "Default file perms are 0644");
            found_root_file = true;
        }
    }
    ASSERT(found_root_file, "Metadata correctly stored and retrieved");

    std::remove(TEST_IMG);
}

int main() {
    std::cout << "STARTING PERMISSION TEST (Legacy)\n";
    std::cout << "=================================\n";
    try {
        test_permissions();
    } catch (const std::exception& e) {
        std::cerr << "\n[CRITICAL FAILURE] Uncaught Exception: " << e.what() << "\n";
        return 1;
    }
    std::cout << "\n=================================\n";
    std::cout << "PERMISSION TEST PASSED.\n";
    return 0;
}