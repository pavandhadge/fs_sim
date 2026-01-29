#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <iomanip> // GEMINI FIX: Added for std::setw in hex_dump

class Disk {
private:
    std::vector<uint8_t> memory;
    const size_t BLOCK_SIZE = 4096;
    size_t BLOCK_COUNT = 0;

public:
    Disk(size_t capacity_bytes);

    void read_block(int block_id, void* buffer);
    void write_block(int block_id, const void* buffer);
    uint8_t* get_ptr(int block_id);
    void hex_dump(int block_id);

    size_t get_block_count() const { return BLOCK_COUNT; }
    size_t get_block_size() const { return BLOCK_SIZE; }
};
