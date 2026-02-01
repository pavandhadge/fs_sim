#include "fs/filesystem.hpp"
#include "util/tokenizer.h"
#include <iostream>
#include <cstring>
#include <algorithm> // GEMINI FIX: for std::min

FileSystem::FileSystem(Disk& disk_allocated) : disk(disk_allocated) {
    this->sb = new SuperBlock();
}

void FileSystem::format() {
    std::cout << "Formatting Disk...\n";

    // 1. Wipe the ENTIRE disk with zeros
    std::vector<uint8_t> zeros(disk.get_block_size(), 0);
    for (size_t i = 0; i < disk.get_block_count(); i++) {
        disk.write_block(i, zeros.data());
    }

    // 2. Configure the SuperBlock object in memory
    sb->magic_number = 0xF5513001;
    sb->total_blocks = disk.get_block_count();

    // Logic for small disks (tests) vs large disks
    if (sb->total_blocks < 4096) {
        sb->blocks_per_group = sb->total_blocks;
        sb->inodes_per_group = sb->total_blocks;
    } else {
        sb->blocks_per_group = 4096;
        sb->inodes_per_group = 4096;
    }

    // Use Ceiling Division to calculate groups
    size_t group_count = (sb->total_blocks + sb->blocks_per_group - 1) / sb->blocks_per_group;
    sb->total_inodes = group_count * sb->inodes_per_group;
    sb->home_dir_inode = 0; // Temp

    // 3. Write SuperBlock to Disk SAFELY
    // GEMINI FIX: Create a zeroed 4KB buffer, copy SB into it, then write.
    // This prevents reading garbage heap memory past the struct.
    std::vector<uint8_t> sb_buffer(disk.get_block_size(), 0);
    std::memcpy(sb_buffer.data(), sb, sizeof(SuperBlock));
    disk.write_block(0, sb_buffer.data());

    // 4. Mount to initialize managers
    mount();

    // 5. Create Root Inode
    int root_id = block_group_managers[0].allocate_inode();
    if (root_id == -1) throw std::runtime_error("Failed to create Root Inode");

    // 6. Update SuperBlock with Root ID
    sb->home_dir_inode = root_id;

    // 7. Initialize Root Inode
    Inode* root = block_group_managers[0].get_inode(root_id);
    root->file_type = FS_DIRECTORY;
    root->id = root_id;
    root->file_size = 0;
    root->uid = 0; // Root owned
    root->gid = 0; // Root group
    root->permissions = 0755; // <--- ADD THIS: Ensure root is rwxr-xr-x
    // Explicitly zero block pointers (redundant but safe)
    std::memset(root->direct_blocks, 0, sizeof(root->direct_blocks));

    // 8. Write Updated SuperBlock to Disk (Final Update)
    std::memcpy(sb_buffer.data(), sb, sizeof(SuperBlock)); // Update buffer
    disk.write_block(0, sb_buffer.data());

    std::cout << "Disk Formatted. Root Inode ID: " << root_id << "\n";
}

// ==========================================
// MOUNT: The "Boot Up" (Read Only)
// ==========================================
void FileSystem::mount() {
    uint8_t* buffer = disk.get_ptr(0);
    SuperBlock* disk_sb = reinterpret_cast<SuperBlock*>(buffer);

    if (disk_sb->magic_number != 0xF5513001) {
        throw std::runtime_error("Error: Invalid FileSystem (Bad Magic Number)");
    }

    std::memcpy(this->sb, disk_sb, sizeof(SuperBlock));

    // GEMINI FIX: Use Ceiling Division here too
    int total_groups = (sb->total_blocks + sb->blocks_per_group - 1) / sb->blocks_per_group;

    block_group_managers.clear();
    block_group_managers.reserve(total_groups);

    for (int i = 0; i < total_groups; i++) {
        block_group_managers.emplace_back(disk, sb, i);
    }
    std::cout << "FileSystem Mounted. Groups: " << total_groups << "\n";
}

// ---------------- HELPERS ----------------

Inode* FileSystem::get_global_inode_ptr(size_t global_id) {
    size_t group_index = global_id / sb->inodes_per_group;
    if (group_index >= block_group_managers.size()) {
        throw std::runtime_error("Inode ID out of bounds!");
    }
    return block_group_managers[group_index].get_inode(global_id);
}

size_t FileSystem::find_inode_in_dir(Inode* parent_inode, const std::string& name) {
    for (int i = 0; i < 12; i++) {
        size_t block_id = parent_inode->direct_blocks[i];
        if (block_id == 0) break;

        uint8_t* buffer = disk.get_ptr(block_id);
        DirEntry* entry = reinterpret_cast<DirEntry*>(buffer);
        int max_entries = 4096 / sizeof(DirEntry);

        for (int j = 0; j < max_entries; j++) {
            if (entry[j].inode_id != 0 &&
                std::strncmp(entry[j].name, name.c_str(), 255) == 0) {
                return entry[j].inode_id;
            }
        }
    }
    return 0;
}

size_t FileSystem::traverse_path_till_parent(std::vector<std::string>& tokenized_path) {
    // GEMINI FIX: If path is empty (root), return root
    if (tokenized_path.empty()) return sb->home_dir_inode;

    size_t current_id = sb->home_dir_inode;

    // GEMINI FIX: If path is just "/file", parent is root.
    if (tokenized_path.size() == 1) return current_id;

    for (size_t i = 0; i < tokenized_path.size() - 1; i++) {
        Inode* current_inode = get_global_inode_ptr(current_id);
        if (current_inode->file_type != FS_DIRECTORY) {
            throw std::runtime_error("Invalid Path: '" + tokenized_path[i] + "' is not a directory.");
        }
        size_t next_id = find_inode_in_dir(current_inode, tokenized_path[i]);
        if (next_id == 0) {
            throw std::runtime_error("Path not found: " + tokenized_path[i]);
        }
        current_id = next_id;
    }
    return current_id;
}

void FileSystem::add_entry_to_dir(Inode* parent_inode, size_t newfile_id, std::string filename) {
    int max_entries = 4096 / sizeof(DirEntry);

    for (int i = 0; i < 12; i++) {
        size_t curr_block_id = parent_inode->direct_blocks[i];

        // Alloc new block if needed
        if (curr_block_id == 0) {
            int group_idx = parent_inode->id / sb->inodes_per_group;
            int new_block = block_group_managers[group_idx].allocate_block();
            if (new_block == -1) throw std::runtime_error("Disk Full: Cannot grow directory");

            parent_inode->direct_blocks[i] = new_block;
            curr_block_id = new_block;
        }

        DirEntry* entry = reinterpret_cast<DirEntry*>(disk.get_ptr(curr_block_id));
        for (int j = 0; j < max_entries; j++) {
            if (entry[j].inode_id == 0) {
                entry[j].inode_id = newfile_id;

                // GEMINI FIX: Safe copy
                std::memset(entry[j].name, 0, sizeof(entry[j].name));
                size_t len_to_copy = std::min(filename.size(), sizeof(entry[j].name) - 1);
                std::memcpy(entry[j].name, filename.c_str(), len_to_copy);
                entry[j].name_len = static_cast<uint8_t>(len_to_copy);

                parent_inode->file_size += sizeof(DirEntry);
                return;
            }
        }
    }
    throw std::runtime_error("Directory Full.");
}

void FileSystem::create_fs_entry(std::string path, FS_FILE_TYPES type) {
    auto tokenized_path = tokenize_path(path, '/');
    if (tokenized_path.empty()) throw std::runtime_error("Path cannot be empty.");

    std::string filename = tokenized_path.back();
    size_t parent_id = traverse_path_till_parent(tokenized_path);
    Inode* parent_inode = get_global_inode_ptr(parent_id);

    if (find_inode_in_dir(parent_inode, filename) != 0) {
        throw std::runtime_error("Error: '" + filename + "' already exists.");
    }

    int new_id = -1;
    for (int i = 0; i < block_group_managers.size(); i++) {
        new_id = block_group_managers[i].allocate_inode();
        if (new_id != -1) break;
    }
    if (new_id == -1) throw std::runtime_error("Disk Full.");

    Inode* new_inode = get_global_inode_ptr(new_id);
    new_inode->id = new_id;
    new_inode->file_size = 0;
    new_inode->file_type = type;
    
    // Permissions
    new_inode->uid = current_uid; 
    new_inode->gid = current_gid;
    // Directories usually get 755, files get 644
    new_inode->permissions = (type == FS_DIRECTORY) ? 0755 : 0644;

    std::memset(new_inode->direct_blocks, 0, sizeof(new_inode->direct_blocks));

    add_entry_to_dir(parent_inode, new_id, filename);
    std::cout << (type == FS_DIRECTORY ? "Directory" : "File") << " '" << filename << "' created.\n";
}

// ---------------- PUBLIC API ----------------

void FileSystem::create_file(std::string path) {
    create_fs_entry(path, FS_FILE_TYPES::FS_FILE);
}

void FileSystem::create_dir(std::string path) {
    create_fs_entry(path, FS_FILE_TYPES::FS_DIRECTORY);
}

void FileSystem::write_file(std::string path, const std::vector<uint8_t>& data) {
    auto tokenized_path = tokenize_path(path, '/');
    std::string filename = tokenized_path.back();
    size_t parent_id = traverse_path_till_parent(tokenized_path);
    Inode* parent_inode = get_global_inode_ptr(parent_id);

    size_t file_id = find_inode_in_dir(parent_inode, filename);
    if (file_id == 0) throw std::runtime_error("File not found: " + path);

    Inode* file_inode = get_global_inode_ptr(file_id);
    if (file_inode->file_type != FS_FILE) throw std::runtime_error("Not a file.");

    // Logic to allocate blocks for data
    size_t block_size = disk.get_block_size();
    size_t required_blocks = (data.size() + block_size - 1) / block_size;
    if (required_blocks > 12) throw std::runtime_error("File too large (>48KB).");

    if (!check_permission(file_inode, 2)) { // 2 = Write
        throw std::runtime_error("Permission denied: No write access to this file.");
    }

    // Free extra blocks if shrinking
    for (size_t i = required_blocks; i < 12; ++i) {
        if (file_inode->direct_blocks[i] != 0) {
             int gid = file_inode->direct_blocks[i] / sb->blocks_per_group;
             block_group_managers[gid].free_block(file_inode->direct_blocks[i]);
             file_inode->direct_blocks[i] = 0;
        }
    }

    // Write Data
    for (size_t i = 0; i < required_blocks; ++i) {
        if (file_inode->direct_blocks[i] == 0) {
            int gid = file_inode->id / sb->inodes_per_group;
            int bid = block_group_managers[gid].allocate_block();
            if (bid == -1) throw std::runtime_error("Disk Full.");
            file_inode->direct_blocks[i] = bid;
        }

        size_t offset = i * block_size;
        size_t bytes = std::min(block_size, data.size() - offset);

        // GEMINI FIX: Use get_ptr directly to avoid temp buffer
        std::memcpy(disk.get_ptr(file_inode->direct_blocks[i]), data.data() + offset, bytes);
    }

    file_inode->file_size = data.size();
    std::cout << "Written " << data.size() << " bytes to " << filename << "\n";
}

void FileSystem::read_direct_block_to_buffer(Inode* file, uint8_t* buffer) {
    size_t block_size = disk.get_block_size();
    for (int i = 0; i < 12; i++) {
        if (file->direct_blocks[i] == 0) break;
        std::memcpy(buffer + (i * block_size), disk.get_ptr(file->direct_blocks[i]), block_size);
    }
}

std::vector<uint8_t> FileSystem::read_file(std::string path) {
    auto tokenized_path = tokenize_path(path, '/');
    std::string filename = tokenized_path.back();
    size_t parent_id = traverse_path_till_parent(tokenized_path);
    size_t file_id = find_inode_in_dir(get_global_inode_ptr(parent_id), filename);

    if (file_id == 0) throw std::runtime_error("File not found.");
    Inode* file_inode = get_global_inode_ptr(file_id);

    if (!check_permission(file_inode, 4)) { // 4 = Read
        throw std::runtime_error("Permission denied: No read access to this file.");
    }

    size_t max_size = 12 * disk.get_block_size();
    std::vector<uint8_t> buffer(max_size);
    read_direct_block_to_buffer(file_inode, buffer.data());

    // GEMINI FIX: Resize to actual data size
    buffer.resize(file_inode->file_size);
    return buffer;
}

void FileSystem::delete_file(std::string path) {
    auto tokenized_path = tokenize_path(path, '/');
    std::string filename = tokenized_path.back();
    size_t parent_id = traverse_path_till_parent(tokenized_path);
    Inode* parent_inode = get_global_inode_ptr(parent_id);

    // GATEKEEPER: Check if user can modify the parent directory
    if (!check_permission(parent_inode, 2)) { 
        throw std::runtime_error("Permission denied: Cannot modify parent directory.");
    }

    // Reuse find logic but we need the entry pointer to zero it out
    int max_entries = 4096 / sizeof(DirEntry);
    for (int i = 0; i < 12; i++) {
        if (parent_inode->direct_blocks[i] == 0) break;
        DirEntry* entry = reinterpret_cast<DirEntry*>(disk.get_ptr(parent_inode->direct_blocks[i]));

        for (int j = 0; j < max_entries; j++) {
            if (entry[j].inode_id != 0 && std::strncmp(entry[j].name, filename.c_str(), 255) == 0) {
                release_file_resources(entry[j].inode_id);
                std::memset(&entry[j], 0, sizeof(DirEntry));
                parent_inode->file_size -= sizeof(DirEntry);
                std::cout << "Deleted " << filename << "\n";
                return;
            }
        }
    }
    throw std::runtime_error("File not found.");
}

void FileSystem::release_file_resources(size_t inode_id) {
    Inode* node = get_global_inode_ptr(inode_id);
    for (int i=0; i<12; i++) {
        if (node->direct_blocks[i] != 0) {
            block_group_managers[node->direct_blocks[i] / sb->blocks_per_group].free_block(node->direct_blocks[i]);
            node->direct_blocks[i] = 0;
        }
    }
    block_group_managers[inode_id / sb->inodes_per_group].free_inode(inode_id);
}

void FileSystem::recursive_resource_release(size_t dir_inode_id) {
    Inode* dir = get_global_inode_ptr(dir_inode_id);
    int max = 4096 / sizeof(DirEntry);
    for (int i=0; i<12; i++) {
        if (dir->direct_blocks[i] == 0) break;
        DirEntry* entry = reinterpret_cast<DirEntry*>(disk.get_ptr(dir->direct_blocks[i]));
        for (int j=0; j<max; j++) {
            if (entry[j].inode_id != 0) {
                Inode* child = get_global_inode_ptr(entry[j].inode_id);
                if (child->file_type == FS_DIRECTORY) recursive_resource_release(entry[j].inode_id);
                else release_file_resources(entry[j].inode_id);
            }
        }
        // Free the directory block itself
        block_group_managers[dir->direct_blocks[i] / sb->blocks_per_group].free_block(dir->direct_blocks[i]);
    }
    block_group_managers[dir_inode_id / sb->inodes_per_group].free_inode(dir_inode_id);
}

void FileSystem::delete_dir(std::string path) {
    auto tokenized_path = tokenize_path(path, '/');
    std::string dirname = tokenized_path.back();
    size_t parent_id = traverse_path_till_parent(tokenized_path);
    Inode* parent_inode = get_global_inode_ptr(parent_id);

    // You need write permission (2) on the parent to remove a subdirectory
    if (!check_permission(parent_inode, 2)) {
        throw std::runtime_error("Permission denied: Cannot modify parent directory.");
    }

    int max_entries = 4096 / sizeof(DirEntry);
    for (int i = 0; i < 12; i++) {
        if (parent_inode->direct_blocks[i] == 0) break;
        DirEntry* entry = reinterpret_cast<DirEntry*>(disk.get_ptr(parent_inode->direct_blocks[i]));

        for (int j = 0; j < max_entries; j++) {
            if (entry[j].inode_id != 0 && std::strncmp(entry[j].name, dirname.c_str(), 255) == 0) {
                Inode* target = get_global_inode_ptr(entry[j].inode_id);
                if (target->file_type != FS_DIRECTORY) throw std::runtime_error("Not a directory.");

                recursive_resource_release(entry[j].inode_id);
                std::memset(&entry[j], 0, sizeof(DirEntry));
                parent_inode->file_size -= sizeof(DirEntry);
                std::cout << "Deleted directory " << dirname << "\n";
                return;
            }
        }
    }
    throw std::runtime_error("Directory not found.");
}

std::vector<FileEntry> FileSystem::list_dir(std::string path) {
    auto tokenized_path = tokenize_path(path, '/');
    size_t target_id;

    // CASE 1: Root Directory "/"
    if (tokenized_path.empty()) {
        target_id = sb->home_dir_inode;
    }
    // CASE 2: Any other directory
    else {
        // GEMINI FIX: Do not pop_back().
        // traverse_path_till_parent automatically returns the parent of the LAST token.
        // Input: ["a", "b"] -> Returns Inode("a")
        // Then we find "b" inside "a".
        std::string dirname = tokenized_path.back();
        size_t parent_id = traverse_path_till_parent(tokenized_path);

        Inode* parent_inode = get_global_inode_ptr(parent_id);
        target_id = find_inode_in_dir(parent_inode, dirname);
    }

    if (target_id == 0) throw std::runtime_error("Directory not found.");

    Inode* dir = get_global_inode_ptr(target_id);
    if (dir->file_type != FS_DIRECTORY) throw std::runtime_error("Not a directory.");
    // GATEKEEPER: Must have Read (4) permission to list a directory
    if (!check_permission(dir, 4)) {
        throw std::runtime_error("Permission denied: Cannot read directory.");
    }

    std::vector<FileEntry> results;
    int max = 4096 / sizeof(DirEntry);

    // Scan all blocks of the directory
    for (int i = 0; i < 12; i++) {
        if (dir->direct_blocks[i] == 0) break;

        DirEntry* entry = reinterpret_cast<DirEntry*>(disk.get_ptr(dir->direct_blocks[i]));
        for (int j = 0; j < max; j++) {
            if (entry[j].inode_id != 0) {
              Inode* item_inode = get_global_inode_ptr(entry[j].inode_id);
              results.push_back({
                  std::string(entry[j].name),
                  item_inode->uid,
                  item_inode->gid,
                  item_inode->permissions,
                  item_inode->file_type == FS_DIRECTORY
              });
            }
        }
    }
    return results;
}

void FileSystem::login(uint16_t uid, uint16_t gid) {
    this->current_uid = uid;
    this->current_gid = gid;
    std::cout << "Logged in as User: " << uid << " Group: " << gid << "\n";
}

void FileSystem::logout() {
    this->current_uid = 0; // Revert to root or a "guest" state
    this->current_gid = 0;
    std::cout << "Logged out. Current user is now Root.\n";
}

bool FileSystem::check_permission(Inode* node, uint16_t access_type) {
    // 1. Root (UID 0) can do EVERYTHING
    if (current_uid == 0) return true;

    uint16_t p = node->permissions;

    // 2. Check if Current User is the Owner
    if (node->uid == current_uid) {
        // Shift bits to check Owner permissions (the first 3 bits of the 9-bit rwxrwxrwx)
        uint16_t owner_perms = (p >> 6) & 0x7; 
        return (owner_perms & access_type);
    }

    // 3. Check if User is in the Group
    if (node->gid == current_gid) {
        uint16_t group_perms = (p >> 3) & 0x7;
        return (group_perms & access_type);
    }

    // 4. Otherwise, check "Other" permissions
    uint16_t other_perms = p & 0x7;
    return (other_perms & access_type);
}
