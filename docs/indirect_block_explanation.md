# Indirect Block Addressing in ext2

## The Problem

Currently, your filesystem uses **12 direct block pointers** per inode:
```
Inode structure:
- direct_blocks[0]  -> points to data block 1
- direct_blocks[1]  -> points to data block 2
- ...
- direct_blocks[11] -> points to data block 12
```

Each block is 4096 bytes (4KB).

**Current limit:** 12 × 4KB = **48KB per file**

This is far too small for real filesystems.

---

## The Solution: Indirect Block Pointers

ext2 uses a hierarchy of indirect blocks:

```
Level 1 - Single Indirect:   Block contains 1024 block pointers (4KB / 4 bytes)
Level 2 - Double Indirect:  Block contains pointers to other indirect blocks
Level 3 - Triple Indirect:  Block contains pointers to double-indirect blocks
```

### Modified Inode Structure

Add these fields to your Inode structure:

```cpp
struct Inode {
    // ... existing fields ...
    
    uint32_t single_indirect;    // Block number pointing to level-1 block
    uint32_t double_indirect;    // Block number pointing to level-2 block
    uint32_t triple_indirect;    // Block number pointing to level-3 block
};
```

---

## How It Works

### Single Indirect (Level 1)
```
Inode.single_indirect -> points to an "indirect block"
                              |
                              v
                     [ptr][ptr][ptr]... (1024 pointers)
                              |
                              v
                     Each ptr -> actual data block
```

- Adds 1024 data blocks = **4MB additional**

### Double Indirect (Level 2)
```
Inode.double_indirect -> points to an indirect block
                              |
                              v
                     [ptr][ptr][ptr]... (1024 pointers)
                              |    |    |
                              v    v    v
                     Each points to ANOTHER indirect block
                              |
                              v
                     [ptr][ptr][ptr]... (1024 pointers per level-2 block)
                              |
                              v
                     Each ptr -> actual data block
```

- 1024 × 1024 = **1,048,576 blocks = 4GB**

### Triple Indirect (Level 3)
```
1024 × 1024 × 1024 = **1 billion+ blocks = ~4TB**
```

---

## File Size Limits Summary

| Level | Additional Blocks | Additional Size |
|-------|-------------------|-----------------|
| Direct (12) | 12 | 48 KB |
| Single Indirect | 1024 | 4 MB |
| Double Indirect | 1,048,576 | 4 GB |
| Triple Indirect | 1,073,741,824 | 4 TB |

---

## Algorithm for Reading a Block

Given a block number `N` you want to read/write:

```cpp
uint8_t* FileSystem::get_block_ptr(Inode* inode, uint32_t block_number) {
    const uint32_t DIRECT_LIMIT = 12;
    const uint32_t SINGLE_LIMIT = DIRECT_LIMIT + 1024;           // 12 + 1024 = 1036
    const uint32_t DOUBLE_LIMIT = SINGLE_LIMIT + 1024 * 1024;    // 1036 + 1M = 1048580
    // Triple goes beyond 4GB, optional for now
    
    if (block_number < DIRECT_LIMIT) {
        // Direct block
        return disk.get_ptr(inode->direct_blocks[block_number]);
    }
    else if (block_number < SINGLE_LIMIT) {
        // Single indirect
        uint32_t indirect_block_num = inode->single_indirect;
        uint32_t* indirect_block = reinterpret_cast<uint32_t*>(disk.get_ptr(indirect_block_num));
        uint32_t index = block_number - DIRECT_LIMIT;
        return disk.get_ptr(indirect_block[index]);
    }
    else if (block_number < DOUBLE_LIMIT) {
        // Double indirect
        uint32_t double_block_num = inode->double_indirect;
        uint32_t* double_block = reinterpret_cast<uint32_t*>(disk.get_ptr(double_block_num));
        
        uint32_t remaining = block_number - SINGLE_LIMIT;
        uint32_t level1_index = remaining / 1024;
        uint32_t level2_index = remaining % 1024;
        
        uint32_t single_block_num = double_block[level1_index];
        uint32_t* single_block = reinterpret_cast<uint32_t*>(disk.get_ptr(single_block_num));
        
        return disk.get_ptr(single_block[level2_index]);
    }
    else {
        throw std::runtime_error("File too large (triple indirect not implemented)");
    }
}
```

---

## Algorithm for Allocating Blocks

When writing to a file and you need a new block:

```cpp
uint32_t FileSystem::allocate_block(Inode* inode, uint32_t block_number) {
    // 1. Allocate the actual data block
    uint32_t data_block = allocate_free_block();  // Your existing function
    
    const uint32_t DIRECT_LIMIT = 12;
    const uint32_t SINGLE_LIMIT = DIRECT_LIMIT + 1024;
    
    if (block_number < DIRECT_LIMIT) {
        inode->direct_blocks[block_number] = data_block;
    }
    else if (block_number < SINGLE_LIMIT) {
        // Need single indirect block
        if (inode->single_indirect == 0) {
            inode->single_indirect = allocate_free_block();
            // Zero out the indirect block
            memset(disk.get_ptr(inode->single_indirect), 0, 4096);
        }
        uint32_t* indirect_block = reinterpret_cast<uint32_t*>(disk.get_ptr(inode->single_indirect));
        indirect_block[block_number - DIRECT_LIMIT] = data_block;
    }
    else {
        // Need double indirect (similar logic)
        throw std::runtime_error("Double indirect not implemented yet");
    }
    
    return data_block;
}
```

---

## Key Implementation Steps

1. **Modify Inode structure** in `disk_datastructures.hpp`:
   - Add `single_indirect`, `double_indirect`, `triple_indirect` fields

2. **Initialize to 0** in your inode allocation function

3. **Modify `read_block()`** to handle indirect addressing

4. **Modify `write_block()`** to allocate indirect blocks as needed

5. **Free indirect blocks** when deleting files (must traverse and free all!)

---

## Testing

```bash
# Write a large file > 48KB
write /largefile <48KB+_of_data>

# Should work after implementing single indirect (up to 4MB)
# Then double indirect (up to 4GB)
```

---

