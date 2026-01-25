
# System Design Document: In-Memory File System (IMFS)

## 1. Architectural Philosophy

The system follows a strict **Layered Architecture**. The goal is to separate the "Physical Media" (Memory) from the "Logical Driver" (File System Logic) to avoid the Split-Brain problem.

**Core Principle:** *The `std::vector<uint8_t>` in the Disk class is the **Single Source of Truth**. All other classes are merely temporary "viewers" or "manipulators" of that vector.*

### High-Level Diagram

```mermaid
[ User / CLI ]
      | calls
      v
[ FileSystem (Driver) ]  <--- Controls logic (Format, Create, Read)
      | owns
      v
[ Disk (Hardware) ]      <--- Owns std::vector<uint8_t> (The Raw Bytes)
      ^
      | views
[ BlockGroupManager ]    <--- Temporary "Lens" to manipulate specific Block Groups

```

---

## 2. Component Design

### 2.1 Layer 1: The Hardware (`Disk`)

**Responsibility:** Simulates the physical hard drive. It enforces the "Block" abstraction. It does **not** know what a file is.

* **State (Members):**
* `std::vector<uint8_t> memory`: The 16MB linear byte array.
* `const int BLOCK_SIZE = 4096`: The atomic unit of transfer.
* `const int BLOCK_COUNT`: Total blocks (4096 blocks for 16MB).


* **Interface (Methods):**
* `read_block(int block_id, void* buffer)`: Copies data OUT of disk.
* `write_block(int block_id, const void* buffer)`: Copies data INTO disk.
* `uint8_t* get_ptr(int block_id)`: **(Internal Use Only)** Returns a raw pointer to the start of a block for direct casting.
* `hex_dump(int block_id)`: Debugging tool to print block contents.



### 2.2 Layer 2: On-Disk Structures (The "Stencils")

These are Plain Old Data (POD) structs. They define how we interpret the raw bytes. **Constraint:** Use `#pragma pack(1)` to prevent compiler padding.

* **`struct SuperBlock` (Fits in Block 0)**
* `magic_number` (4 bytes): e.g., `0xF5F5F5F5`.
* `total_inodes` (4 bytes).
* `total_blocks` (4 bytes).
* `block_group_size` (4 bytes): How many blocks in one group?


* **`struct Inode` (e.g., 64 bytes)**
* `id` (4 bytes).
* `file_type` (1 byte): 0=Free, 1=File, 2=Directory.
* `file_size` (4 bytes): Actual bytes used.
* `direct_blocks[12]` (48 bytes): Array of Block IDs where data lives.


* **`struct DirEntry` (Variable Length - simplified for v1)**
* `inode_id` (4 bytes).
* `name_len` (1 byte).
* `name[255]` (255 bytes): Fixed width for simplicity in v1.



### 2.3 Layer 3: The Driver (`FileSystem`)

**Responsibility:** The "God Object" that coordinates operations.

* **State:**
* `Disk disk`: The instance of the hardware.
* `SuperBlock* sb`: A pointer cast directly to `disk.memory[0]`.


* **Interface:**
* `format()`: Wipes disk, writes SuperBlock, zeroes bitmaps.
* `mount()`: Reads Block 0, verifies magic number, sets up `sb` pointer.
* `create_file(path)`:
1. Resolves path to parent directory.
2. Finds free Inode (via `BlockGroupManager`).
3. Creates `DirEntry` in parent.


* `write_file(inode_id, data)`:
1. Calculates needed blocks.
2. Allocates blocks (via `BlockGroupManager`).
3. Writes data.





### 2.4 Layer 4: The Manager (`BlockGroupManager`)

**Responsibility:** A transient helper class to manage allocations within a specific region (Block Group). It is created, does a job, and is destroyed.

* **Usage:**
```cpp
// Need a free block?
BlockGroupManager bg(disk, 0); // Manage Group 0
int block_id = bg.find_free_block();

```


* **Logic:**
* It knows *where* the bitmaps are located for its group.
* It performs bitwise operations to find/flip bits.
* It updates the bitmaps directly in the `Disk` memory.



---

## 3. Data Flow Example: `create_file("/a.txt")`

1. **Path Resolution:** `FileSystem` looks at Root Inode (fixed ID 0). Reads data block. Finds entry "/".
2. **Allocation (Inode):**
* `FileSystem` creates `BlockGroupManager(disk, 0)`.
* Calls `bg.allocate_inode()`.
* `bg` scans Inode Bitmap in Group 0. Finds bit #5 is 0.
* `bg` sets bit #5 to 1. Returns ID 5.


3. **Initialization:**
* `FileSystem` calculates offset for Inode 5.
* Casts pointer: `Inode* node = (Inode*)disk.get_ptr(...)`.
* Sets `node->file_type = FILE`.


4. **Directory Update:**
* `FileSystem` reads Root Directory data block.
* Appends `DirEntry { id: 5, name: "a.txt" }`.
* Updates Root Inode size.



---

## 4. The Golden Rules (Veterans' Code of Conduct)

Follow these rules strictly to ensure your design succeeds.

### Rule 1: The "No-Copy" Rule

**Never** copy data from the disk into a class member variable to "store" it.

* *Bad:* `class Inode { int size; ... };` (If you change `size` here, the disk doesn't know).
* *Good:* `struct Inode { ... };` ... `Inode* ptr = (Inode*)disk.get_ptr(...)`. Modifying `ptr->size` writes directly to the disk memory.

### Rule 2: Integer Arithmetic Only

Do not store `Inode*` or `char*` inside your structs.

* If Inode 5 points to Block 10, store the integer `10`.
* Pointers are only valid for *milliseconds* while a function is running. They are not valid "data".

### Rule 3: Bitmaps are Sacred

Never write data to a block without marking it as "Used" in the bitmap first.

* If you write data but forget the bitmap, the next file creation will overwrite your data.
* **Always** update the bitmap **before** writing the data (Allocation first).

### Rule 4: Zero on Allocation

When you allocate a block (get it from the bitmap), you **must** zero it out immediately (`memset(ptr, 0, BLOCK_SIZE)`).

* *Why?* If you delete a file, the old data stays there. If you re-allocate that block to a new file, the new file might contain "garbage" (old sensitive data) until you overwrite it.

### Rule 5: Debug Early

Before writing `create_file`, you must write `dump_memory()`. If you cannot see the bytes, you are coding blind.

---

### Suggested File Structure

* `main.cpp` (CLI loop)
* `Disk.h/cpp` (The hardware)
* `FileSystem.h/cpp` (The driver)
* `Structures.h` (The structs: SuperBlock, Inode, etc.)
* `Utils.h` (Bit manipulation helpers, Hex dump)
