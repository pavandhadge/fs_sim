#include "fs/filesystem.hpp"
#include "fs/block_group_manager.hpp"
#include "fs/disk.hpp"
#include "fs/disk_datastructures.hpp"

FileSystem::FileSystem(Disk& disk){
    // this->disk  = disk_alloted;
    // size_t allocation_size_in_byte = allocated_size * 1024;
    // this->disk = Disk(allocation_size_in_byte);
    this->total_groups = disk.get_block_count() / 4096;
    size_t total_inodes = total_groups * 4096;
    this->sb = new SuperBlock(total_inodes,total_groups);
    block_group_managers.reserve(total_groups);
    for(int i=0;i<total_groups;i++){
        block_group_managers.emplace_back(disk,sb,i);
    }
}
