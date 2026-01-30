#include "fs/disk.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <iomanip>

// GEMINI FIX: Required for mmap, open, close
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

Disk::Disk(size_t capacity_bytes, const char* filename) {
    if (capacity_bytes % BLOCK_SIZE != 0) {
        throw std::invalid_argument("Disk capacity must be multiple of 4096");
    }

    // --- OLD ABSTRACTION ---
    // this->memory.resize(capacity_bytes, 0);
    // -----------------------

    // --- NEW ABSTRACTION ---
    // 1. Open File (Create if doesn't exist)
    this->fd = open(filename, O_RDWR | O_CREAT, 0666);
    if (this->fd == -1) {
        throw std::runtime_error("Failed to open disk image file");
    }

    // 2. Resize File to match disk capacity (Physical Allocation)
    if (ftruncate(this->fd, capacity_bytes) == -1) {
        close(this->fd);
        throw std::runtime_error("Failed to resize disk image");
    }

    // 3. Map File to Memory (The "Magic" Link)
    this->mapped_data = (uint8_t*)mmap(NULL, capacity_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, this->fd, 0);

    if (this->mapped_data == MAP_FAILED) {
        close(this->fd);
        throw std::runtime_error("Failed to mmap disk image");
    }
    // -----------------------

    this->BLOCK_COUNT = capacity_bytes / BLOCK_SIZE;
}

Disk::~Disk() {
    // --- NEW ABSTRACTION ---
    if (this->mapped_data != MAP_FAILED) {
        // Force OS to write "Dirty Pages" to physical disk
        msync(this->mapped_data, BLOCK_COUNT * BLOCK_SIZE, MS_SYNC);
        // Unmap memory
        munmap(this->mapped_data, BLOCK_COUNT * BLOCK_SIZE);
    }
    if (this->fd != -1) {
        close(this->fd);
    }
}

void Disk::read_block(int block_id, void* buffer) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        throw std::out_of_range("Disk Read Error: Block ID out of bounds");
    }
    size_t offset = block_id * BLOCK_SIZE;

    // --- OLD ABSTRACTION ---
    // std::memcpy(buffer, &memory[offset], BLOCK_SIZE);

    // --- NEW ABSTRACTION ---
    std::memcpy(buffer, this->mapped_data + offset, BLOCK_SIZE);
}

void Disk::write_block(int block_id, const void* buffer) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        throw std::out_of_range("Disk Write Error: Block ID out of bounds");
    }
    size_t offset = block_id * BLOCK_SIZE;

    // --- OLD ABSTRACTION ---
    // std::memcpy(&memory[offset], buffer, BLOCK_SIZE);

    // --- NEW ABSTRACTION ---
    std::memcpy(this->mapped_data + offset, buffer, BLOCK_SIZE);
}

uint8_t* Disk::get_ptr(int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        throw std::out_of_range("Disk Access Error: Block ID out of bounds");
    }
    size_t offset = block_id * BLOCK_SIZE;

    // --- OLD ABSTRACTION ---
    // return &memory[offset];

    // --- NEW ABSTRACTION ---
    return this->mapped_data + offset;
}

void Disk::hex_dump(int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        throw std::out_of_range("Disk Dump Error: Block ID out of bounds");
    }
    std::cout << "--- Hex Dump of Block " << block_id << " ---\n";
    size_t start_offset = block_id * BLOCK_SIZE;
    size_t end_offset = start_offset + BLOCK_SIZE;

    for (size_t i = start_offset; i < end_offset; i++) {
        // --- OLD ABSTRACTION ---
        // int val = static_cast<int>(memory[i]);

        // --- NEW ABSTRACTION ---
        int val = static_cast<int>(this->mapped_data[i]);

        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << val << " ";
        if ((i - start_offset + 1) % 16 == 0) {
            std::cout << "\n";
        }
    }
    std::cout << std::dec << "\n";
}
