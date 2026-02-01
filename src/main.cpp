#include "fs/filesystem.hpp"
#include "fs/disk.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <limits> // Required for numeric_limits

// Helper to split command line arguments
std::vector<std::string> parse_command(const std::string& input) {
    std::vector<std::string> tokens;
    std::stringstream ss(input);
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string format_permissions(uint16_t p, bool is_dir) {
    std::string res = is_dir ? "d" : "-";
    const char* chars = "rwx";
    for (int i = 6; i >= 0; i -= 3) {
        res += (p & (4 << i)) ? 'r' : '-';
        res += (p & (2 << i)) ? 'w' : '-';
        res += (p & (1 << i)) ? 'x' : '-';
    }
    return res;
}

int main() {
    // 1. User Input for Disk Size
    size_t size_mb = 0;
    while (true) {
        std::cout << "Enter disk size in MB (must be multiple of 16): ";

        if (!(std::cin >> size_mb)) {
            // Validate integer input
            std::cin.clear(); // Clear error flag
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Discard bad input
            std::cout << "Invalid input. Please enter a number.\n";
            continue;
        }

        if (size_mb > 0 && size_mb % 16 == 0) {
            break; // Input is valid
        } else {
            std::cout << "Error: Size must be positive and a multiple of 16 (e.g., 16, 32, 64).\n";
        }
    }

    // CRITICAL: Clear the newline left in the buffer by std::cin >>
    // Otherwise, the first std::getline in the REPL will be skipped.
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    const size_t DISK_SIZE = size_mb * 1024 * 1024;
    const char* DISK_NAME = "my_fs.img";

    std::cout << "\n[System] Initializing " << size_mb << "MB Disk backed by '" << DISK_NAME << "'...\n";

    // 2. Initialize Hardware & Driver
    // If the file exists but has a different size, Disk constructor will resize (ftruncate) it.
    Disk disk(DISK_SIZE, DISK_NAME);
    FileSystem fs(disk);

    // 3. Smart Startup: Try to Mount, otherwise Format
    try {
        std::cout << "[System] Attempting to mount existing file system...\n";
        fs.mount();

        // Optional: Check if the mounted FS size matches the physical disk size
        // (If you grew the disk from 16MB to 32MB, you might want to warn the user)
        // For now, we assume if it mounts, it's usable.
        std::cout << "[System] Mount successful! Data preserved.\n";

    } catch (const std::exception& e) {
        std::cout << "[System] Mount failed or new disk detected (" << e.what() << ").\n";
        std::cout << "[System] Formatting new file system...\n";
        try {
            fs.format();
        } catch (const std::exception& ex) {
            std::cerr << "[Critical Error] Format failed: " << ex.what() << "\n";
            return 1;
        }
    }

    std::cout << "\n=== File System REPL ===\n";
    std::cout << "Commands: ls, touch, mkdir, rm, rmdir, write, read, format, login, logout, whoami, exit\n";
    std::cout << "Note: Changes are automatically saved when you 'exit'.\n";

    // 4. REPL Loop
    std::string line;
    while (true) {
        // Display current user in the prompt (like a real Linux terminal)
        uint16_t uid = fs.get_current_user();
        std::cout << "\n[user:" << uid << "] fs> ";

        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::vector<std::string> args = parse_command(line);
        std::string cmd = args[0];

        try {
            if (cmd == "exit") {
                std::cout << "[System] Syncing to disk and exiting...\n";
                break;
            }
            // --- NEW: LOGIN COMMAND ---
            else if (cmd == "login") {
                if (args.size() < 3) throw std::runtime_error("Usage: login <uid> <gid>");
                uint16_t new_uid = std::stoi(args[1]);
                uint16_t new_gid = std::stoi(args[2]);
                fs.login(new_uid, new_gid);
            }
            // --- NEW: LOGOUT COMMAND ---
            else if (cmd == "logout") {
                fs.logout();
            }
            // --- NEW: WHOAMI COMMAND ---
            else if (cmd == "whoami") {
                std::cout << "Current UID: " << fs.get_current_user() << "\n";
            }
            else if (cmd == "format") {
                std::cout << "[Warning] This will erase all data. Confirm? (y/n): ";
                std::string confirm;
                std::getline(std::cin, confirm);
                if (confirm == "y") {
                    fs.format();
                } else {
                    std::cout << "Format cancelled.\n";
                }
            }
            else if (cmd == "mount") {
                fs.mount();
            }
            else if (cmd == "ls") {
                std::string path = (args.size() > 1) ? args[1] : "/";
                auto entries = fs.list_dir(path);

                std::cout << "Listing '" << path << "':\n";
                if (entries.empty()) std::cout << "(empty)\n";
                for (const auto& entry : entries) {
                    std::cout << format_permissions(entry.permissions, entry.is_directory)
                      << "  " << entry.uid 
                      << "  " << entry.gid 
                      << "  " << entry.name << "\n";
                }
            }
            else if (cmd == "mkdir") {
                if (args.size() < 2) throw std::runtime_error("Usage: mkdir <path>");
                fs.create_dir(args[1]);
            }
            else if (cmd == "touch") {
                if (args.size() < 2) throw std::runtime_error("Usage: touch <path>");
                fs.create_file(args[1]);
            }
            else if (cmd == "rm") {
                if (args.size() < 2) throw std::runtime_error("Usage: rm <path>");
                fs.delete_file(args[1]);
            }
            else if (cmd == "rmdir") {
                if (args.size() < 2) throw std::runtime_error("Usage: rmdir <path>");
                fs.delete_dir(args[1]);
            }
            else if (cmd == "write") {
                if (args.size() < 3) throw std::runtime_error("Usage: write <path> <content>");

                std::string content;
                size_t first_space = line.find(' ', line.find(' ') + 1);
                if (first_space != std::string::npos) {
                    content = line.substr(first_space + 1);
                }

                std::vector<uint8_t> data(content.begin(), content.end());
                fs.write_file(args[1], data);
            }
            else if (cmd == "read") {
                if (args.size() < 2) throw std::runtime_error("Usage: read <path>");

                std::vector<uint8_t> data = fs.read_file(args[1]);
                std::string text(data.begin(), data.end());
                std::cout << text << "\n";
            }
            else {
                std::cout << "Unknown command: " << cmd << "\n";
            }

        } catch (const std::exception& e) {
            std::cout << "[Error] " << e.what() << "\n";
        }
    }

    return 0;
}
