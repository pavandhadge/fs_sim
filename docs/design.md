
### 1. `system_design.md`

# System Design: In-Memory File System (fs_sim)

## 1. Architectural Philosophy

The system follows a strict **Layered Architecture**. The goal is to separate the "Physical Media" (Memory) from the "Logical Driver" (File System Logic) to avoid the Split-Brain problem.

**Core Principle:** *The `std::vector<uint8_t>` in the `Disk` class is the **Single Source of Truth**. All other classes are merely temporary "viewers" or "manipulators" of that vector.*

### Conceptual Diagram

```mermaid
[ User / REPL ]
      | calls
      v
[ FileSystem (Driver) ]  <--- Controls logic (Format, Create, Read, Delete)
      | creates
      v
[ BlockGroupManager ]    <--- Transient "Lens" to manage allocations in specific groups
      | manipulates
      v
[ Disk (Hardware) ]      <--- Owns std::vector<uint8_t> (The Raw Bytes)

```

## 2. Component Design

### 2.1 Layer 1: The Hardware (`Disk`)

**Responsibility:** Simulates the physical hard drive. It enforces the "Block" abstraction. It does **not** know what a file is.

* **State:**
* `std::vector<uint8_t> memory`: The linear byte array (Simulated Disk).
* `const int BLOCK_SIZE = 4096`: The atomic unit of transfer.
* `size_t BLOCK_COUNT`: Calculated based on disk capacity.


* **Interface:**
* `read_block(int block_id, void* buffer)`: Safer copy OUT of disk.
* `write_block(int block_id, const void* buffer)`: Safer copy INTO disk.
* `uint8_t* get_ptr(int block_id)`: **(Internal Use Only)** Returns a raw pointer to the start of a block for zero-copy struct casting.
* `hex_dump(int block_id)`: Debugging tool.



### 2.2 Layer 2: On-Disk Structures (The "Stencils")

These are Plain Old Data (POD) structs found in `fs/disk_datastructures.hpp`. They define how we interpret the raw bytes.

* **`struct SuperBlock` (Block 0)**
* Contains global metadata: Magic Number (`0xF5513001`), Total Blocks, Geometry (Inodes/Blocks per Group), and the Root Directory Inode ID.


* **`struct Inode`**
* The "Object" of the file system. Contains ID, Type (File/Dir), Size, Permissions, and the `direct_blocks[12]` array.


* **`struct DirEntry`**
* The naming mechanism. Maps a filename string to an Inode ID. Stored inside data blocks of directory inodes.



### 2.3 Layer 3: The Driver (`FileSystem`)

**Responsibility:** The "God Object" that coordinates operations.

* **State:**
* `Disk& disk`: Reference to the hardware.
* `SuperBlock* sb`: Pointer cast directly to Block 0.
* `std::vector<BlockGroupManager>`: Managers for each allocation zone.


* **Interface:**
* **Lifecycle:** `format()`, `mount()`.
* **File Ops:** `create_file()`, `write_file()`, `read_file()`, `delete_file()`.
* **Dir Ops:** `create_dir()`, `list_dir()`, `delete_dir()` (recursive).



### 2.4 Layer 4: The Manager (`BlockGroupManager`)

**Responsibility:** Handles the complex bitwise math for allocating Inodes and Blocks within a specific Block Group.

* **Logic:**
* Locates Bitmaps relative to the Group Start.
* `allocate_inode()`: Scans Inode Bitmap for a 0, flips to 1.
* `allocate_block()`: Scans Block Bitmap for a 0, flips to 1.
* `free_inode/block()`: Flips 1 to 0.
