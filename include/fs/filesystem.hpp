
#include "fs/block_group_manager.hpp"
#include "fs/disk.hpp"
#include "fs/disk_datastructures.hpp"
#include <cstddef>
#include <cstdint>
#include <string>


class FileSystem{
    private :
    // Disk& disk;
    SuperBlock *sb;
    unsigned int total_groups;
    std::vector<BlockGroupManager>block_group_managers;

  public:
  FileSystem(Disk& disk);
  void format();
  void mount();

  //for noe read only prints the buffer in future we will return a unit_8 pointer

  void create_file(std::string path);
  void delete_file(std::string path);
  void read_file(std::string path);

  void create_dir(std::string path);
  void delete_dir(std::string path);
  void read_dir(std::string path);
};
