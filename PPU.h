#include <iostream>

struct PPU {

    uint8_t PPUCTRL = 0x00; // PPU configuration register at 0x2000 (how to operate)
    uint8_t PPUMASK = 0x00; // Rendering configuration register at 0x2001 (how to render)
    uint8_t PPUSTATUS = 0x00; // PPU status register at 0x2002 (status)

    int scanline = 0; // step of screen
    int dots = 0; // PPU cycles

    uint8_t CPURead(uint16_t address) {
        if (address == 0x2002) {
            return PPUSTATUS;
        }
        else {
            throw std::runtime_error("Unsupported PPU register read");
        }

    }

    void CPUWrite(uint16_t address, uint8_t data) {
        if (address == 0x2000) {
            PPUCTRL = data;
        }
        else if (address ==  0x2001) {
            PPUMASK = data;
        }
        else {
            throw std::runtime_error("Unsupported PPU register write");
        }
    }

    void Clock() {

        dots++;

        if (dots >= 341) {
            dots = 0;
            scanline++;

            if (scanline >= 262) { // finished screen
                scanline = 0;
            }
        }

    }

};
