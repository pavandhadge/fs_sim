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
    // 1. Calculate Limits
    int inodes_per_group = sb->inodes_per_group;
    int start_id = group_id * inodes_per_group;
    int end_id   = start_id + inodes_per_group;

    // 2. FIXED: Bounds check logic was flipped
    if (inode_id < start_id || inode_id >= end_id) {
        throw std::out_of_range("BlockGroupManager: Inode ID not in this group");
    }

    // 3. FIXED: Use inodes_per_group for modulo
    int local_index = inode_id % inodes_per_group;

    // 4. Pointer Arithmetic
    // Since Disk memory is a vector, we can safely add offset across block boundaries
    uint8_t* table_start = get_inode_table_start();
    size_t byte_offset = local_index * sizeof(Inode);

    return reinterpret_cast<Inode*>(table_start + byte_offset);
}

int BlockGroupManager::allocate_inode() {
    uint8_t* bitmap = get_inode_bitmap_ptr();

    // Find free bit (0 to inodes_per_group-1)
    int local_index = find_first_free_bit(bitmap, sb->inodes_per_group);

    if (local_index == -1) return -1; // Group full

    // Mark used
    set_bit(bitmap, local_index);

    // CRITICAL: Zero out the new inode memory
    // We calculate the Global ID to get the pointer
    int global_id = (group_id * sb->inodes_per_group) + local_index;
    Inode* node = get_inode(global_id);
    std::memset(node, 0, sizeof(Inode));

    // Initialize standard fields
    node->id = global_id;

    return global_id;
}

void BlockGroupManager::free_inode(int global_inode_id) {
    // FIXED: Convert to local index
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
    // FIXED: Use the helper, not the hardcoded constant
    uint8_t* bitmap = get_block_bitmap_ptr();

    int local_index = find_first_free_bit(bitmap, sb->blocks_per_group);

    if (local_index == -1) return -1;

    // FIXED: Reserve Block 0 of the group (if it's Group 0, it's Superblock)
    // If local_index is 0, we should skip it to maintain symmetry/safety
    if (local_index == 0) {
        set_bit(bitmap, 0); // Mark it used so we don't find it again
        return allocate_block(); // Try again
    }

    set_bit(bitmap, local_index);

    // Zero out the actual data block
    int global_block_id = (group_id * sb->blocks_per_group) + local_index;
    std::memset(disk.get_ptr(global_block_id), 0, disk.get_block_size());

    return global_block_id;
}

void BlockGroupManager::free_block(int global_block_id) {
    int local_index = global_block_id % sb->blocks_per_group;
    clear_bit(get_block_bitmap_ptr(), local_index);
}

// ==========================================
// BITWISE HELPERS (Keep as is, they looked good)
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

int BlockGroupManager::find_first_free_bit(uint8_t* bitmap, int max_bits) {
    int max_bytes = max_bits / 8;
    for (int i = 0; i < max_bytes; i++) {
        if (bitmap[i] != 0xFF) {
            for (int bit = 0; bit < 8; bit++) {
                if (!((bitmap[i] >> bit) & 1)) {
                    int found = (i * 8) + bit;
                    if (found < max_bits) return found;
                }
            }
        }
    }
    return -1;
}
