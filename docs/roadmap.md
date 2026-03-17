# Project Roadmap: fs_sim (Ext2-Lite File System)

---

## Phase 0: The Hardware Layer (COMPLETED)
**Goal:** Abstract the raw memory so the rest of the system thinks it is talking 
to a physical disk.

- **Disk Class:** Implemented memory-mapped storage with mmap
- **Persistence:** Disk data backed by real file on disk
- **Debug:** Implemented hex_dump for debugging
- **Structs:** Defined packed SuperBlock, Inode, and DirEntry

---

## Phase 1: The Administrator (COMPLETED)
**Goal:** Manage the "State" of the file system. Know what is free and what is used.

- **Bitmaps:** Implemented bitwise operations (find_first_free_bit, set_bit, clear_bit)
- **Groups:** Implemented BlockGroupManager to handle isolation of allocation zones
- **Allocation:** Can allocate/free Inodes and Blocks reliably

---

## Phase 2: The Object Store (COMPLETED)
**Goal:** Create the abstract concept of a "File" (without names yet).

- **Format:** format() correctly wipes disk, sets geometry, writes SuperBlock
- **Mount:** mount() reads SuperBlock and initializes managers
- **Read/Write:** write_file supports dynamic growth, read_file retrieves exact data

---

## Phase 3: The Naming System (COMPLETED)
**Goal:** Give human-readable names to Inodes.

- **Directories:** Implemented as files containing DirEntry lists
- **Traversals:** Path resolution turns strings (/a/b) into Inode IDs
- **Recursion:** delete_dir implements "rm -rf" logic

---

## Phase 4: The Interface (COMPLETED)
**Goal:** Interact with the system.

- **REPL:** main.cpp implements shell loop with ls, mkdir, touch, write, read, rm
- **Testing:** Comprehensive test suite with persistence and stress tests

---

## Phase 5: Permissions (COMPLETED)
**Goal:** Add security with UNIX-style permissions.

- **UID/GID:** Added owner and group tracking to Inodes
- **Permission Bits:** Implemented rwxrwxrwx model
- **Commands:** chmod, chown, chgrp commands
- **Checks:** Permission validation on all file operations

---

## Phase 6: Symlinks (COMPLETED)
**Goal:** Support symbolic links (shortcuts).

- **Symlink Creation:** ln -s command
- **Path Resolution:** Automatic resolution of symlink targets
- **Relative/Absolute:** Support both relative and absolute paths

---

## Phase 7: Indirect Addressing (COMPLETED)
**Goal:** Support files larger than 48KB.

- **Single Indirect:** Files up to ~4MB
- **Double Indirect:** Files up to ~4GB  
- **Triple Indirect:** Files up to ~4TB
- **Implementation:** Added single_indirect, double_indirect, triple_indirect to Inode

---

## Future Scope

1. **Journaling:** Implement a Write-Ahead Log (WAL) to ensure consistency during crashes
2. **Hard Links:** Multiple names pointing to same inode
3. **Extended Attributes:** File metadata beyond standard fields
4. **Journaling:** Transaction logging for crash recovery
5. **Cache Optimization:** In-memory caching layer for frequently accessed blocks
6. **ACLs:** More granular access control lists beyond UNIX permissions

---

## Completed Features Summary

- [x] Memory-mapped disk I/O
- [x] SuperBlock with magic number
- [x] Block groups with bitmaps
- [x] Inode allocation/deallocation
- [x] Block allocation/deallocation
- [x] File create/read/write/delete
- [x] Directory create/list/delete (recursive)
- [x] Path resolution (/a/b/c)
- [x] Permission system (rwx, uid, gid)
- [x] Symbolic links
- [x] Indirect block pointers (single/double/triple)
- [x] Persistent storage (disk images)
- [x] Interactive REPL
- [x] Comprehensive test suite

---

## Test Results

- disk_test: PASSED
- fs_operations_test: PASSED
- fs_directory_test: PASSED
- fs_permissions_test: PASSED
- fs_persistence_test: PASSED
- fs_stress_test: MOSTLY PASSED (1 edge case)
- LegacyFsTest: PASSED
- LegacyPermissionTest: PASSED

---

## Performance Metrics

- 100 create/write/delete cycles: ~0.8 seconds
- 150 files mass creation: ~40ms
- 500 files across 5 directories: ~2.5 seconds
- Single file create: <1ms
- Disk full (1 group): 178 files
