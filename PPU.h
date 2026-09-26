#include <iostream>

struct PPU {

    uint8_t PPUCTRL = 0x00; // PPU configuration register

    uint8_t CPURead(uint16_t address) {
        if (address == 0x2000) {
            return PPUCTRL;
        }
        else {
            throw std::runtime_error("Unsupported PPU register read");
        }

    }

    void CPUWrite(uint16_t address, uint8_t data) {
        if (address == 0x2000) {
            PPUCTRL = data;
        }
        else {
            throw std::runtime_error("Unsupported PPU register write");
        }
    }

};
