
#include "fs/block_group_manager.hpp"
#include "fs/disk.hpp"
#include "fs/disk_datastructures.hpp"
#include <cstddef>
#include <cstdint>
#include <string>


class FileSystem{
    private :
    Disk& disk;
    SuperBlock *sb;
    unsigned int total_groups;
    std::vector<BlockGroupManager>block_group_managers;

    // [Private Helper]
    // Routes a Global Inode ID to the correct BlockGroupManager based on ID ranges.
    // Usage: Used inside traverse_path to support multiple block groups.
    Inode* get_global_inode_ptr(size_t global_id);

    // [Private Helper]
    // Scans the data blocks of a directory Inode to find a specific filename.
    // Returns: The Inode ID if found, or 0 if not found.
    size_t find_inode_in_dir(Inode* parent_inode, const std::string& name);

    // [Public/Private]
    // Navigates the path and returns the Inode ID of the PARENT directory.
    // Example: For path "/home/docs/file.txt", it returns the Inode ID of "docs".
    size_t traverse_path_till_parent(std::vector<std::string>& tokenized_path);


    void read_direct_block_to_buffer(Inode* file,uint8_t* buffer);
    void release_file_resources(size_t inode_id);
    void recursive_resource_release(size_t dir_inode_id);
  void add_entry_to_dir(Inode* parent_inode, size_t newfile_id, std::string filename);
  void create_fs_entry(std::string path, FS_FILE_TYPES type) ;
  public:
  ~FileSystem() {
      delete this->sb;
  }
  FileSystem(Disk& disk);
  void format();
  void mount();
// size_t traverse_path_till_parent(std::vector<std::string>&tokenized_path);
  //for noe read only prints the buffer in future we will return a unit_8 pointer

  void create_file(std::string path);
  void delete_file(std::string path);
  std::vector<uint8_t> read_file(std::string path);

  void create_dir(std::string path);
  void delete_dir(std::string path);
  // std::vector<std::string> read_dir(std::string path);
  std::vector<std::string> list_dir(std::string path) ;
};
