
**Rule of Thumb:** Do not move to the next phase until the current phase is bug-free. In file systems, bugs in Phase 1 look like "ghosts" in Phase 4 and are impossible to debug.

---

# Project Roadmap: In-Memory File System (Ext2-Lite)

## Phase 0: The Hardware Layer (Foundation)

**Goal:** Abstract the raw C++ memory so the rest of the system thinks it is talking to a physical disk.

* **Step 0.1: The `Disk` Class**
* Implement the class with `std::vector<uint8_t> memory`.
* Implement `read_block(int block_id, void* buffer)` and `write_block`.
* **Constraint:** This class must **never** know what an Inode or File is. It only knows bytes and blocks.


* **Step 0.2: The Debugger (Hex Dump)**
* Implement `dump_block(int block_id)`.
* It should print the hex and ASCII representation of a block. You will use this every single day.


* **Step 0.3: Layout Definition**
* Define your constants: `BLOCK_SIZE = 4096`, `DISK_SIZE = 16MB`.
* Calculate how many blocks you have total.



**Exit Criteria:** You can write "Hello World" to Block 10, read it back, and see it in your Hex Dump.

---

## Phase 1: The Administrator (Superblock & Bitmaps)

**Goal:** Manage the "State" of the file system. Know what is free and what is used.

* **Step 1.1: The Superblock**
* Define `struct SuperBlock`.
* Implement `format()`: This function wipes the disk, sets up the Superblock (magic number, counts), and clears the bitmaps.


* **Step 1.2: The Bitmaps**
* Reserve Block 1 for "Inode Bitmap" and Block 2 for "Data Block Bitmap".
* Implement `allocate_block()`: Find a `0` bit, flip it to `1`, return the ID.
* Implement `free_block(int id)`: Flip `1` to `0`.
* *Repeat the same for Inodes.*



**Exit Criteria:** You can call `allocate_block()` 5 times and get IDs 0, 1, 2, 3, 4. If you call `free_block(2)` and allocate again, you get 2.

---

## Phase 2: The Object Store (Inodes)

**Goal:** Create the abstract concept of a "File" (without names yet).

* **Step 2.1: The Inode Struct**
* Define `struct Inode`. Must contain: `file_size`, `type` (file/dir), and `block_pointers[]`.
* *Decision:* How many direct pointers? (Start with 10 direct pointers. Ignore indirect pointers for now).


* **Step 2.2: Inode Table Access**
* Implement `read_inode(int inode_id, Inode* out_inode)`.
* Implement `write_inode(int inode_id, Inode* in_inode)`.
* *Math:* You need to calculate which block the Inode lives in. Formula: `(inode_id * sizeof(Inode)) / BLOCK_SIZE`.


* **Step 2.3: File Data Read/Write**
* Implement `inode_write_data(int inode_id, int offset, char* data)`.
* Logic:
1. Calculate which block index corresponds to `offset`.
2. Look up the physical block ID in `inode.block_pointers`.
3. If it's 0 (unallocated), call `allocate_block()` and save it to the Inode.
4. Write the data to that block.





**Exit Criteria:** You can create an Inode, write 5KB of data to it (spanning 2 blocks), and read it back correctly.

---

## Phase 3: The Naming System (Directories)

**Goal:** Give human-readable names to Inodes.

* **Step 3.1: Directory Entry**
* Define `struct DirEntry { uint16_t inode_id; char name[255]; }`.


* **Step 3.2: The Root Directory**
* Update `format()` to strictly create Inode 0 as the "Root Directory".


* **Step 3.3: Directory Logic**
* "Writing to a directory" means appending a `DirEntry` struct to the directory's data block.
* "Reading a directory" means iterating over `DirEntry` structs in that block.


* **Step 3.4: Path Parsing**
* Implement `resolve_path(string path) -> int inode_id`.
* Logic: Split string by `/`. Start at Root Inode. Search for "home". Get ID. Open that Inode. Search for "user". Etc.



**Exit Criteria:** You can create a file named "test.txt", and when you list the root directory, you see "test.txt" pointing to Inode 1.

---

## Phase 4: The User Interface (CLI)

**Goal:** Interact with the system.

* Implement the loop:
```cpp
while(true) {
    cin >> command;
    if (command == "mkdir") ...
    if (command == "write") ...
    if (command == "cat") ...
}

```



---

### Critical "Senior" Advice for this Roadmap:

1. **Unit Test as you go:** Do not write the CLI (Phase 4) until Phase 3 works. Write a `test_phase_1.cpp` that just pounds the allocator to make sure it doesn't break.
2. **Persistence Check:** Because you are using `std::vector` and `reinterpret_cast`, you can actually save your file system to a real file on your laptop's hard drive (`fwrite` the vector to `disk.img`) and load it back later. This is a great way to debug "corruption" issues.
3. **Simplification:** For Phase 3, don't worry about deleting files from directories yet. Removing an entry from the middle of a list is annoying. Just mark the `inode_id` in the `DirEntry` as 0 to "soft delete" it.

Would you like a more detailed breakdown of **Phase 1 (The Bitmaps)**, as that is usually the first major hurdle in logic?
