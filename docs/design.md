# fs_sim: A UNIX-like File System Simulation

## Overview

fs_sim is a complete UNIX-like filesystem simulator written in C++17 that mimics the 
architecture and behavior of real filesystem implementations, specifically inspired by 
ext2 (Second Extended File System) used in early Linux systems.

In simple terms: fs_sim is a "fake hard drive" that runs inside a C++ program. You can 
create files, folders, set permissions, write data, and all that data gets saved to a 
real file on your computer. When you restart the program, your files are still there - 
just like a real filesystem.

---

## Core Concepts

### 1. Blocks

A hard drive is just a giant array of bytes. Reading 1 byte at a time is slow. So 
filesystems group bytes into "blocks" - typically 4096 bytes (4KB) at a time.

In fs_sim:
- All reads/writes happen in 4KB chunks
- Block 0, Block 1, Block 2, ... Block N
- Block size = 4096 bytes

Example: If you have a 16MB "disk", it has 4096 blocks (16MB / 4KB = 4096)

### 2. SuperBlock

The SuperBlock is like the "header" of the entire filesystem. It's stored in Block 0 
and contains:
- Magic number (to verify it's a valid filesystem)
- Total number of blocks
- Total number of inodes
- How many blocks/inodes per group
- Where the root directory is

If the SuperBlock gets corrupted, the entire filesystem becomes unreadable.

### 3. Inodes

In UNIX filesystems, files don't have names stored inside them. Instead, files have an 
"inode" (index node) - a data structure that stores:
- File's size
- File's type (regular file, directory, symlink)
- Permissions (who can read/write/execute)
- Owner (UID, GID)
- Timestamps (created, modified, accessed)
- Pointers to where the actual data is stored on disk

In fs_sim, each file/directory has an Inode. The Inode ID is unique across the 
entire filesystem.

### 4. Direct Block Pointers

Each Inode has space for 12 direct block pointers:

    Inode for "myfile.txt"
    ├── direct_blocks[0] → Block 500
    ├── direct_blocks[1] → Block 501
    ├── direct_blocks[2] → Block 502
    ├── ...
    └── direct_blocks[11] → Block 511

This means a file can be up to 12 × 4KB = 48KB using only direct pointers.

For larger files, fs_sim also supports:
- Single Indirect: A block that contains pointers to more data blocks
- Double Indirect: A block that points to blocks that point to data blocks  
- Triple Indirect: Three levels of indirection

### 5. Directories

In UNIX, a directory is just a SPECIAL FILE that contains a list of entries. Each 
entry maps a name to an inode number:

    Directory "/home"
    ├── Entry: "file1.txt" → Inode #15
    ├── Entry: "file2.txt" → Inode #23  
    └── Entry: "subdir" → Inode #45

### 6. Bitmaps

How does the filesystem know which blocks are free? Using bitmaps!

A bitmap is an array of bits where:
- 0 = free
- 1 = allocated

This is extremely memory-efficient: 1 bit per block vs 1 byte per block.

### 7. Block Groups

Real filesystems divide the disk into "Block Groups" to reduce fragmentation and 
keep related data together.

Each Block Group contains:
- SuperBlock (copy)
- Inode Bitmap
- Block Bitmap
- Inode Table
- Data Blocks

In fs_sim:
- Small disks (< 4096 blocks): 1 block group
- Larger disks: Multiple block groups

### 8. Permissions

fs_sim implements UNIX-style permissions with 9 bits:

    Owner | Group | Others
    rwx   | rwx   | rwx
    421   | 421   | 421

Example: rwxr-xr-x = 0755
- Owner: 7 (rwx)
- Group: 5 (r-x)
- Others: 5 (r-x)

### 9. Symlinks

A symbolic link (symlink) is like a shortcut - it's a file that points to another file.

fs_sim supports symlinks with path resolution.

### 10. Persistence

The "disk" isn't just RAM - it's backed by a real file on your computer using mmap.

---

## Architecture Layers

fs_sim follows a LAYERED ARCHITECTURE:

```
┌──────────────────────────────────────────────────────┐
│                    USER LAYER                         │
│         (main.cpp - REPL Command Interface)          │
│   Commands: ls, touch, mkdir, rm, write, read...    │
└──────────────────────┬───────────────────────────────┘
                       │
┌──────────────────────▼───────────────────────────────┐
│               FILESYSTEM LAYER                         │
│              (filesystem.cpp - Core Logic)             │
│   - create_file(), write_file(), read_file()         │
│   - Path traversal, permission checking                │
└──────────────────────┬───────────────────────────────┘
                       │
┌──────────────────────▼───────────────────────────────┐
│            BLOCK GROUP LAYER                          │
│        (block_group_manager.cpp - Allocation)         │
│   - allocate_inode(), allocate_block()               │
│   - Bitmap manipulation                               │
└──────────────────────┬───────────────────────────────┘
                       │
┌──────────────────────▼───────────────────────────────┐
│                 DISK LAYER                            │
│                 (disk.cpp - Hardware)                 │
│   - Memory-mapped file I/O (mmap)                    │
│   - read_block(), write_block(), get_ptr()           │
└──────────────────────────────────────────────────────┘
```

---

## On-Disk Layout

For a 16MB Disk (4096 blocks):

```
BLOCK 0: SuperBlock
  - Magic number, block count, inode count
  - Block/inode group info
  - Root directory inode ID

GROUP 0:
  Block 1: Inode Bitmap (tracks which inodes are used)
  Block 2: Block Bitmap (tracks which data blocks are used)
  Blocks 3-10: Inode Table (stores Inode structs)
  Blocks 11+: Data Blocks (file content, directory entries)
```

---

## Features

- SuperBlock with magic number validation
- Block group-based disk organization
- Inode allocation with bitmap tracking
- Block allocation with bitmap tracking
- Direct block pointers (12 per inode)
- Single, double, and triple indirect blocks
- Unix-style permissions (rwxrwxrwx)
- Owner/group model (uid, gid)
- Symbolic links with path resolution
- Directory hierarchy with . and .. entries
- Persistent storage via disk images
- Automatic mount/format detection

---

## Building and Running

### Build
```bash
mkdir build && cd build
cmake ..
make
```

### Run
```bash
./fs_sim
```

### Test
```bash
make check
```

---

## Data Flow Examples

### Creating a File
1. REPL parses command, calls fs.create_file("/myfile.txt")
2. FileSystem tokenizes path → ["myfile.txt"]
3. Traverses to root directory (inode #1)
4. Checks if "myfile.txt" already exists
5. Calls BlockGroupManager to allocate a free Inode
6. Creates DirEntry: {name: "myfile.txt", inode_id: 10}
7. Adds entry to root directory's data block
8. Success!

### Writing to a File
1. REPL parses, calls fs.write_file("/myfile.txt", data)
2. Finds "myfile.txt" inode (#10)
3. Checks permissions
4. Calculates required blocks
5. Calls BlockGroupManager.allocate_block()
6. Copies data to block via memcpy
7. Updates Inode with block pointers and file_size

### Reading a File
1. REPL calls fs.read_file("/myfile.txt")
2. Finds inode #10
3. Checks permissions
4. Gets block IDs from direct_blocks
5. Reads blocks into buffer
6. Returns data to user

### Deleting a File
1. REPL calls fs.delete_file("/myfile.txt")
2. Finds entry in parent directory
3. Gets inode ID (#10)
4. Frees all blocks (clears Block Bitmap)
5. Frees inode (clears Inode Bitmap)
6. Removes DirEntry from parent directory
