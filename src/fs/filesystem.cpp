#include "fs/filesystem.hpp"
#include "fs/block_group_manager.hpp"
#include "fs/disk.hpp"
#include "fs/disk_datastructures.hpp"
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include "util/tokenizer.h"

FileSystem::FileSystem(Disk& disk_allocated) : disk(disk_allocated) {
    // We do NOT create managers here because we don't know
    // the disk geometry yet (until we Mount or Format).
    this->sb = new SuperBlock();
}

FileSystem::~FileSystem() {
    delete this->sb;
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


void FileSystem::create_file(std::string path){

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
            throw std::runtime_error("Invalid Path: Not a directory.");
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
void FileSystem::read_dir(std::string path){
    auto tokenized_path = tokenize_path(path, '/');



}
