#pragma once
#include <iostream>
#include "PPUBus.h"

struct PPU {

    uint8_t PPUCTRL = 0x00; // PPU configuration register at 0x2000 (how to operate)
    uint8_t PPUMASK = 0x00; // Rendering configuration register at 0x2001 (how to render)
    uint8_t PPUSTATUS = 0x00; // PPU status register at 0x2002 (status)

    uint16_t scanline = 0; // step of screen
    uint16_t dots = 0; // PPU cycles
    uint16_t ppu_address = 0x0000; // address register (14 bit address space)
    uint8_t read_buffer = 0x00; // internal read buffer since reads from CPU are delayed

    bool send_NMI = false; // send NMI to cpu if true
    bool write_toggle = false; // write order of PPU address (false=expecting first write)

    uint8_t OAMADDR = 0x00; // register for cpu to access OAM

    std::vector<std::vector<uint8_t>> frameBuffer{240, std::vector<uint8_t>(256, 0)}; // stores decoded tiles
    std::vector<std::vector<bool>> backgroundOpaque{240, std::vector<bool>(256, false)}; // stores whether background is universal background color (CHR pixel value was 0)

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
        else if (address == 0x2004) { // OAM
            return ppuBus.OAMRead(OAMADDR);
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
        else if (address == 0x2003) {
            OAMADDR = data;
        }
        else if (address == 0x2004) {
            ppuBus.OAMWrite(OAMADDR, data);
            OAMADDR++;
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

        if (scanline < 240 && (dots >= 1 && dots <= 256)) {
                //background
                if ((PPUMASK & (1 << 3)) > 0) { // is background render enabled
                    uint16_t baseNameTableAddr = 0x2000;
                    uint8_t addrSelection = PPUCTRL & 0x03;
                    if (addrSelection == 0x00) {}
                    else if (addrSelection == 0x01) {baseNameTableAddr = 0x2400;}
                    else if (addrSelection == 0x02) {baseNameTableAddr = 0x2800;}
                    else {baseNameTableAddr = 0x2C00;}
                    if (((PPUMASK & (1 << 1)) == 0) && (dots-1 < 8)) { // don't render for leftmost 8 pixels
                    // don't render pixel
                    }
                    else {
                        DrawBGPixel(baseNameTableAddr, dots, scanline);
                    }

                }
                //sprite
                if ((PPUMASK & (1 << 4)) > 0) { // is sprite render enabled
                    if ((PPUMASK & (1 << 2)) > 0) { // should render sprite at leftmost 8 pixels
                        DrawSpritePixel(dots, scanline);
                    }
                    else {
                        if (dots-1 < 8) {
                            // don't render sprite pixel
                        }
                        else {
                            DrawSpritePixel(dots, scanline);
                        }
                    }
                }

            }

        if (scanline == 241 && dots == 1) { // in VBlank
            PPUSTATUS |= 0b10000000;

            if (PPUCTRL & 0x80) {
                send_NMI = true;
            }
        }

        else if (scanline == 261 && dots == 1) { // in Prerender
            PPUSTATUS &= 0b01111111;
            ClearFrame();
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

    void DrawTile(const std::vector<std::vector<uint8_t>>& tile, uint8_t row, uint8_t col, uint8_t paletteIndex) {
        // Take decoded tile and place in frame buffer based on row and col position

        for (size_t y = 0; y < 8; y++) {
            for (size_t x = 0; x < 8; x++) {
                if ((PPUMASK & (1 << 1)) > 0) { // should render leftmost 8 pixels
                    uint8_t colorInd = ColorIndexLookupBackground(paletteIndex, tile[y][x]);
                    frameBuffer[row+y][col+x] = colorInd;
                    backgroundOpaque[row+y][col+x] = (tile[y][x] != 0);
                }
                else {
                    if (col + x < 8) {
                        frameBuffer[row+y][col+x] = ppuBus.Read(0x3F00);
                        backgroundOpaque[row+y][col+x] = false;
                    }
                    else {
                        uint8_t colorInd = ColorIndexLookupBackground(paletteIndex, tile[y][x]);
                        frameBuffer[row+y][col+x] = colorInd;
                        backgroundOpaque[row+y][col+x] = (tile[y][x] != 0);
                    }
                }
            }
        }

    }
    void DrawBGPixel(uint16_t Address, uint16_t dots, uint16_t scanline) {

        uint16_t screenY = scanline;
        uint16_t screenX = dots - 1;

        uint8_t tileRow = screenY / 8;
        uint8_t tileCol = screenX / 8;

        uint8_t localX = screenX % 8;
        uint8_t localY = screenY % 8;

        uint16_t patternBaseAddress = 0x0000;
        if ((PPUCTRL & (1 << 4)) > 0) {
            patternBaseAddress = 0x1000;
        }
        uint8_t tileId = ppuBus.Read(Address + (tileRow * 32) + tileCol);
        uint8_t paletteIndex = AttributeTableLookup(Address + 960, tileRow, tileCol);
        auto decodedTile = DecodeTile(patternBaseAddress + (tileId*16));
        uint8_t colorInd = ColorIndexLookupBackground(paletteIndex, decodedTile[localY][localX]);
        frameBuffer[screenY][screenX] = colorInd;
        backgroundOpaque[screenY][screenX] = (decodedTile[localY][localX] != 0);

    }

    auto DetermineSpriteNum(uint16_t screenX, uint16_t screenY) {
        std::vector<uint16_t> sprites;
        for (uint16_t i = 0; i < 64; i++) {
            uint16_t Y = ppuBus.OAMRead(i*4);
            uint16_t X = ppuBus.OAMRead((i*4)+3);
            bool sprite16Size = (PPUCTRL & (1 << 5)) > 0;
            if (sprite16Size) {
                if ((screenY >= Y+1 && screenY < Y+17) && (screenX >= X && screenX < X+8)) {
                    sprites.push_back(i);
                }
            }
            else {
                if ((screenY >= Y+1 && screenY < Y+9) && (screenX >= X && screenX < X+8)) {
                    sprites.push_back(i);
                }
            }
        }
        return sprites; // sprite not found at screen location
    }

    void DrawSpritePixel(uint16_t dots, uint16_t scanline){

        uint16_t screenX = dots - 1;
        uint16_t screenY = scanline;
        auto spriteNums = DetermineSpriteNum(screenX, screenY);
        for (uint16_t spriteNum : spriteNums) {

            uint16_t base = spriteNum * 4;
            uint16_t Y = ppuBus.OAMRead(base);
            uint8_t tileId = ppuBus.OAMRead(base + 1);
            uint8_t attribute = ppuBus.OAMRead(base + 2);
            uint16_t X = ppuBus.OAMRead(base + 3);

            uint8_t localX = screenX - X;
            uint8_t localY = screenY - (Y + 1);

            bool horizontal_flip = (attribute & (1 << 6)) > 0;
            bool vertical_flip = (attribute & (1 << 7)) > 0;

            uint8_t paletteIndex = attribute & 0x03;
            bool backgroundPriority = (attribute & (1 << 5)) > 0;


            bool spriteSize16 = (PPUCTRL & (1 << 5)) > 0;
            if (spriteSize16) { // if sprite is 8x16 (two tiles, 32 bytes long)
                uint16_t patternBaseAddress = 0x0000;
                if ((tileId & 0x01) > 0) {
                    patternBaseAddress = 0x1000;
                }

                uint8_t topTileId = tileId & 0xFE;
                uint8_t bottomTileId = topTileId + 1;

                auto bottomTile = DecodeTile(patternBaseAddress + (bottomTileId * 16));

                auto combinedTile = DecodeTile(patternBaseAddress + (topTileId * 16));
                combinedTile.insert(combinedTile.end(), bottomTile.begin(), bottomTile.end());
                if (horizontal_flip) {
                    localX = 7 - localX;
                }
                if (vertical_flip) {
                    localY = 15 - localY;
                }
                uint8_t pixel_value = combinedTile[localY][localX];
                if (pixel_value == 0) { // sprite pixel is transparent or opaque background with priority
                    continue;
                }
                if (backgroundPriority && backgroundOpaque[screenY][screenX]) {
                    return; // this is winning sprite, but background covers it
                }
                uint8_t color = ColorIndexLookupSprite(paletteIndex, pixel_value);
                frameBuffer[screenY][screenX] = color;
                return; // stop since we found first non-transparent sprite pixel

                }
            else {
                uint16_t patternBaseAddress = 0x0000; // where the tiles are stored in CHR ROM
                if ((PPUCTRL & (1 << 3)) > 0) {
                    patternBaseAddress = 0x1000;
                }

                uint16_t spriteTileAddress = patternBaseAddress + (tileId * 16);
                auto tile = DecodeTile(spriteTileAddress);
                if (horizontal_flip) {
                    localX = 7 - localX;
                }
                if (vertical_flip) {
                    localY = 7 - localY;
                }
                uint8_t pixel_value = tile[localY][localX];
                if (pixel_value == 0) { // sprite pixel is transparent or opaque background with priority
                    continue;
                }
                if (backgroundPriority && backgroundOpaque[screenY][screenX]) {
                    return; // this is winning sprite, but background covers it
                }
                uint8_t color = ColorIndexLookupSprite(paletteIndex, pixel_value);
                frameBuffer[screenY][screenX] = color;
                return; // stop since we found first non-transparent sprite pixel
            }
        }
    }

    void DrawNameTable(uint16_t Address) {
        // 30x32 screen. bytes stored sequentially. each byte stores tile id. position on screen is implied
        // goes through each position on the screen, fetches and decodes tile with tile ID, draws tile in frameBuffer

        if ((PPUMASK & (1 << 3)) > 0) { // is background render enabled
            uint16_t patternBaseAddress = 0x0000; // where the tiles are stored in CHR ROM
                if ((PPUCTRL & (1 << 4)) > 0) {
                    patternBaseAddress = 0x1000;
                }

            for (size_t screenY = 0; screenY < 30; screenY++) {
                for (size_t screenX = 0; screenX < 32; screenX++) {
                    uint8_t tileId = ppuBus.Read(Address + (screenY * 32) + screenX);
                    uint8_t paletteIndex = AttributeTableLookup(Address + 960, screenY, screenX);
                    auto decodedTile = DecodeTile(patternBaseAddress + (tileId*16));
                    DrawTile(decodedTile, screenY*8, screenX*8, paletteIndex);
                }
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

    uint8_t ColorIndexLookupBackground(uint8_t palette, uint8_t pixelValue) { // lookup color index byte from palette ram
        if (pixelValue == 0) { // universal background color
            return ppuBus.Read(0x3F00);
        }
        else {
            return ppuBus.Read(0x3F00 + (palette*4) + pixelValue);
        }
    }

    uint8_t ColorIndexLookupSprite(uint8_t palette, uint8_t pixelValue) { // lookup color index byte from palette ram
        if (pixelValue == 0) {
            throw std::runtime_error("Can't return color. Sprite color index 0 is transparent");
        }
        else {
            return ppuBus.Read(0x3F10 + (palette*4) + pixelValue);
        }
    }

    void DrawSprite(uint8_t spriteNum) {

        if ((PPUMASK & (1 << 4)) > 0) { // is sprite render enabled
            uint8_t base = spriteNum * 4;
            uint16_t Y = ppuBus.OAMRead(base);
            uint8_t tileId = ppuBus.OAMRead(base + 1);
            uint8_t attribute = ppuBus.OAMRead(base + 2);
            uint16_t X = ppuBus.OAMRead(base + 3);

            bool horizontal_flip = (attribute & (1 << 6)) > 0;
            bool vertical_flip = (attribute & (1 << 7)) > 0;

            bool backgroundPriority = (attribute & (1 << 5)) > 0;

            uint8_t paletteIndex = attribute & 0x03;

            bool spriteSize16 = (PPUCTRL & (1 << 5)) > 0;

            if (spriteSize16) { // if sprite is 8x16 (two tiles, 32 bytes long)
                uint16_t patternBaseAddress = 0x0000;
                if ((tileId & 0x01) > 0) {
                    patternBaseAddress = 0x1000;
                }

                uint8_t topTileId = tileId & 0xFE;
                uint8_t bottomTileId = topTileId + 1;

                auto bottomTile = DecodeTile(patternBaseAddress + (bottomTileId * 16));

                auto combinedTile = DecodeTile(patternBaseAddress + (topTileId * 16));
                combinedTile.insert(combinedTile.end(), bottomTile.begin(), bottomTile.end());

                for (size_t row = 0; row < 16; row++) { // theres a quirk for drawing sprites. we add one to Y since Y alone means the next scanline
                    for (size_t col = 0; col < 8; col++) {
                        uint8_t sourceRow = row;
                        uint8_t sourceCol = col;
                        if (horizontal_flip) {
                            sourceCol = 7 - sourceCol;
                        }
                        if (vertical_flip) {
                            sourceRow = 15 - sourceRow;
                        }
                        uint8_t pixel_value = combinedTile[sourceRow][sourceCol];
                        if (pixel_value != 0 && (Y+row+1) < 240 && (X+col) < 256) {
                            if (backgroundPriority && backgroundOpaque[Y+1+row][X+col]) {
                                    // don't overwrite background
                            }
                            else {
                                if ((PPUMASK & (1 << 2)) > 0) { // should render sprite at leftmost 8 pixels
                                    uint8_t color = ColorIndexLookupSprite(paletteIndex, pixel_value);
                                    frameBuffer[Y+1+row][X+col] = color;
                                }
                                else {
                                    if (col + X < 8) {
                                        // don't render sprite
                                    }
                                    else {
                                        uint8_t color = ColorIndexLookupSprite(paletteIndex, pixel_value);
                                        frameBuffer[Y+1+row][X+col] = color;
                                    }
                                }
                            }
                        }
                    }
                }

            }
            else {
                uint16_t patternBaseAddress = 0x0000; // where the tiles are stored in CHR ROM
                if ((PPUCTRL & (1 << 3)) > 0) {
                    patternBaseAddress = 0x1000;
                }

                uint16_t spriteTileAddress = patternBaseAddress + (tileId * 16);
                auto tile = DecodeTile(spriteTileAddress);


                for (size_t row = 0; row < 8; row++) { // theres a quirk for drawing sprites. we add one to Y since Y alone means the next scanline
                    for (size_t col = 0; col < 8; col++) {
                        uint8_t sourceRow = row;
                        uint8_t sourceCol = col;
                        if (horizontal_flip) {
                            sourceCol = 7 - sourceCol;
                        }
                        if (vertical_flip) {
                            sourceRow = 7 - sourceRow;
                        }
                        uint8_t pixel_value = tile[sourceRow][sourceCol];
                        if (pixel_value != 0 && (Y+row+1) < 240 && (X+col) < 256) {
                            if (backgroundPriority && backgroundOpaque[Y+1+row][X+col]) {
                                    // don't overwrite background
                            }
                            else {
                                if ((PPUMASK & (1 << 2)) > 0) { // should render sprite at leftmost 8 pixels
                                    uint8_t color = ColorIndexLookupSprite(paletteIndex, pixel_value);
                                    frameBuffer[Y+1+row][X+col] = color;
                                }
                                else {
                                    if (col + X < 8) {
                                        // don't render sprite
                                    }
                                    else {
                                        uint8_t color = ColorIndexLookupSprite(paletteIndex, pixel_value);
                                        frameBuffer[Y+1+row][X+col] = color;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }



    }

    void ClearFrame() {
        uint8_t universal_color = ppuBus.Read(0x3F00);

        for (size_t row = 0; row < 240; row++) {
            for (size_t col = 0; col < 256; col++) {
                frameBuffer[row][col] = universal_color;
                backgroundOpaque[row][col] = false;
            }
        }
    }

};
