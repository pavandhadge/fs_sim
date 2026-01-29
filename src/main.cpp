#include "fs/filesystem.hpp"
#include "fs/disk.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

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

int main() {
    // 1. Initialize System (10 MB Disk)
    const size_t DISK_SIZE = 10 * 1024 * 1024;
    std::cout << "[System] Initializing " << (DISK_SIZE / 1024 / 1024) << "MB Disk...\n";

    Disk disk(DISK_SIZE);
    FileSystem fs(disk);

    // 2. Auto-Format on startup (since RAM disk is empty)
    try {
        fs.format();
    } catch (const std::exception& e) {
        std::cerr << "[Critical Error] Format failed: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n=== File System REPL ===\n";
    std::cout << "Commands: ls, touch, mkdir, rm, rmdir, write, read, format, exit\n";

    // 3. REPL Loop
    std::string line;
    while (true) {
        std::cout << "\nfs> ";
        if (!std::getline(std::cin, line)) break; // Handle EOF
        if (line.empty()) continue;

        std::vector<std::string> args = parse_command(line);
        std::string cmd = args[0];

        try {
            if (cmd == "exit") {
                break;
            }
            else if (cmd == "format") {
                fs.format();
            }
            else if (cmd == "mount") {
                fs.mount();
            }
            else if (cmd == "ls") {
                std::string path = (args.size() > 1) ? args[1] : "/";
                auto files = fs.list_dir(path);

                std::cout << "Listing '" << path << "':\n";
                if (files.empty()) std::cout << "(empty)\n";
                for (const auto& name : files) {
                    std::cout << "  " << name << "\n";
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

                // Combine all remaining tokens into one content string
                std::string content;
                size_t first_space = line.find(' ', line.find(' ') + 1); // Find space after path
                if (first_space != std::string::npos) {
                    content = line.substr(first_space + 1);
                }

                std::vector<uint8_t> data(content.begin(), content.end());
                fs.write_file(args[1], data);
            }
            else if (cmd == "read") {
                if (args.size() < 2) throw std::runtime_error("Usage: read <path>");

                std::vector<uint8_t> data = fs.read_file(args[1]);

                // Convert to string for printing
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
