#pragma once
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

    std::vector<std::vector<uint8_t>> frameBuffer{240, std::vector<uint8_t>(256, 0)}; // stores decoded tiles

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
            std::cout << std::format("Reading to {:X}", (int)address) << std::endl;
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
            std::cout << std::format("Writing to {:X}", (int)address) << std::endl;
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

    auto DecodeTile(uint16_t Address) { // fetch tile in chrROM. tiles are 16 bytes (8x8) using two-bit plan strategy
        //  tiles are stored in chrROM sequentially, where the value of pixel one is bit 7 of byte 0 and byte 8
        // the pixel values are the chosen color index in a palette
        std::vector<std::vector<uint8_t>> pixels(8, std::vector<uint8_t>(8, 0));
        for (size_t row = 0; row < 8; row++) {
            uint8_t lowByte = ppuBus.Read(Address + row);
            uint8_t highByte = ppuBus.Read(Address + row + 8);
            for (size_t col = 0; col < 8; col++) {
                uint8_t lowBit = (lowByte & (1 << (7-col))) > 0;
                uint8_t highBit = (highByte & (1 << (7-col))) > 0;

                uint8_t pixel = (highBit << 1) | lowBit;
                pixels[row][col] = pixel;
            }

        }
        return pixels;
    }

    void DrawTile(std::vector<std::vector<uint8_t>>& tile, uint8_t row, uint8_t col) {
        // Take decoded tile and place in frame buffer based on row and col position

        for (size_t y = 0; y < 8; y++) {
            for (size_t x = 0; x < 8; x++) {
                frameBuffer[row+y][col+x] = tile[y][x];
            }
        }

    }

    void DrawNameTable(uint16_t Address) {
        // 30x32 screen. bytes stored sequentially. each byte stores tile id. position on screen is implied
        // goes through each position on the screen, fetches and decodes tile with tile ID, draws tile in frameBuffer
        uint16_t patternBaseAddress = 0x0000; // where the tiles are stored in CHR ROM
                if ((PPUCTRL & (1 << 4)) > 0) {
                    patternBaseAddress = 0x1000;
                }

        for (size_t screenY = 0; screenY < 30; screenY++) {
            for (size_t screenX = 0; screenX < 32; screenX++) {
                uint8_t tileId = ppuBus.Read(Address + (screenY * 32) + screenX);
                std::vector<std::vector<uint8_t>> decodedTile = DecodeTile(patternBaseAddress + (tileId*16));
                DrawTile(decodedTile, screenY*8, screenX*8);
            }
        }
    }

    uint8_t AttributeTableLookup(uint16_t attributeTableAddr, uint8_t tileRow, uint8_t tileCol) { // 4x4 tile region for each byte,  two bits for each quadrant (2x2 tiles)
        // look up attribute byte for specific tile and extract palette selection
        uint16_t attributeByteAddr = attributeTableAddr + ((tileRow/4) * 8) + (tileCol/4);
        uint8_t attributeByte = ppuBus.Read(attributeByteAddr);

        uint8_t quadY = tileRow % 4;
        uint8_t quadX = tileCol % 4;

        if (quadY < 2 && quadX < 2) { // top left
            return (attributeByte & (1 << 1)) | (attributeByte & 0x01);
        }
        else if (quadY < 2 && quadX >= 2) { // top right
            return ((attributeByte & (1 << 3)) >> 2) | ((attributeByte & (1 << 2)) >> 2);
        }
        else if (quadY >= 2 && quadX < 2) { // bottom left
            return ((attributeByte & (1 << 5)) >> 4) | ((attributeByte & (1 << 4)) >> 4);
        }
        else { // bottom right
            return ((attributeByte & (1 << 7)) >> 6) | ((attributeByte & (1 << 6)) >> 6);
        }
    }

};
