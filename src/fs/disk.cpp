#include "fs/disk.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <iomanip>

Disk::Disk(size_t capacity_bytes) {
    if (capacity_bytes % BLOCK_SIZE != 0) {
        throw std::invalid_argument("Disk capacity must be multiple of 4096");
    }
    this->memory.resize(capacity_bytes, 0);
    this->BLOCK_COUNT = capacity_bytes / BLOCK_SIZE;
}

void Disk::read_block(int block_id, void* buffer) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        throw std::out_of_range("Disk Read Error: Block ID out of bounds");
    }
    size_t offset = block_id * BLOCK_SIZE;
    std::memcpy(buffer, &memory[offset], BLOCK_SIZE);
}

void Disk::write_block(int block_id, const void* buffer) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        throw std::out_of_range("Disk Write Error: Block ID out of bounds");
    }
    size_t offset = block_id * BLOCK_SIZE;
    std::memcpy(&memory[offset], buffer, BLOCK_SIZE);
}

uint8_t* Disk::get_ptr(int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        throw std::out_of_range("Disk Access Error: Block ID out of bounds");
    }
    size_t offset = block_id * BLOCK_SIZE;
    return &memory[offset];
}

void Disk::hex_dump(int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        throw std::out_of_range("Disk Dump Error: Block ID out of bounds");
    }
    std::cout << "--- Hex Dump of Block " << block_id << " ---\n";
    size_t start_offset = block_id * BLOCK_SIZE;
    size_t end_offset = start_offset + BLOCK_SIZE;

    for (size_t i = start_offset; i < end_offset; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(memory[i]) << " ";
        if ((i - start_offset + 1) % 16 == 0) {
            std::cout << "\n";
        }
    }
    std::cout << std::dec << "\n";
}
