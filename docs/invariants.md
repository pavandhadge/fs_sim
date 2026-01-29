---

### 3. `invariants.md`

# System Invariants & Integrity Checks

To ensure stability, the file system must maintain the following invariants at all times. Violating these leads to corruption, data loss, or "phantom" data.

## 1. The Allocation Invariant (Critical)
Every block must be in exactly **one** of two states:
1.  **Marked Free:** The bit in the Bitmap is `0`.
2.  **Owned:** The bit in the Bitmap is `1` **AND** an Inode points to it.

**Violations:**
* **Leak:** Bitmap says `1` (Used), but no Inode points to it. (Space is lost forever).
* **Double Allocation:** Bitmap says `1`, but *two different* Inodes point to it. (Writing to File A corrupts File B).
* **Phantom Data:** Bitmap says `0` (Free), but an Inode points to it. (The next time you create a new file, it will overwrite this existing file's data).

## 2. The Size/Block Invariant
The file size stored in the Inode must match the number of blocks allocated.

* **Example:** If Block Size is 4KB and File Size is 5KB, the Inode **must** point to exactly 2 blocks.
* **Violation:** If File Size is 5KB but you only have 1 block allocated, reading the last 1KB will read unallocated memory (garbage or crash).

## 3. The Directory Tree Invariant
The directory structure must remain a **Tree**.

* **Root Exists:** Inode designated as Root must always be a Directory.
* **No Cycles:** A directory cannot be a subdirectory of itself.
* **Unique Names:** A directory cannot contain two entries with the same filename.

## 4. The Golden Rules (Implementation Guide)

1.  **The "No-Copy" Rule:** Do not store file data in class members. Use `disk.get_ptr()` to modify data directly on the simulated disk.
2.  **Integer Arithmetic Only:** Store Block IDs (integers), never memory pointers, inside structs. Pointers are invalid after a restart; integers persist.
3.  **Bitmaps are Sacred:** Never write to a block without marking it as "Used" in the bitmap first.
4.  **Zero on Allocation:** When allocating a block, `memset` it to 0 immediately to prevent data leakage from previous files.
5.  **Recursive Cleanup:** When deleting a directory, you must recursively delete its children first (`recursive_resource_release`), or you will create "orphan" blocks that leak disk space.
