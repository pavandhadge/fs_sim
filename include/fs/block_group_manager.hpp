#pragma once
#include "fs/disk.hpp"
#include "fs/disk_datastructures.hpp"

class BlockGroupManager {
private:
    Disk& disk;
    SuperBlock* sb;
    int group_id;

    // Relative Offsets (Valid for ANY group)
    const int INODE_BITMAP_OFFSET = 1;
    const int BLOCK_BITMAP_OFFSET = 2;
    const int INODE_TABLE_OFFSET  = 3;

    // Helpers
    uint8_t* get_inode_bitmap_ptr();
    uint8_t* get_block_bitmap_ptr();
    uint8_t* get_inode_table_start();

    // Bitwise Ops
    bool get_bit(uint8_t* bitmap, int local_index);
    void set_bit(uint8_t* bitmap, int local_index);
    void clear_bit(uint8_t* bitmap, int local_index);
    int find_first_free_bit(uint8_t* bitmap, int max_bits);

public:
int get_block_id_for_inode(int inode_id) ;
   BlockGroupManager(Disk& d, SuperBlock* sb, int id)
        : disk(d), sb(sb), group_id(id) {}

    int allocate_inode();
    void free_inode(int global_inode_id);

    int allocate_block();
    void free_block(int global_block_id);

    Inode* get_inode(int global_inode_id);
    bool is_inode_allocated(int global_inode_id);
};
