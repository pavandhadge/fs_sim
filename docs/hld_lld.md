# High-Level & Low-Level Design (HLD/LLD)

## 1. High-Level Design (HLD)

The system is designed as a **Monolithic Library** running in user space. It mimics 
a Kernel Driver architecture but executes within a single C++ process.

### 1.1 Design Philosophy

**Layered Architecture:** The system follows a strict layered architecture. The goal 
is to separate the "Physical Media" (Memory) from the "Logical Driver" (File System 
Logic) to avoid the Split-Brain problem.

**Core Principle:** *The memory-mapped file in the `Disk` class is the **Single Source 
of Truth**. All other classes are merely temporary "viewers" or "manipulators" of 
that memory.*

### 1.2 Data Flow Scenario: `write_file("/doc.txt", data)`

1. **Request:** CLI sends path and data to `FileSystem` class.
2. **Resolution:** `FileSystem` reads the Root Directory to find the Inode for `doc.txt`.
3. **Allocation:** `FileSystem` realizes `doc.txt` needs more space. It requests a 
   free block from the `BlockGroupManager`.
4. **Hardware Write:** `FileSystem` calculates the offset of the new block and instructs 
   `Disk` to write data to memory-mapped area.

### 1.3 Memory Layout (The 16MB Map)

The raw memory buffer is divided into **Block Groups**:

```
[ Block 0        ] -> SuperBlock (Global Settings)
[ Block 1        ] -> Inode Bitmap (Tracks used inodes)
[ Block 2        ] -> Block Bitmap (Tracks used data blocks)
[ Block 3 - 10   ] -> Inode Table (Storage for File Metadata)
[ Block 11 - N   ] -> Data Region (Actual file content & Directory lists)
```

---

## 2. Low-Level Design (LLD)

### 2.1 Layer 1: The Hardware (`Disk`)

**Responsibility:** Simulates the physical hard drive. It enforces the "Block" 
abstraction. It does **not** know what a file is.

**State:**
- `uint8_t* mapped_data`: Pointer to memory-mapped file
- `int fd`: File descriptor
- `const int BLOCK_SIZE = 4096`: The atomic unit of transfer
- `size_t BLOCK_COUNT`: Calculated based on disk capacity

**Interface:**
- `read_block(int block_id, void* buffer)`: Copy OUT of disk
- `write_block(int block_id, const void* buffer)`: Copy INTO disk
- `uint8_t* get_ptr(int block_id)`: Returns raw pointer to block start
- `hex_dump(int block_id)`: Debugging tool

**Key Implementation:**
- Uses `mmap()` for memory-mapped I/O
- Uses `ftruncate()` to pre-allocate disk file
- Uses `msync()` to flush data on shutdown

---

### 2.2 Layer 2: On-Disk Structures (The "Stencils")

All structures must be packed (`#pragma pack(1)`) to match disk layout exactly.

#### SuperBlock

Located at Block 0. Contains global filesystem metadata.

```cpp
struct SuperBlock {
    size_t magic_number;        // 0xF5513001
    size_t total_inodes;
    size_t total_blocks;
    size_t inodes_per_group;
    size_t blocks_per_group;
    size_t home_dir_inode;     // Root Directory ID
};
```

#### Inode

The fundamental metadata structure. Each file or directory is represented by an inode.

```cpp
struct Inode {
    size_t id;                      // Inode ID
    FS_FILE_TYPES file_type;         // File, Directory, Symlink
    size_t file_size;               // Size in bytes
    uint16_t uid;                   // Owner User ID
    uint16_t gid;                   // Owner Group ID
    uint16_t permissions;            // e.g., 0755
    size_t direct_blocks[12];       // Direct block pointers
    size_t single_indirect;          // Single indirect
    size_t double_indirect;         // Double indirect
    size_t triple_indirect;         // Triple indirect
};
```

#### DirEntry

Directory entries map filenames to inode IDs. Stored inside directory data blocks.

```cpp
struct DirEntry {
    size_t inode_id;     // Points to the Inode
    uint8_t name_len;    // Length of filename
    char name[255];     // Fixed width filename
};
```

---

### 2.3 Layer 3: The Driver (`FileSystem`)

**Responsibility:** The "God Object" that coordinates all filesystem operations.

**State:**
- `Disk& disk`: Reference to the hardware
- `SuperBlock* sb`: Pointer cast directly to Block 0
- `std::vector<BlockGroupManager>`: Managers for each allocation zone
- `uint16_t current_uid/gid`: Current user for permission checks

**Interface:**
- **Lifecycle:** `format()`, `mount()`
- **File Ops:** `create_file()`, `write_file()`, `read_file()`, `delete_file()`
- **Dir Ops:** `create_dir()`, `list_dir()`, `delete_dir()`
- **Meta Ops:** `chmod()`, `chown()`, `chgrp()`, `get_stats()`

---

### 2.4 Layer 4: The Manager (`BlockGroupManager`)

**Responsibility:** Handles the complex bitwise math for allocating Inodes and Blocks 
within a specific Block Group.

**Logic:**
- Locates Bitmaps relative to the Group Start
- `allocate_inode()`: Scans Inode Bitmap for a 0, flips to 1
- `allocate_block()`: Scans Block Bitmap for a 0, flips to 1
- `free_inode/block()`: Flips 1 to 0

**Constants (offsets within each group):**
```cpp
const int INODE_BITMAP_OFFSET = 1;
const int BLOCK_BITMAP_OFFSET = 2;
const int INODE_TABLE_OFFSET  = 3;
```

---

## 3. Key Algorithms

### Algorithm: `create_file("/home/notes.txt")`

1. **Path Parse:** Split string into `["home", "notes.txt"]`
2. **Traversal:** Start at Root Inode → Find "home" → Get Inode ID X
3. **Pre-Check:** Scan directory's data to ensure "notes.txt" doesn't exist
4. **Resource Allocation:** Call BlockGroupManager to find free Inode ID
5. **Inode Init:** Set type = FS_FILE, size = 0, permissions
6. **Linkage:** Create DirEntry `{inode: Z, name: "notes.txt"}` in parent directory

### Algorithm: `write_file(inode_id, data)`

1. **Capacity Check:** `required_blocks = ceil(data.size / 4096)`
2. **Expansion Loop:**
   - If `inode.direct_blocks[i]` is empty, allocate a Data Block
   - Update `inode.direct_blocks[i]` with new Block ID
3. **Data Write:** Loop through data, memcpy into physical blocks
4. **Metadata Update:** Update `inode.file_size`

### Algorithm: Path Resolution

1. Tokenize path: "/a/b/c" → ["a", "b", "c"]
2. Start at root inode
3. For each component:
   - Read directory's data blocks
   - Find entry matching component name
   - Get the inode ID
   - Move to that inode
4. Handle ".." (move up in path stack)
5. Handle "." (stay in current directory)
6. Handle symlinks (resolve target path recursively)

### Algorithm: Permission Check

1. If UID == 0 (root), allow all operations
2. If UID matches file's UID, check owner permissions
3. If GID matches file's GID, check group permissions
4. Otherwise, check other permissions

---

## 4. Block Group Layout

Each block group has a fixed layout:

```
┌─────────────────────────────────────────┐
│ Block 0 (Global): SuperBlock            │
├─────────────────────────────────────────┤
│ GROUP 0:                                │
│   Block 1: Inode Bitmap                 │
│   Block 2: Block Bitmap                 │
│   Block 3-10: Inode Table               │
│   Block 11+: Data Blocks                 │
├─────────────────────────────────────────┤
│ GROUP 1:                                │
│   Block N+1: Inode Bitmap               │
│   Block N+2: Block Bitmap               │
│   ...                                   │
└─────────────────────────────────────────┘
```

---

## 5. Indirect Block Addressing

For files larger than 48KB (12 direct blocks), fs_sim uses indirect addressing:

**Single Indirect:**
- Inode has `single_indirect` pointer to an indirect block
- Indirect block contains 1024 block pointers
- Total: 12 + 1024 = 1036 blocks = ~4MB

**Double Indirect:**
- `double_indirect` points to indirect blocks of indirect blocks
- 1024 × 1024 = ~1 million blocks = ~4GB

**Triple Indirect:**
- `triple_indirect` for three levels
- ~4TB maximum file size
