#include <iostream>
#include "PPUBus.h"

struct PPU {

    uint8_t PPUCTRL = 0x00; // PPU configuration register at 0x2000 (how to operate)
    uint8_t PPUMASK = 0x00; // Rendering configuration register at 0x2001 (how to render)
    uint8_t PPUSTATUS = 0x00; // PPU status register at 0x2002 (status)

    int scanline = 0; // step of screen
    int dots = 0; // PPU cycles
    uint16_t ppu_address = 0x0000; // address register (14 bit address space)
    uint8_t read_buffer = 0x00; // internal read buffer since reads from CPU are delayed

    bool send_NMI = false; // send NMI to cpu if true
    bool write_toggle = false; // write order of PPU address (false=expecting first write)

    std::vector<std::vector<uint8_t>> frameBuffer{240, std::vector<uint8_t>(256, 0)};

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
        else if (address == 0x2007) {

            uint8_t ret_val = 0x00;
            if (ppu_address >= 0x3F00) { // palette ram
                ret_val = ppuBus.Read(ppu_address);
                read_buffer = ppuBus.Read(ppu_address - 0x1000); // corresponding value in nametable
            }
            else {
                ret_val = read_buffer;
                read_buffer = ppuBus.Read(ppu_address);
            }

            if ((PPUCTRL & 0x04) > 0) { // if vram address increment is set
                ppu_address += 32;
            }
            else {
                ppu_address += 1;
            }
            ppu_address &= 0x3FFF;
            return ret_val;

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
            if ((PPUCTRL & 0x04) > 0) { // if vram address increment is set
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

    auto DecodeTile(uint16_t tileAddress) {
        std::vector<std::vector<uint8_t>> pixels(8, std::vector<uint8_t>(8, 0));

        for (size_t row = 0; row < 8; row++) {

            uint8_t lowPlane  = ppuBus.Read(tileAddress + row);
            uint8_t highPlane = ppuBus.Read(tileAddress + row + 8);

            for (size_t x = 0; x < 8; x++) {
                int col = 7 - x;
                uint8_t pixel_val = (((highPlane & (1 << col)) > 0) << 1) | ((lowPlane & (1 << col)) > 0);
                pixels[row][x] = pixel_val;
            }
        }
        return pixels;
    }

    void PrintTile(const std::vector<std::vector<uint8_t>>& pixels) {
        for (size_t row = 0; row < 8; row++) {

            for (size_t x = 0; x < 8; x++) {
                char c;
                uint8_t pxl_val = pixels[row][x];

                if (pxl_val == 0x00) {
                    c = ' ';
                }
                else if (pxl_val == 0x01) {
                    c = '.';
                }
                else if (pxl_val == 0x02) {
                    c = '*';
                }
                else if (pxl_val == 0x03) {
                    c = '#';
                }
                else {
                    c = 'N';
                }
                std::cout << c;
            }
            std::cout << std::endl;
        }
    }

    void DrawTile (std::vector<std::vector<uint8_t>>& tile, int screenX, int screenY) {
        for (size_t row = 0; row < 8; row++) {

            for (size_t x = 0; x < 8; x++) {
                    frameBuffer[static_cast<size_t>(screenX) + x][static_cast<size_t>(screenY) + row] = tile[row][x];
            }
        }
    }

    void DrawPatternTable(uint16_t patternTableAddress) {
        for (int tileNum = 0; tileNum < 256; tileNum++) {
            int y = (tileNum / 16) * 8;
            int x = (tileNum % 16) * 8;
            uint16_t tileAddr = patternTableAddress + (tileNum * 16);
            auto tile = DecodeTile(tileAddr);

            DrawTile(tile, x, y);

        }
    }

    void DrawNameTable(uint16_t nametableAddress) {
        for (int row = 0; row < 31; row++) {
            for (int col = 0; col < 33; col++) {

            }
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
