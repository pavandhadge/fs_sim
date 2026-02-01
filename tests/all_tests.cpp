#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>

// Test function declarations
extern int disk_test_main();
extern int fs_operations_test_main();
extern int fs_directory_test_main();
extern int fs_permissions_test_main();
extern int fs_persistence_test_main();
extern int fs_stress_test_main();

// Forward declarations - each test file should have a unique main function
// We'll rename them in the actual test files

struct TestSuite {
    std::string name;
    int (*run)();
};

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║          FILESYSTEM COMPREHENSIVE TEST SUITE             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    // Note: Individual test binaries are compiled separately
    // This is a placeholder showing the test structure
    
    std::cout << "Test Suite Structure:\n";
    std::cout << "--------------------\n";
    std::cout << "1. disk_test           - Disk-level operations (read/write/pointer)\n";
    std::cout << "2. fs_operations_test  - File operations (create/write/read/delete)\n";
    std::cout << "3. fs_directory_test   - Directory and path handling\n";
    std::cout << "4. fs_permissions_test - Permission system (owner/group/other/root)\n";
    std::cout << "5. fs_persistence_test - Mount/remount data persistence\n";
    std::cout << "6. fs_stress_test      - Stress tests and edge cases\n";
    std::cout << "\n";

    std::cout << "To run all tests, use: make check\n";
    std::cout << "Or run individual tests:\n";
    std::cout << "  ./disk_test\n";
    std::cout << "  ./fs_operations_test\n";
    std::cout << "  ./fs_directory_test\n";
    std::cout << "  ./fs_permissions_test\n";
    std::cout << "  ./fs_persistence_test\n";
    std::cout << "  ./fs_stress_test\n";
    std::cout << "\n";

    return 0;
}
