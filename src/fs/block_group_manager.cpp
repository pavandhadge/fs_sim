#include "fs/block_group_manager.hpp"
#include <stdexcept>
#include <cstring> // for memset

// ==========================================
// POINTER HELPERS
// ==========================================
uint8_t* BlockGroupManager::get_inode_bitmap_ptr() {
    int block_id = (group_id * sb->blocks_per_group) + INODE_BITMAP_OFFSET;
    return disk.get_ptr(block_id);
}

uint8_t* BlockGroupManager::get_block_bitmap_ptr() {
    int block_id = (group_id * sb->blocks_per_group) + BLOCK_BITMAP_OFFSET;
    return disk.get_ptr(block_id);
}

uint8_t* BlockGroupManager::get_inode_table_start() {
    int block_id = (group_id * sb->blocks_per_group) + INODE_TABLE_OFFSET;
    return disk.get_ptr(block_id);
}

// ==========================================
// INODE LOGIC
// ==========================================
Inode* BlockGroupManager::get_inode(int inode_id) {
    int inodes_per_group = sb->inodes_per_group;
    int start_id = group_id * inodes_per_group;
    int end_id   = start_id + inodes_per_group;

    if (inode_id < start_id || inode_id >= end_id) {
        throw std::out_of_range("BlockGroupManager: Inode ID not in this group");
    }

    int local_index = inode_id % inodes_per_group;
    uint8_t* table_start = get_inode_table_start();
    size_t byte_offset = local_index * sizeof(Inode);

    return reinterpret_cast<Inode*>(table_start + byte_offset);
}

int BlockGroupManager::allocate_inode() {
    uint8_t* bitmap = get_inode_bitmap_ptr();

    // GEMINI FIX: Inode 0 (Global) is usually reserved.
    int start_bit = (group_id == 0) ? 1 : 0;

    int local_index = find_first_free_bit(bitmap, sb->inodes_per_group, start_bit);
    if (local_index == -1) return -1;

    set_bit(bitmap, local_index);

    int global_id = (group_id * sb->inodes_per_group) + local_index;
    Inode* node = get_inode(global_id);
    std::memset(node, 0, sizeof(Inode));
    node->id = global_id;

    return global_id;
}

void BlockGroupManager::free_inode(int global_inode_id) {
    int local_index = global_inode_id % sb->inodes_per_group;
    clear_bit(get_inode_bitmap_ptr(), local_index);
}

bool BlockGroupManager::is_inode_allocated(int global_inode_id) {
    int local_index = global_inode_id % sb->inodes_per_group;
    return get_bit(get_inode_bitmap_ptr(), local_index);
}

// ==========================================
// BLOCK LOGIC
// ==========================================
int BlockGroupManager::allocate_block() {
    uint8_t* bitmap = get_block_bitmap_ptr();

    // GEMINI FIX: In Group 0, we must skip the Metadata blocks (SB + Bitmaps + Table)
    int start_bit = 0;
    if (group_id == 0) {
        // Calculate how many blocks the Inode Table takes
        int table_size_blocks = (sb->inodes_per_group * sizeof(Inode)) / disk.get_block_size();
        // Offset = 1(SB) + 1(IBMap) + 1(BBMap) + TableSize
        start_bit = INODE_TABLE_OFFSET + table_size_blocks;
    }

    int local_index = find_first_free_bit(bitmap, sb->blocks_per_group, start_bit);
    if (local_index == -1) return -1;

    set_bit(bitmap, local_index);
    int global_block_id = (group_id * sb->blocks_per_group) + local_index;

    // Zero out the new block
    std::memset(disk.get_ptr(global_block_id), 0, disk.get_block_size());

    return global_block_id;
}

void BlockGroupManager::free_block(int global_block_id) {
    int local_index = global_block_id % sb->blocks_per_group;
    clear_bit(get_block_bitmap_ptr(), local_index);
}

// ==========================================
// BITWISE HELPERS
// ==========================================
bool BlockGroupManager::get_bit(uint8_t* bitmap, int index) {
    return (bitmap[index / 8] & (1 << (index % 8))) != 0;
}

void BlockGroupManager::set_bit(uint8_t* bitmap, int index) {
    bitmap[index / 8] |= (1 << (index % 8));
}

void BlockGroupManager::clear_bit(uint8_t* bitmap, int index) {
    bitmap[index / 8] &= ~(1 << (index % 8));
}

int BlockGroupManager::find_first_free_bit(uint8_t* bitmap, int max_bits, int start_bit) {
    for (int i = start_bit; i < max_bits; i++) {
        if (!get_bit(bitmap, i)) {
            return i;
        }
    }
    return -1;
}

int BlockGroupManager::get_block_id_for_inode(int inode_id) {
    int group_id = inode_id / sb->inodes_per_group;
    int local_index = inode_id % sb->inodes_per_group;
    int inodes_per_block = 4096 / sizeof(Inode);
    int block_offset = local_index / inodes_per_block;
    int group_start = group_id * sb->blocks_per_group;
    return group_start + INODE_TABLE_OFFSET + block_offset;
}
