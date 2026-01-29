
# fs_sim: A Unix-like File System Simulation

This project is a functional simulation of a block-based file system, implemented in C++17. It mimics the architecture of early Unix file systems (specifically the Ext2 design), featuring inodes, block groups, bitmaps, and hierarchical directory structures. The system runs on a simulated "disk" (a contiguous memory buffer) and provides a shell-like interface (REPL) for interacting with the file system.

## Project Architecture

The file system divides the disk into distinct regions to manage metadata and data efficiently. The architecture is built around the following core components:

* **Disk Simulation:** A raw byte vector acts as the physical storage layer. All reads and writes happen in 4096-byte blocks.
* **SuperBlock:** Located at the beginning of the disk, this structure stores global metadata, including the total block count, inode count, and geometry of the file system.
* **Block Group Managers:** The disk is divided into "Block Groups." Each group manages its own set of inodes and data blocks using bitmaps. This design (inspired by Ext2) helps reduce fragmentation and keeps related data physically close.
* **Inodes:** The fundamental metadata structure. Each file or directory is represented by an inode containing permissions, file type, size, and pointers to 12 direct data blocks.
* **Directory Entries:** Directories are treated as special files containing a list of `DirEntry` structures, which map human-readable filenames to inode IDs.

### Technical Capabilities

* **Formatting & Mounting:** Creating a fresh file system layout and loading the state from an existing disk.
* **File Operations:** Create (`touch`), Read, Write, and Delete (`rm`) files.
* **Directory Operations:** Create (`mkdir`), List (`ls`), and Recursively Delete (`rm -rf` equivalent).
* **Persistence Simulation:** The system correctly serializes structures to the disk buffer, allowing data to survive "remounts" (simulated reboots).

## Development Journey & Methodology

The original intent of this project was to implement a file system entirely from scratch, without external assistance or the use of AI tools, to test my understanding of operating system concepts.

However, during the implementation of the core allocation logic and memory management, I encountered significant friction with C++ pointer arithmetic, memory safety (stack vs. heap lifecycles), and the nuances of block persistence.

I chose to pivot from a "solo" approach to using AI (Gemini) as a mentor and code reviewer. The AI was utilized for:

1. **Mentorship:** Explaining the specific math required for Block Group offsets and Inode-to-Block translation.
2. **Error Correction:** Identifying critical bugs, such as buffer overflows during formatting and memory leaks during file deletion.
3. **Tooling:** Generating the REPL interface and the CMake testing suite to allow me to focus on the core file system logic.

While I did not strictly adhere to my original goal of zero-AI assistance, the process mirrored a real-world engineering environment where a junior developer works alongside a senior architect. The resulting code reflects my own architectural decisions, refined and stabilized through that feedback loop.

## Build and Usage

### Prerequisites

* C++17 compliant compiler (GCC/Clang)
* CMake (3.10+)

### Building

```bash
mkdir build && cd build
cmake ..
make

```

### Running the Shell

To start the interactive file system shell:

```bash
./fs_sim

```

### Running Tests

A comprehensive test suite is included to verify persistence, memory allocation, and large file handling.

```bash
make check

```

## Future Scope

There are several areas where this simulation could be expanded to mirror a production file system:

* **Indirect Addressing:** Currently, files are limited to 12 direct blocks (approx. 48KB). Implementing singly and doubly indirect pointers would allow for much larger files.
* **File Backing:** The "disk" currently lives in RAM. Modifying the `Disk` class to `mmap` a binary file on the host OS would allow for true persistence across program restarts.
* **Permissions:** Adding user IDs, group IDs, and read/write/execute permission checks to the inodes.
* **Journaling:** Implementing a write-ahead log to prevent data corruption during crashes (similar to Ext3/4).
