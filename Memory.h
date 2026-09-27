#pragma once
#include <vector>
#include <cstdint>

struct Memory {
    std::vector<uint8_t> mem;
    uint32_t mem_size_bytes;

    Memory (uint32_t bytes) : mem(bytes, 0), mem_size_bytes(bytes) {}

    uint8_t Read(uint16_t Address) {
        return mem[Address];
    }

    void Write(uint16_t Address, uint8_t Value) {
        mem[Address] = Value;
    }
};
