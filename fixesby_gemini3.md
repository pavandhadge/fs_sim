### Project Report: File System Implementation

**Developer:** Pavan Santosh Dhadge
**Reviewer:** Gemini (AI Mentor)
**Status:** Functional Basic File System (Ext2-style)

---

### 1. Executive Summary of Your Efforts

You have successfully implemented a **Unix-like File System** from scratch. This is a non-trivial engineering task that many Computer Science graduates never attempt.

**What you got right (The Hard Stuff):**

* **Architecture:** Your mental model of `SuperBlock`  `Group Descriptors`  `Bitmaps`  `Inodes`  `Data Blocks` is essentially correct and mirrors the **Ext2** file system used by Linux for decades.
* **Path Traversal:** You correctly conceptualized how to turn a string path (`/home/pavan`) into a series of Inode lookups.
* **Data Structures:** Your structs (`Inode`, `DirEntry`) were memory-aligned and correctly designed for serialization.
* **Bit Manipulation:** Your logic for finding free bits in the allocation maps was sound.

**Where you needed support (The "Gotchas"):**

* **C++ Memory Safety:** Returning pointers to stack memory (which causes crashes) and confusion between `sizeof(pointer)` vs `sizeof(buffer)`.
* **Resource Lifecycle:** You implemented *Creation* well, but missed the complexity of *Destruction* (cleaning up blocks recursively to prevent disk leaks).
* **Persistence:** The distinction between "Formatting" (wiping) and "Mounting" (loading state) was initially blurred.

---

### 2. Technical Fix Report (What I patched)

Here is a breakdown of the specific interventions I made to your codebase.

#### A. Critical Stability Fixes (Crash Prevention)

1. **The Stack Pointer Trap:**
* *Issue:* In `read_file`, you were creating `uint8_t buffer[4096]` on the stack and returning it. This memory is destroyed when the function ends, leading to garbage data.
* *Fix:* Switched to `std::vector<uint8_t>`, which uses Heap memory and manages its own lifetime safely.


2. **The `sizeof` Bug:**
* *Issue:* `sizeof(curr_block)` was returning `8` (pointer size), not `4096`. This meant your directory loops were only checking the first entry.
* *Fix:* Hardcoded `4096` (or `BLOCK_SIZE`) for buffer iterations.


3. **Bounds Checking:**
* *Issue:* `BlockGroupManager` calculations for `local_index` were slightly off, risking writing to the wrong block group.
* *Fix:* Added strict bounds checking (`inode_id < start_id || inode_id >= end_id`) to prevent cross-group corruption.



#### B. Logic & Architectural Fixes

4. **Format vs. Mount:**
* *Issue:* Your `mount` function was overwriting variables, effectively re-formatting the disk every time you ran the program.
* *Fix:* Separated them. `format()` now writes the SuperBlock. `mount()` **only reads** it.


5. **Directory Growth:**
* *Issue:* `add_entry_to_dir` failed if the first block was full.
* *Fix:* Added logic to detect a full block (`curr_block_id == 0`) and dynamically allocate a new data block for the directory.


6. **Recursive Deletion (`rm -rf`):**
* *Issue:* `delete_dir` only removed the directory name but left all the files inside it "orphaned" on the disk (Memory Leak).
* *Fix:* Implemented `recursive_resource_release` to dive into subdirectories and free every single block before deleting the parent.



#### C. Edge Case Handling

7. **The Root Inode Paradox:**
* *Issue:* You created the root inode, but never saved its ID (`home_dir_inode`) into the SuperBlock during format.
* *Fix:* Updated `format()` to write the SuperBlock *after* allocating Inode 1.


8. **Inode 0 Reservation:**
* *Issue:* The system treated Inode 0 as valid, but 0 usually means "NULL" or "Error".
* *Fix:* Modified `find_first_free_bit` to start searching at index 1 for Group 0.



### 3. Conclusion

You did the heavy lifting of designing the **engine**. I simply acted as the mechanic who tightened the bolts, fixed the leaks, and added the safety rails.

**This is now a working File System.** You can format it, mount it, create complex directory trees, write data, read it back, and delete it all safely. Great work.
