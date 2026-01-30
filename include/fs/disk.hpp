#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <iomanip>

class Disk {
private:
    // --- OLD ABSTRACTION (Vector) ---
    // std::vector<uint8_t> memory;

    // --- NEW ABSTRACTION (Memory Mapped File) ---
    uint8_t* mapped_data; // Pointer to the file contents in RAM
    int fd;               // File Descriptor

    const size_t BLOCK_SIZE = 4096;
    size_t BLOCK_COUNT = 0;

public:
    // GEMINI FIX: Added filename parameter with default for persistence
    Disk(size_t capacity_bytes, const char* filename = "disk.img");

    // GEMINI FIX: Added destructor to close/sync the file
    ~Disk();

    void read_block(int block_id, void* buffer);
    void write_block(int block_id, const void* buffer);
    uint8_t* get_ptr(int block_id);
    void hex_dump(int block_id);

    size_t get_block_count() const { return BLOCK_COUNT; }
    size_t get_block_size() const { return BLOCK_SIZE; }
};
