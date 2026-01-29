### 2. `hld_lld.md`


# High-Level & Low-Level Design (HLD/LLD)

## 1. High-Level Design (HLD)

The system is designed as a **Monolithic Library** running in user space. It mimics a Kernel Driver architecture but executes within a single C++ process.

### 1.1 Data Flow Scenario: `write_file("/doc.txt", data)`

1.  **Request:** CLI sends path and data to `FileSystem` class.
2.  **Resolution:** `FileSystem` reads the Root Directory to find the Inode for `doc.txt`.
3.  **Allocation:** `FileSystem` realizes `doc.txt` needs more space. It requests a free block from the `BlockGroupManager`.
4.  **Hardware Write:** `FileSystem` calculates the offset of the new block and instructs `Disk` to `write(offset, data)`.

### 1.2 Memory Layout (The 16MB Map)
The raw memory buffer is divided into **Block Groups**.

```text
[ Block 0        ] -> SuperBlock (Global Settings)
[ Block 1        ] -> Inode Bitmap (Tracks used inodes)
[ Block 2        ] -> Block Bitmap (Tracks used data blocks)
[ Block 3 - 10   ] -> Inode Table (Storage for File Metadata)
[ Block 11 - N   ] -> Data Region (Actual file content & Directory lists)

```

## 2. Low-Level Design (LLD)

### 2.1 Core Data Structures

**Constraint:** All structures must be packed (`#pragma pack(1)`) to match disk layout exactly.

#### `SuperBlock`

```cpp
struct SuperBlock {
    uint32_t magic_number;      // 0xF5513001
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t inodes_per_group;
    uint32_t blocks_per_group;
    uint32_t home_dir_inode;    // Root Directory ID
    // ... padding
};

```

#### `Inode`

```cpp
struct Inode {
    uint32_t id;
    uint16_t file_type;         // 1=File, 2=Dir
    uint16_t permissions;       // e.g. 0755
    uint32_t file_size;         // Size in bytes
    uint32_t direct_blocks[12]; // Pointers to data blocks
};

```

#### `DirEntry`

```cpp
struct DirEntry {
    uint32_t inode_id;          // Points to the Inode
    uint8_t name_len;
    char name[255];             // Fixed width filename
};

```

### 2.2 Key Algorithms

#### Algorithm: `create_file("/home/notes.txt")`

1. **Path Parse:** Split string into `["home", "notes.txt"]`.
2. **Traversal:**
* Start at Root Inode. Read data block. Find "home". Get ID X.
* Go to Inode X.


3. **Pre-Check:** Scan Inode X's data to ensure "notes.txt" doesn't already exist.
4. **Resource Allocation:**
* Call `BlockGroupManager` to find a free Inode ID (say, Z).
* Mark Inode Z as used in the Inode Bitmap.


5. **Inode Init:**
* Get pointer to Inode Z in the table.
* Set `type = FS_FILE`, `size = 0`.


6. **Linkage:**
* Create a `DirEntry` object: `{ inode: Z, name: "notes.txt" }`.
* Append this object to the data block of Inode X (the parent directory).



#### Algorithm: `write_file(inode_id, data)`

1. **Capacity Check:** `required_blocks = ceil(data.size / 4096)`.
2. **Expansion Loop:**
* If `inode.direct_blocks[i]` is `0` (empty), call `BlockGroupManager` to allocate a **Data Block**.
* Update `inode.direct_blocks[i]` with the new Block ID.


3. **Data Write:**
* Loop through the data chunks.
* `memcpy` data into the physical blocks pointed to by the Inode.


4. **Metadata Update:**
* Update `inode.file_size`.
