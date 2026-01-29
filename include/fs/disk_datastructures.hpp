#pragma once
#include <cstddef> // for size_t
#include <cstring> // for memset, memcpy, strlen
#include <cstdint> // for uint8_t
#include <algorithm> // GEMINI FIX: Added for std::min

enum FS_FILE_TYPES {
    FS_FREE = 0,
    FS_FILE = 1,
    FS_DIRECTORY = 2
};

#pragma pack(push, 1)

struct SuperBlock {
    size_t magic_number;
    size_t total_inodes;
    size_t total_blocks;
    size_t inodes_per_group;
    size_t blocks_per_group;
    size_t home_dir_inode;

    // Default Constructor
    SuperBlock() :
        magic_number(0xF5513001),
        total_inodes(0),
        total_blocks(0),
        inodes_per_group(4096),
        blocks_per_group(4096),
        home_dir_inode(0) {} // GEMINI FIX: Initialize home_dir_inode

    // Parameterized Constructor
    SuperBlock(size_t t_inodes, size_t t_blocks) :
        magic_number(0xF5513001),
        total_inodes(t_inodes),
        total_blocks(t_blocks),
        inodes_per_group(4096),
        blocks_per_group(4096),
        home_dir_inode(0) {}
};

struct Inode {
    size_t id;
    FS_FILE_TYPES file_type;
    size_t file_size;
    size_t direct_blocks[12];

    Inode() {
        id = 0;
        file_type = FS_FREE;
        file_size = 0;
        std::memset(direct_blocks, 0, sizeof(direct_blocks));
    }
};

struct DirEntry {
    size_t inode_id;
    uint8_t name_len;
    char name[255];

    DirEntry() : inode_id(0), name_len(0) {
        std::memset(name, 0, 255);
    }

    DirEntry(size_t id, const char* filename) {
        inode_id = id;
        size_t len = std::strlen(filename);
        if (len > 255) len = 255;
        name_len = static_cast<uint8_t>(len);
        std::memset(name, 0, 255);
        std::memcpy(name, filename, len);
    }
};

#pragma pack(pop)
