#include "fs/filesystem.hpp"
#include "fs/block_group_manager.hpp"
#include "fs/disk.hpp"
#include "fs/disk_datastructures.hpp"
#include <cstddef>
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

//inode id of hte parent
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
void FileSystem::read_dir(std::string path){
    auto tokenized_path = tokenize_path(path, '/');

}


void FileSystem::create_file(std::string path) {
    auto tokenized_path = tokenize_path(path, '/');
    std::string filename = tokenized_path.back(); // Get "file.txt"

    // 1. Get Parent Directory
    size_t parent_inodeid = traverse_path_till_parent(tokenized_path);
    Inode* parent_inode = get_global_inode_ptr(parent_inodeid);

    // Safety: Check if file already exists in parent
    if (find_inode_in_dir(parent_inode, filename) != 0) {
        throw std::runtime_error("Error: File already exists.");
    }

    // 2. Allocate Inode (With Safe Loop)
    int newfile_id = -1;
    for (int i = 0; i < block_group_managers.size(); i++) {
        newfile_id = block_group_managers[i].allocate_inode();
        if (newfile_id != -1) break; // Found one!
    }

    if (newfile_id == -1) {
        throw std::runtime_error("Disk Full: Unable to allocate inode.");
    }

    // 3. Initialize the Inode
    // Note: get_global_inode_ptr gives direct access to Disk Memory.
    // Assigning values here writes them to the disk immediately.
    Inode* newfile_inode = get_global_inode_ptr(newfile_id);
    newfile_inode->id = newfile_id;
    newfile_inode->file_size = 0;
    newfile_inode->file_type = FS_FILE_TYPES::FS_FILE;
    // Important: Zero out the block pointers just in case
    std::memset(newfile_inode->direct_blocks, 0, sizeof(newfile_inode->direct_blocks));

    // 4. THE MISSING LINK: Add entry to Parent Directory
    // You need to write a helper for this (see logic below)
    add_entry_to_dir(parent_inode, newfile_id, filename);

    std::cout << "File " << filename << " successfully created with ID " << newfile_id << "\n";
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
