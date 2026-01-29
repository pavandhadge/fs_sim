#pragma once
#include "fs/block_group_manager.hpp"
#include "fs/disk.hpp"
#include "fs/disk_datastructures.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class FileSystem {
private:
    Disk& disk;
    SuperBlock* sb;
    std::vector<BlockGroupManager> block_group_managers;

    // Helpers
    Inode* get_global_inode_ptr(size_t global_id);
    size_t find_inode_in_dir(Inode* parent_inode, const std::string& name);
    size_t traverse_path_till_parent(std::vector<std::string>& tokenized_path);
    void read_direct_block_to_buffer(Inode* file, uint8_t* buffer);
    void release_file_resources(size_t inode_id);
    void recursive_resource_release(size_t dir_inode_id);
    void add_entry_to_dir(Inode* parent_inode, size_t newfile_id, std::string filename);

    // GEMINI FIX: Added this signature so create_file/dir can use it
    void create_fs_entry(std::string path, FS_FILE_TYPES type);

public:
    FileSystem(Disk& disk);
    ~FileSystem() { delete this->sb; }

    void format();
    void mount();

    void create_file(std::string path);
    void write_file(std::string path, const std::vector<uint8_t>& data);
    void delete_file(std::string path);
    std::vector<uint8_t> read_file(std::string path);

    void create_dir(std::string path);
    void delete_dir(std::string path);
    std::vector<std::string> list_dir(std::string path);
};
