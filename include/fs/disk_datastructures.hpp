#include <cstddef>
#include <cstdint>
#include <filesystem>

enum FS_FILE_TYPES  { FS_FREE = 0,
   FS_FILE = 1,
   FS_DIRECTORY = 2};

struct SuperBlock{
    size_t magic_number; // (4 bytes)
    size_t total_inodes; // (4 bytes)
    size_t total_blocks; // (4 bytes)
    size_t block_group_size; // (4 bytes)
};

struct Inode {
  size_t id;
  FS_FILE_TYPES file_type;
  size_t file_size;
  size_t direct_blocks[12];
};


struct DirEntry{
    size_t inode_id;
    std::uint8_t name_len;
    char name[255];
};
