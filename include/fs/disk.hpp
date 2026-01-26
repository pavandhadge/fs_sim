#pragma once

#include <cstdint>
#include <vector>
// #include <iostream>
#include <cstring>
// #include <stdexcept>

class Disk {
private:
    std::vector<uint8_t> memory;
    const size_t BLOCK_SIZE = 4096;
    size_t BLOCK_COUNT = 0;

public:
    // Constructor
    Disk(size_t capacity_bytes);

    // Core Operations
    void read_block(int block_id, void* buffer);
    void write_block(int block_id, const void* buffer);

    // The "Veteran" Helper (Critical for casting structs)
    uint8_t* get_ptr(int block_id);

    // Debugging
    void hex_dump(int block_id);

    // Getters
    size_t get_block_count() const { return BLOCK_COUNT; }
    size_t get_block_size() const { return BLOCK_SIZE; }
};
