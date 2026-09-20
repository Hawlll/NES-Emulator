#pragma once
#include "Memory.h"
#include "Video.h"
#include <iostream>

struct Bus {

    Memory& mem;
    Video& video;

    Bus (Memory& memory, Video& vd) : mem(memory), video(vd) {}

    uint8_t Read(uint16_t Address) { // NES memory map

        if (Address <= 0x1FFF) { // internal ram. NES Hardware limitations led to 2KiB. To maintain compatibility with games, address will mirror after 0x07FF

            return mem.Read(Address & 0x07FF); // Bit AND with lower 11 bits since address space is 11 bits with 2KiB of memory
        }

        else if (Address >= 0x2000 && Address <= 0x3FFF) { // Pixel Processing Unit (PPU) Registers. Only 8 registers, so it will repeat to keep compatibility
            return mem.Read(0x2000 + (Address & 0x0007));
        }

        else if (Address >= 0x4000 && Address <= 0x401F) { // Audio Processing Units and Input/Output
            return 0x00;

        }

        else if (Address >= 0x4020) { // Cartridge
            return 0x00;
        }

        else { // address outside space
            throw std::runtime_error("Address outside range 0x0000-0xFFFF" + std::format("{:#X}\n", (int)Address));
        }

    }

    void Write(uint16_t Address, uint8_t Value) {

         if (Address <= 0x1FFF) {

            mem.Write(Address & 0x07FF, Value); // Bit AND with lower 11 bits since address space is 11 bits with 2KiB of memory
        }

        else if (Address >= 0x2000 && Address <= 0x3FFF) { // Pixel Processing Unit (PPU) Registers
            mem.Write(0x2000 + (Address & 0x0007), Value);

        }

        else if (Address >= 0x4000 && Address <= 0x401F) { // Audio Processing Units and Input/Output
        }

        else if (Address >= 0x4020) { // Cartridge
        }

        else { // address outside space
            throw std::runtime_error("Address outside range 0x0000-0xFFFF" + std::format("{:#X}\n", (int)Address));
        }
    }


};
