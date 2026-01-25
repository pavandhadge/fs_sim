
# High-Level Design (HLD)

## 1. System Architecture

The system is designed as a **Monolithic Library** running in user space. It mimics a Kernel Driver architecture but executes within a single C++ process.

### 1.1 Architectural Layers

The system is composed of three distinct layers of abstraction:

1. **The Interface Layer (CLI/API):** The entry point. It parses user commands (e.g., `mkdir /home`) and converts them into file system calls. It handles input validation but contains no storage logic.
2. **The Logic Layer (FileSystem Driver):** The "Brain." It manages the logic of files, directories, and paths. It knows what an "Inode" is and how to traverse a directory tree. It enforces file system consistency (invariants).
3. **The Physical Layer (Virtual Disk):** The "Body." It manages the raw 16MB memory buffer. It enforces the concept of "Blocks" and "Addresses" but knows nothing about files.

### 1.2 Data Flow

**Scenario:** User executes `write_file("/doc.txt", data)`

1. **Request:** CLI sends path and data to `FileSystem` class.
2. **Resolution:** `FileSystem` reads the Root Directory to find the Inode for `doc.txt`.
3. **Allocation:** `FileSystem` realizes `doc.txt` needs more space. It requests a free block from the `BlockManager`.
4. **Hardware Write:** `FileSystem` calculates the offset of the new block and instructs `Disk` to `write(offset, data)`.

### 1.3 Memory Layout (The 16MB Map)

The raw memory buffer is divided into **Block Groups** (simplified to 1 Global Group for Phase 1 to reduce complexity, then scalable).

**Global Memory Map:**

```text
[ Block 0       ] -> SuperBlock (Global Settings)
[ Block 1       ] -> Inode Bitmap (Tracks used inodes)
[ Block 2       ] -> Block Bitmap (Tracks used data blocks)
[ Block 3 - 10  ] -> Inode Table (Storage for File Metadata)
[ Block 11 - N  ] -> Data Region (Actual file content & Directory lists)

```

---

# Low-Level Design (LLD)

## 2. Core Data Structures (The "Stencils")

These structures define how raw bytes are interpreted.
**Constraint:** All structures must be packed (`#pragma pack(1)`) to match disk layout exactly.

### 2.1 `SuperBlock` (Live in Block 0)

```cpp
struct SuperBlock {
    uint32_t magic_number;      // 0xFSSIM001 (Identity check)
    uint32_t total_blocks;      // 4096 (for 16MB / 4KB)
    uint32_t total_inodes;      // e.g., 1024
    uint32_t block_size;        // 4096
    uint32_t free_blocks_count; // optimization
    uint32_t free_inodes_count; // optimization
    // Padding to ensure structure size is consistent if needed
};

```

### 2.2 `Inode` (The File Metadata)

The "Object" of the file system. Does NOT contain the filename.

```cpp
struct Inode {
    uint32_t id;                // Unique ID (0 to total_inodes-1)
    uint16_t type;              // 0=Free, 1=File, 2=Directory
    uint16_t permissions;       // (Optional for now)
    uint32_t file_size;         // Size in bytes
    uint32_t creation_time;     // Timestamp
    uint32_t direct_blocks[12]; // Pointers to the first 12 data blocks
    // Indirect blocks can be added here in V2
};

```

### 2.3 `DirectoryEntry` (The Naming System)

Stored *inside* the data blocks of a Directory Inode.

```cpp
struct DirectoryEntry {
    uint32_t inode_id;      // Points to the Inode
    uint8_t name_len;       // Length of filename
    char name[60];          // Fixed size filename for simplicity (V1)
    uint8_t file_type;      // Cache of the type (File/Dir)
};

```

---

## 3. Class Design (C++)

### 3.1 Class `Disk` (The Hardware)

**Responsibility:** Encapsulate `std::vector` and provide block-level access.

* **Attributes:**
* `std::vector<uint8_t> memory`
* `const size_t BLOCK_SIZE`


* **Methods:**
* `void read(int block_id, void* buffer)`: Safer copy.
* `void write(int block_id, void* buffer)`: Safer copy.
* `uint8_t* get_raw_ptr(int block_id)`: **Critical.** Returns `&memory[block_id * size]`. Used by the driver to cast structs.



### 3.2 Class `BitmapManager` (The Accountant)

**Responsibility:** Abstract bitwise operations.

* **Methods:**
* `int allocate_bit(uint8_t* bitmap_ptr, int size)`: Scans for a `0`, sets to `1`, returns index.
* `void free_bit(uint8_t* bitmap_ptr, int index)`: Sets to `0`.
* `bool get_bit(uint8_t* bitmap_ptr, int index)`: Status check.



### 3.3 Class `FileSystem` (The Driver)

**Responsibility:** Orchestrate everything.

* **Attributes:**
* `Disk disk`
* `SuperBlock* sb` (Initialized on `mount`)


* **Public API:**
* `void format()`: Wipes disk, writes SuperBlock, creates Root Inode (ID 0).
* `int create_file(string path)`: Full logic (Resolve path -> Alloc Inode -> Link).
* `int write_file(int inode_id, char* buffer, size_t size)`: Handles block spanning.
* `void list_dir(string path)`: formatted print of entries.


* **Private Helpers:**
* `int resolve_path(string path)`: Returns Inode ID for a string path.
* `Inode* get_inode_ptr(int id)`: Helper to cast raw memory to Inode*.



---

## 4. Key Algorithms (Logic Flow)

### 4.1 Algorithm: `create_file("/home/user/notes.txt")`

1. **Path Parse:** Split string into `["home", "user", "notes.txt"]`.
2. **Traversal:**
* Start at Root Inode (ID 0). Read data block. Find "home". Get ID X.
* Go to Inode X. Read data block. Find "user". Get ID Y.
* Go to Inode Y.


3. **Pre-Check:** Scan Inode Y's data to ensure "notes.txt" doesn't already exist.
4. **Resource Allocation:**
* Call `BitmapManager` to find a free Inode ID (say, Z).
* Mark Inode Z as used in the Inode Bitmap.


5. **Inode Init:**
* Get pointer to Inode Z in the table.
* Set `type = FILE`, `size = 0`.


6. **Linkage:**
* Create a `DirectoryEntry` object: `{ inode: Z, name: "notes.txt" }`.
* Append this object to the data block of Inode Y (the parent directory).


7. **Commit:** Updates are live in memory immediately.

### 4.2 Algorithm: `write_file(inode_id, data)`

1. **Capacity Check:** `required_blocks = ceil(data.size / 4096)`.
2. **Expansion Loop:**
* If `inode.direct_blocks[i]` is `0` (empty), call `BitmapManager` to allocate a **Data Block**.
* Update `inode.direct_blocks[i]` with the new Block ID.
* Update `SuperBlock.free_blocks_count`.


3. **Data Write:**
* Loop through the data chunks.
* `memcpy` data into the physical blocks pointed to by the Inode.


4. **Metadata Update:**
* Update `inode.file_size`.



---

## 5. Implementation Rules

1. **Error Handling:** Every `allocate` call can fail (return -1). Handle it (Disk Full).
2. **Safety:** Never use `strcpy` for filenames. Use `strncpy` to respect the fixed buffer size.
3. **Testing:** Build the `BitmapManager` first. If you can't reliably track free space, nothing else matters.