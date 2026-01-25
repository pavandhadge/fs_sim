
You should write a function called `check_integrity()` (like a mini-`fsck`) that runs these checks during debugging. Here are the specific invariants for your project:

### 1. The Allocation Invariant (The most critical one)

Every block must be in exactly **one** of two states:

1. **Marked Free:** The bit in the Bitmap is `0`.
2. **Owned:** The bit in the Bitmap is `1` **AND** an Inode points to it.

**Violations to watch for:**

* **Leak:** Bitmap says `1` (Used), but no Inode points to it. (Space is lost forever).
* **Double Allocation:** Bitmap says `1`, but *two different* Inodes point to it. (Writing to File A corrupts File B).
* **Phantom Data:** Bitmap says `0` (Free), but an Inode points to it. (The next time you create a new file, it will overwrite this existing file's data).

### 2. The Size/Block Invariant

The file size stored in the Inode must match the number of blocks allocated.

* **Example:** If Block Size is 4KB and File Size is 5KB, the Inode **must** point to exactly 2 blocks.
* **Violation:** If File Size is 5KB but you only have 1 block allocated, reading the last 1KB will read unallocated memory (garbage or crash).

### 3. The Directory Tree Invariant

The directory structure must remain a **Tree** (or strictly a DAG if you allowed hard links, but stick to Tree for now).

* **Root Exists:** Inode 0 must always be a Directory.
* **No Cycles:** A directory cannot be a subdirectory of itself (e.g., `/A/B/A` is illegal).
* **Unique Names:** A directory cannot contain two entries with the same filename.

### 4. The Reference Invariant (Self-Consistency)

* **Valid Inode IDs:** A directory entry cannot point to Inode #500 if your system only supports 100 Inodes.
* **Valid Types:** If a directory entry says "My name is `Docs/`", the Inode it points to **must** have `type = DIRECTORY`. It cannot point to a file Inode.

### How to use this?

When you write your `write_file` or `delete_file` functions, ask yourself: *"Does this code operation preserve these 4 rules?"*

**Example:**
When you delete a file, you must:

1. Clear the bit in the Bitmap (**Invariant 1**).
2. Remove the Inode pointer (**Invariant 1**).
3. Remove the name from the Directory (**Invariant 3**).

If you forget step 1, you have a **Leak**.
If you forget step 3, you have a **Dangling Pointer** (a filename pointing to a deleted inode).
