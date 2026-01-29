#include "fs/filesystem.hpp"
#include "fs/block_group_manager.hpp"
#include "fs/disk.hpp"
#include "fs/disk_datastructures.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include "util/tokenizer.h"

FileSystem::FileSystem(Disk& disk_allocated) : disk(disk_allocated) {
    // We do NOT create managers here because we don't know
    // the disk geometry yet (until we Mount or Format).
    this->sb = new SuperBlock();
}


// ==========================================
// FORMAT: The "Factory Reset" (Write Only)
// ==========================================
void FileSystem::format() {
    std::cout << "Formatting Disk...\n";

    // 1. Wipe the ENTIRE disk with zeros
    // This ensures no old data/inodes confuse us later.
    std::vector<uint8_t> zeros(4096, 0);
    for (int i = 0; i < disk.get_block_count(); i++) {
        disk.write_block(i, zeros.data());
    }

    // 2. Configure the SuperBlock
    // We calculate the geometry based on the actual physical disk size.
    sb->magic_number = 0xF5513001;
    sb->total_blocks = disk.get_block_count();
    sb->blocks_per_group = 4096;
    sb->inodes_per_group = 4096;
    sb->total_inodes = (sb->total_blocks / sb->blocks_per_group) * sb->inodes_per_group;

    // 3. Write SuperBlock to Disk (Block 0)
    disk.write_block(0, sb);

    // 4. Force a Mount to initialize our Managers
    // We need managers to exist before we can allocate the Root Inode!
    mount();

    // 5. Create the Root Directory (Inode 1)
    // Now that managers exist, we can use them.
    int root_id = block_group_managers[0].allocate_inode();

    // Safety check: The first allocation on a fresh disk should be Inode 1
    // (Assuming Inode 0 is skipped/reserved in your manager logic)
    if (root_id == -1) throw std::runtime_error("Failed to create Root Inode");

    // 6. Initialize Root Inode
    Inode* root = block_group_managers[0].get_inode(root_id);
    root->file_type = FS_DIRECTORY;
    root->id = root_id;
    root->file_size = 0;

    std::cout << "Disk Formatted. Root Inode ID: " << root_id << "\n";
}

// ==========================================
// MOUNT: The "Boot Up" (Read Only)
// ==========================================
void FileSystem::mount() {
    // 1. Read SuperBlock from Block 0
    uint8_t* buffer = disk.get_ptr(0);
    SuperBlock* disk_sb = reinterpret_cast<SuperBlock*>(buffer);

    // 2. Verification (The Handshake)
    if (disk_sb->magic_number != 0xF5513001) {
        throw std::runtime_error("Error: Invalid FileSystem (Bad Magic Number)");
    }

    // 3. Load State (Disk -> Memory)
    // We copy the data so we can use 'this->sb' safely
    std::memcpy(this->sb, disk_sb, sizeof(SuperBlock));

    // 4. Initialize Managers
    // Now we know the geometry, so we can create the managers.
    int total_groups = sb->total_blocks / sb->blocks_per_group;

    block_group_managers.clear();
    block_group_managers.reserve(total_groups);

    for (int i = 0; i < total_groups; i++) {
        // Create a manager for each group
        block_group_managers.emplace_back(disk, sb, i);
    }

    std::cout << "FileSystem Mounted. Groups: " << total_groups << "\n";
}



// Returns the Inode ID if found, or -1 (or throws) if not found
size_t FileSystem::find_inode_in_dir(Inode* parent_inode, const std::string& name) {
    // 1. Iterate through the 12 direct blocks of the directory
    for (int i = 0; i < 12; i++) {
        size_t block_id = parent_inode->direct_blocks[i];

        // If block_id is 0, it means we reached the end of the directory data
        if (block_id == 0) break;

        // 2. Read the block
        uint8_t buffer[4096];
        disk.read_block(block_id, buffer);

        // 3. Cast to DirEntry array
        DirEntry* entry = reinterpret_cast<DirEntry*>(buffer);
        int max_entries = 4096 / sizeof(DirEntry);

        // 4. Scan the entries
        for (int j = 0; j < max_entries; j++) {
            // Check if entry is valid AND name matches
            if (entry[j].inode_id != 0 &&
                std::strncmp(entry[j].name, name.c_str(), 255) == 0) {
                return entry[j].inode_id; // Found it!
            }
        }
    }

    return 0; // Not Found (0 is usually reserved/invalid in our logic)
}

// Helper: Returns the pointer to the Inode, no matter which group it is in
Inode* FileSystem::get_global_inode_ptr(size_t global_id) {
    // 1. Calculate which group owns this ID
    size_t group_index = global_id / sb->inodes_per_group;

    // 2. Safety Check
    if (group_index >= block_group_managers.size()) {
        throw std::runtime_error("Inode ID out of bounds!");
    }

    // 3. Ask that specific manager
    // Note: We access the vector at [group_index]
    return block_group_managers[group_index].get_inode(global_id);
}

//global inode id of hte parent
size_t FileSystem::traverse_path_till_parent(std::vector<std::string>& tokenized_path) {
    size_t current_id = sb->home_dir_inode;

    if (tokenized_path.size() <= 1) return current_id;

    for (size_t i = 0; i < tokenized_path.size() - 1; i++) {

        // --- FIXED: DYNAMIC DISPATCH ---
        // Old Way: block_group_managers[0].get_inode(...) -> WRONG
        // New Way: Use the helper to find the right group automatically
        Inode* current_inode = get_global_inode_ptr(current_id);
        // -------------------------------

        if (current_inode->file_type != FS_DIRECTORY) {
            throw std::runtime_error("Invalid Path: Not a directory : "+tokenized_path[i]);
        }

        // Search inside this inode (The logic for this function stays the same)
        size_t next_id = find_inode_in_dir(current_inode, tokenized_path[i]);

        if (next_id == 0) {
            throw std::runtime_error("Path not found.");
        }

        current_id = next_id;
    }

    return current_id;
}




void FileSystem::add_entry_to_dir(Inode* parent_inode, size_t newfile_id, std::string filename) {
    int max_entries = 4096 / sizeof(DirEntry); // Fixed: 4096, not sizeof pointer

    // Loop through the 12 possible direct pointers
    for (int i = 0; i < 12; i++) {
        size_t curr_block_id = parent_inode->direct_blocks[i];

        // CASE 1: Block is not allocated yet.
        // We need to grow the directory!
        if (curr_block_id == 0) {
            // 1. Allocate a new block from the same group as the parent
            // (You might need a helper to find which group the parent is in)
            int group_idx = parent_inode->id / sb->inodes_per_group;
            int new_block = block_group_managers[group_idx].allocate_block();

            if (new_block == -1) throw std::runtime_error("Disk Full: Cannot grow directory");

            // 2. Link it to the parent
            parent_inode->direct_blocks[i] = new_block;
            curr_block_id = new_block;
        }

        // CASE 2: Scan the block for an empty slot
        uint8_t* block_ptr = disk.get_ptr(curr_block_id);
        DirEntry* entry = reinterpret_cast<DirEntry*>(block_ptr);

        // Fixed: Use 'j' to avoid shadowing 'i'
        for (int j = 0; j < max_entries; j++) {
            // Found an empty slot (ID 0 means free)
            if (entry[j].inode_id == 0) {

                // 1. Link the ID
                entry[j].inode_id = newfile_id;

                // 2. Copy the Name safely (Fixed: strncpy)
                // Clear memory first to remove old garbage
                std::memset(entry[j].name, 0, 255);
                std::strncpy(entry[j].name, filename.c_str(), 254);

                // 3. Set Length
                entry[j].name_len = static_cast<uint8_t>(filename.size());


                // 4. Update Parent Size (Crucial for `ls`)
                // We physically added an entry, so we increase the size count
                parent_inode->file_size += sizeof(DirEntry);

                return; // Done!
            }
        }
    }

    throw std::runtime_error("Directory Full: Limit of 12 blocks reached.");
}

void FileSystem::read_direct_block_to_buffer(Inode* file, uint8_t* buffer) {
    int block_size = disk.get_block_size(); // Usually 4096

    // Iterate through the 12 direct blocks
    for (int i = 0; i < 12; i++) {
        size_t block_id = file->direct_blocks[i];

        // Stop if we hit an unallocated block
        if (block_id == 0) break;

        // Calculate the destination address in the large buffer
        // logic: Start of buffer + (Block Index * 4096)
        uint8_t* dest_ptr = buffer + (i * block_size);

        // Read DIRECTLY into the final buffer. No temp buffer needed.
        disk.read_block(block_id, dest_ptr);
    }
}

// Returns a Vector (Safe, manages its own memory)
std::vector<uint8_t> FileSystem::read_file(std::string path) {
    // 1. Resolve Path
    auto tokenized_path = tokenize_path(path, '/');
    std::string filename = tokenized_path.back();

    // Find Parent
    size_t parent_id = traverse_path_till_parent(tokenized_path);
    Inode* parent_inode = get_global_inode_ptr(parent_id);

    // 2. Find the File Inode
    size_t file_id = find_inode_in_dir(parent_inode, filename);
    if (file_id == 0) {
        throw std::runtime_error("File not found: " + path);
    }

    Inode* file_inode = get_global_inode_ptr(file_id);

    // 3. Prepare Buffer
    // We allocate exactly as much size as the file needs
    // (Or 48KB max if you want to read all blocks)
    size_t max_size = 12 * disk.get_block_size();
    std::vector<uint8_t> file_content(max_size);

    // 4. Read Data
    // We pass .data() which gives the raw pointer to the vector's internal array
    read_direct_block_to_buffer(file_inode, file_content.data());

    // Optional: Resize the vector to the actual file size (if you store file_size in Inode)
    // file_content.resize(file_inode->file_size);

    return file_content;
}


void FileSystem::delete_file(std::string path) {
    auto tokenized_path = tokenize_path(path, '/');
    std::string filename = tokenized_path.back();

    // 1. Find Parent Directory
    size_t parent_id = traverse_path_till_parent(tokenized_path);
    Inode* parent_inode = get_global_inode_ptr(parent_id);

    // 2. Find the entry in the parent directory
    bool found = false;
    int max_entries = 4096 / sizeof(DirEntry);

    for (int i = 0; i < 12; i++) {
        size_t block_id = parent_inode->direct_blocks[i];
        if (block_id == 0) break;

        // Get direct pointer to disk memory
        uint8_t* block_ptr = disk.get_ptr(block_id);
        DirEntry* entry = reinterpret_cast<DirEntry*>(block_ptr);

        for (int j = 0; j < max_entries; j++) {
            // Check if this is the file we want
            if (entry[j].inode_id != 0 &&
                std::strncmp(entry[j].name, filename.c_str(), 255) == 0) {

                // FOUND IT!
                size_t inode_to_delete = entry[j].inode_id;

                // Step A: Wipe the Directory Entry (Create a hole)
                // We just set ID to 0. Your add_entry will reuse this slot later.
                std::memset(&entry[j], 0, sizeof(DirEntry));

                // Step B: Decrease Parent Size
                parent_inode->file_size -= sizeof(DirEntry);

                // Step C: Free the actual resources (The heavy lifting)
                release_file_resources(inode_to_delete);

                found = true;
                std::cout << "File '" << filename << "' deleted successfully.\n";
                return; // We are done
            }
        }
    }

    if (!found) {
        throw std::runtime_error("Error: File '" + filename + "' does not exist.");
    }
}

void FileSystem::release_file_resources(size_t inode_id) {
    // 1. Get the Inode so we know which blocks it owns
    Inode* file_inode = get_global_inode_ptr(inode_id);

    // 2. Free all Data Blocks used by this file
    for (int i = 0; i < 12; i++) {
        if (file_inode->direct_blocks[i] != 0) {
            // Calculate which group owns this block to free it correctly
            // (Assuming you have a helper or global block_free function)
            int block_global_id = file_inode->direct_blocks[i];

            // For simplicity, finding the manager:
            int group_id = block_global_id / sb->blocks_per_group;
            block_group_managers[group_id].free_block(block_global_id);

            file_inode->direct_blocks[i] = 0;
        }
    }

    // 3. Free the Inode itself
    int group_id = inode_id / sb->inodes_per_group;
    block_group_managers[group_id].free_inode(inode_id);
}



void FileSystem::delete_dir(std::string path) {
    auto tokenized_path = tokenize_path(path, '/');
    std::string dirname = tokenized_path.back();

    // 1. Find Parent
    size_t parent_id = traverse_path_till_parent(tokenized_path);
    Inode* parent_inode = get_global_inode_ptr(parent_id);

    // 2. Find and Unlink the Directory Entry (Same code as delete_file)
    bool found = false;
    int max_entries = 4096 / sizeof(DirEntry);

    for (int i = 0; i < 12; i++) {
        size_t block_id = parent_inode->direct_blocks[i];
        if (block_id == 0) break;

        uint8_t* block_ptr = disk.get_ptr(block_id);
        DirEntry* entry = reinterpret_cast<DirEntry*>(block_ptr);

        for (int j = 0; j < max_entries; j++) {
            if (entry[j].inode_id != 0 &&
                std::strncmp(entry[j].name, dirname.c_str(), 255) == 0) {

                size_t target_inode_id = entry[j].inode_id;

                // CRITICAL CHECK: Ensure it is actually a directory
                Inode* target_inode = get_global_inode_ptr(target_inode_id);
                if (target_inode->file_type != FS_DIRECTORY) {
                    throw std::runtime_error("Error: Path is not a directory.");
                }

                // Step A: Recursively delete everything inside
                recursive_resource_release(target_inode_id);

                // Step B: Wipe the Entry from Parent
                std::memset(&entry[j], 0, sizeof(DirEntry));
                parent_inode->file_size -= sizeof(DirEntry);

                found = true;
                std::cout << "Directory '" << dirname << "' deleted recursively.\n";
                return;
            }
        }
    }

    if (!found) throw std::runtime_error("Directory not found.");
}

void FileSystem::recursive_resource_release(size_t dir_inode_id) {
    Inode* dir_inode = get_global_inode_ptr(dir_inode_id);

    // 1. Loop through all data blocks of this directory
    int max_entries = 4096 / sizeof(DirEntry);

    for (int i = 0; i < 12; i++) {
        size_t block_id = dir_inode->direct_blocks[i];
        if (block_id == 0) break;

        uint8_t* block_ptr = disk.get_ptr(block_id);
        DirEntry* entry = reinterpret_cast<DirEntry*>(block_ptr);

        // 2. Check every entry in the block
        for (int j = 0; j < max_entries; j++) {
            if (entry[j].inode_id != 0) {

                // Ignore special entries "." and ".." if you implemented them
                // if (strcmp(entry[j].name, ".") == 0 || strcmp(entry[j].name, "..") == 0) continue;

                Inode* child_inode = get_global_inode_ptr(entry[j].inode_id);

                if (child_inode->file_type == FS_DIRECTORY) {
                    // RECURSION: Dive deeper!
                    recursive_resource_release(entry[j].inode_id);
                } else {
                    // BASE CASE: It's a file, just free it
                    release_file_resources(entry[j].inode_id);
                }
            }
        }

        // 3. Free the directory data block itself (since we emptied it)
        // Note: You need a helper to find which group owns this block
        int group_id = block_id / sb->blocks_per_group;
        block_group_managers[group_id].free_block(block_id);
    }

    // 4. Finally, free the Directory Inode itself
    int group_id = dir_inode_id / sb->inodes_per_group;
    block_group_managers[group_id].free_inode(dir_inode_id);
}


// [Private Helper]
// Handles the creation logic for both Files and Directories.
void FileSystem::create_fs_entry(std::string path, FS_FILE_TYPES type) {
    auto tokenized_path = tokenize_path(path, '/');

    if (tokenized_path.empty()) {
        throw std::runtime_error("Error: Path cannot be empty.");
    }

    std::string filename = tokenized_path.back();

    // 1. Resolve Parent
    size_t parent_id = traverse_path_till_parent(tokenized_path);
    Inode* parent_inode = get_global_inode_ptr(parent_id);

    // 2. Existence Check
    if (find_inode_in_dir(parent_inode, filename) != 0) {
        throw std::runtime_error("Error: '" + filename + "' already exists.");
    }

    // 3. Allocate Inode (Safe Loop)
    int new_id = -1;
    for (int i = 0; i < block_group_managers.size(); i++) {
        new_id = block_group_managers[i].allocate_inode();
        if (new_id != -1) break;
    }

    if (new_id == -1) throw std::runtime_error("Disk Full: Unable to allocate inode.");

    // 4. Initialize Inode
    Inode* new_inode = get_global_inode_ptr(new_id);
    new_inode->id = new_id;
    new_inode->file_size = 0;
    new_inode->file_type = type; // <--- Uses the passed type correctly
    std::memset(new_inode->direct_blocks, 0, sizeof(new_inode->direct_blocks));

    // 5. Link to Parent
    add_entry_to_dir(parent_inode, new_id, filename);

    // Optional: Print specific message
    std::string type_str = (type == FS_DIRECTORY) ? "Directory" : "File";
    std::cout << type_str << " '" << filename << "' created successfully (ID: " << new_id << ")\n";
}


// [Public]
void FileSystem::create_file(std::string path) {
    // Just calls the worker with FS_FILE
    create_fs_entry(path, FS_FILE_TYPES::FS_FILE);
}

// [Public]
void FileSystem::create_dir(std::string path) {
    // Just calls the worker with FS_DIRECTORY
    create_fs_entry(path, FS_FILE_TYPES::FS_DIRECTORY);
}


// Returns a clean list of filenames (like 'ls')
std::vector<std::string> FileSystem::list_dir(std::string path) {
    // 1. Resolve Path (Same as read_file)
    auto tokenized_path = tokenize_path(path, '/');

    // Edge case: If path is "/", traverse logic needs care.
    // Assuming traverse handles it or we handle root specially:
    size_t target_id;
    if (tokenized_path.empty()) {
        target_id = sb->home_dir_inode; // Root
    } else {
        // Reuse your existing logic to get the directory inode
        std::string dirname = tokenized_path.back();
        size_t parent_id = traverse_path_till_parent(tokenized_path);
        Inode* parent_inode = get_global_inode_ptr(parent_id);
        target_id = find_inode_in_dir(parent_inode, dirname);
    }

    if (target_id == 0) throw std::runtime_error("Directory not found.");

    Inode* dir_inode = get_global_inode_ptr(target_id);
    if (dir_inode->file_type != FS_DIRECTORY) {
        throw std::runtime_error("Path is not a directory.");
    }

    // 2. Read Raw Data (Reuse your logic!)
    // We can manually call the helper to get the raw buffer
    size_t max_size = 12 * disk.get_block_size();
    std::vector<uint8_t> raw_buffer(max_size);
    read_direct_block_to_buffer(dir_inode, raw_buffer.data());

    // 3. Parse the Buffer (The "Directory" difference)
    std::vector<std::string> file_list;
    DirEntry* entries = reinterpret_cast<DirEntry*>(raw_buffer.data());
    int max_entries = max_size / sizeof(DirEntry);

    for (int i = 0; i < max_entries; i++) {
        // Only include slots that are actually used (ID != 0)
        if (entries[i].inode_id != 0) {
            file_list.push_back(entries[i].name);
        }
    }

    return file_list;
}
