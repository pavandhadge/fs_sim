### 4. `roadmap.md`

# Project Roadmap: In-Memory File System (Ext2-Lite)
---
**Rule of Thumb:** Do not move to the next phase until the current phase is bug-free. In file systems, bugs in Phase 1 look like "ghosts" in Phase 4 and are impossible to debug.

---


## Phase 0: The Hardware Layer (Completed)
**Goal:** Abstract the raw C++ memory so the rest of the system thinks it is talking to a physical disk.

* **Disk Class:** Implemented `std::vector<uint8_t>` storage with read/write/pointer access.
* **Debug:** Implemented `hex_dump`.
* **Structs:** Defined packed `SuperBlock`, `Inode`, and `DirEntry`.

## Phase 1: The Administrator (Completed)
**Goal:** Manage the "State" of the file system. Know what is free and what is used.

* **Bitmaps:** Implemented bitwise operations (`find_first_free_bit`, `set_bit`).
* **Groups:** Implemented `BlockGroupManager` to handle isolation of allocation zones.
* **Allocation:** Can allocate/free Inodes and Blocks reliably.

## Phase 2: The Object Store (Completed)
**Goal:** Create the abstract concept of a "File" (without names yet).

* **Format:** `format()` correctly wipes disk, sets geometry, and writes SuperBlock *before* mounting to ensure persistence.
* **Mount:** `mount()` reads SuperBlock and initializes managers.
* **Read/Write:** `write_file` supports dynamic growth (allocating new blocks on the fly) and `read_file` retrieves exact byte vectors.

## Phase 3: The Naming System (Completed)
**Goal:** Give human-readable names to Inodes.

* **Directories:** Implemented as files containing `DirEntry` lists.
* **Traversals:** `resolve_path` logic turns strings (`/a/b`) into Inode IDs.
* **Recursion:** `delete_dir` implements "rm -rf" logic to recursively free resources.

## Phase 4: The Interface (Completed)
**Goal:** Interact with the system.

* **REPL:** `main.cpp` implements a shell loop with `ls`, `mkdir`, `touch`, `write`, `read`, `rm`.
* **Testing:** `tests/fs_test.cpp` simulates reboots (Persistence) and heavy load (Stress Testing).

## Future Scope

1.  **Indirect Pointers:** Currently, files are limited to 12 direct blocks (~48KB). Implementing indirect pointers will allow files up to GBs in size.
2.  **Permissions:** Add `uid`, `gid`, and `rwx` checks to Inodes (fields exist in struct, logic needed in `FileSystem`).
3.  **File Backing:** Modify `Disk` to `mmap` a real file (`disk.img`) on the host OS, allowing the file system to persist even after the C++ program exits.
4.  **Journaling:** Implement a Write-Ahead Log (WAL) to ensure consistency during crashes.
