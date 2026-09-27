#include <iostream>
#include "PPUBus.h"

struct PPU {

    uint8_t PPUCTRL = 0x00; // PPU configuration register at 0x2000 (how to operate)
    uint8_t PPUMASK = 0x00; // Rendering configuration register at 0x2001 (how to render)
    uint8_t PPUSTATUS = 0x00; // PPU status register at 0x2002 (status)

    int scanline = 0; // step of screen
    int dots = 0; // PPU cycles
    uint16_t ppu_address = 0x0000; // address register (14 bit address space)

    bool send_NMI = false; // send NMI to cpu if true
    bool write_toggle = false; // write order of PPU address (false=expecting first write)

    PPUBus& ppuBus;

    PPU(PPUBus& ppuB) : ppuBus(ppuB) {}

    uint8_t Read(uint16_t Address) {
        return ppuBus.Read(Address);
    }

    void Write(uint16_t Address, uint8_t Value) {
        ppuBus.Write(Address, Value);
    }


    uint8_t CPURead(uint16_t address) {
        if (address == 0x2002) {

            uint8_t prev = PPUSTATUS;
            PPUSTATUS &= 0b01111111; // clear VBlank flag
            write_toggle = false;
            return prev;
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
        else if (address == 0x2006) {
            if (write_toggle == false) {
                ppu_address = (data & 0x3F) << 8;
            }
            else {
                ppu_address |= data;
            }
            write_toggle = !write_toggle;
        }
        else if (address == 0x2007) {

            ppuBus.Write(ppu_address, data);
            if ((PPUCTRL & 0x04) > 0) {
                ppu_address += 32;
            }
            else {
                ppu_address += 1;
            }
            ppu_address &= 0x3FFF;
        }
        else {
            throw std::runtime_error("Unsupported PPU register write");
        }
    }

    void Clock() {

        if (scanline == 241 && dots == 1) { // in VBlank
            PPUSTATUS |= 0b10000000;

            if (PPUCTRL & 0x80) {
                send_NMI = true;
            }
        }

        else if (scanline == 261 && dots == 1) { // in Prerender
            PPUSTATUS &= 0b01111111;
        }

        dots++;

        if (dots >= 341) { // finished scanline
            dots = 0;
            scanline++;

            if (scanline >= 262) { // finished screen
                scanline = 0;
            }
        }

    }

};
