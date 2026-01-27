
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


  public:
  ~FileSystem() {
      delete this->sb;
  }
  FileSystem(Disk& disk);
  void format();
  void mount();
// size_t traverse_path_till_parent(std::vector<std::string>&tokenized_path);
  //for noe read only prints the buffer in future we will return a unit_8 pointer
  void add_entry_to_dir(Inode* parent_inode, size_t newfile_id, std::string filename);

  void create_file(std::string path);
  void delete_file(std::string path);
  void read_file(std::string path);

  void create_dir(std::string path);
  void delete_dir(std::string path);
  void read_dir(std::string path);
};
