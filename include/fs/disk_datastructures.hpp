#pragma once
#include <cstddef> // for size_t
#include <cstring> // for memset, memcpy, strlen
#include <cstdint> // for uint8_t

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
    size_t inodes_per_group; // Removed 'const' to allow reading from disk
    size_t blocks_per_group;

    // Default Constructor
    SuperBlock() :
        magic_number(0xF5513001),
        total_inodes(0),
        total_blocks(0),
        inodes_per_group(4096),
        blocks_per_group(4096) {}

    // Parameterized Constructor (Used during Format)
    SuperBlock(size_t t_inodes, size_t t_blocks) :
        magic_number(0xF5513001),
        total_inodes(t_inodes),
        total_blocks(t_blocks),
        inodes_per_group(4096),
        blocks_per_group(4096) {}
};

struct Inode {
    size_t id;
    FS_FILE_TYPES file_type;
    size_t file_size;
    size_t direct_blocks[12];

    // Constructor to zero-out memory
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

    // Default Constructor
    DirEntry() : inode_id(0), name_len(0) {
        std::memset(name, 0, 255);
    }

    // Helper Constructor for creating new entries safely
    DirEntry(size_t id, const char* filename) {
        inode_id = id;

        // Safety: Prevent buffer overflow
        size_t len = std::strlen(filename);
        if (len > 255) len = 255;

        name_len = static_cast<uint8_t>(len);

        // Zero out the buffer first, then copy
        std::memset(name, 0, 255);
        std::memcpy(name, filename, len);
    }
};

#pragma pack(pop)
