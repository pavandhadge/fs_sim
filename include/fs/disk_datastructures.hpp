#pragma once
#pragma pack(1)
#include <cstddef>
#include <cstdint>

enum FS_FILE_TYPES  {
    FS_FREE = 0,
    FS_FILE = 1,
    FS_DIRECTORY = 2
};

struct SuperBlock{
    size_t magic_number; // (4 bytes)
    size_t total_inodes; // (4 bytes)
    size_t total_blocks; // (4 bytes)
    const size_t inodes_per_group = 4096;
    const size_t  blocks_per_group = 4096; // (4 bytes) -- >fized at 4096 blocks i.e in total 16mb
};

struct Inode {
  size_t id; //4 bytes
  FS_FILE_TYPES file_type; // 1 byte --> ultimately 4 bytes
  size_t file_size; //4 bytes
  size_t direct_blocks[12]; // 64 - 12 = 52 i.e 12 , max size of each file can be 12*4096 = 48kb
};


struct DirEntry{
    size_t inode_id;
    std::uint8_t name_len;
    char name[255];
};
