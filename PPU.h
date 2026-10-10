#pragma once
#include <iostream>
#include "PPUBus.h"

struct PPU {

    uint8_t PPUCTRL = 0x00; // PPU configuration register at 0x2000 (how to operate)
    uint8_t PPUMASK = 0x00; // Rendering configuration register at 0x2001 (how to render)
    uint8_t PPUSTATUS = 0x00; // PPU status register at 0x2002 (status)

    // internal registers
    uint16_t v = 0x0000; // current vram address register (current position in background and is ppu_address)
    uint16_t t = 0x0000; // temporary vram address register (builds v during rendering)
    uint8_t x = 0x00; // fine x regsiter
    bool w = false; // write toggle register

    // register that store data before entering shift registers
    uint8_t nextTileId = 0x00;
    uint8_t nextTileAttribute = 0x00;
    uint8_t nextTileLow = 0x00;
    uint8_t nextTileHigh = 0x00;

    // background shift regsiters

    // tile rendering pipeline
    uint16_t backgroundShiftPatternLow = 0x0000;
    uint16_t backgroundShiftPatternHigh = 0x0000;

    // palette selection pipeline
    uint16_t backgroundShiftAttributeLow = 0x0000;
    uint16_t backgroundShiftAttributeHigh = 0x0000;



    uint16_t scanline = 0; // step of screen (0-261)
    uint16_t dots = 0; // PPU cycles (0-340)
    uint8_t read_buffer = 0x00; // internal read buffer since reads from CPU are delayed

    bool send_NMI = false; // send NMI to cpu if true

    uint8_t OAMADDR = 0x00; // register for cpu to access OAM

    std::vector<std::vector<uint8_t>> frameBuffer{240, std::vector<uint8_t>(256, 0)}; // stores rendered frame
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
            w = false;
            return prev;
        }
        else if (address == 0x2004) { // OAM
            return ppuBus.OAMRead(OAMADDR);
        }
        else if (address == 0x2007) {

            uint8_t ret_val = 0x00;
            if (v >= 0x3F00) { // palette ram
                ret_val = ppuBus.Read(v);
                read_buffer = ppuBus.Read(v - 0x1000); // corresponding value in nametable
            }
            else {
                ret_val = read_buffer;
                read_buffer = ppuBus.Read(v);
            }

            if ((PPUCTRL & 0x04) > 0) { // if vram address increment is set
                v += 32;
            }
            else {
                v += 1;
            }
            v &= 0x3FFF;
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
            uint8_t nametableSelect = data & 0x03;
            t &= 0xF3FF; // clear bits 10-11 to store nametable select
            t |= (nametableSelect << 10);
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
        else if (address == 0x2005) { //write to scroll registers
            if (w == false) {
                uint8_t coarseX = data / 8;
                uint8_t fineX = data % 8;

                t &= 0xFFE0; // clear lower 5 bits for coarseX
                t |= coarseX;

                x = fineX;
            }
            else {
                uint8_t coarseY = data / 8;
                uint8_t fineY = data % 8;

                t &= 0x0C1F; // clear bits 5-9 for coarseY and bits 12-14 for fineY
                t |= (coarseY << 5);
                t |= (fineY << 12);

            }
            w = !w;
        }
        else if (address == 0x2006) {
            if (w == false) {
                t &= 0x00FF; // clear upper 8 bits
                t |= (data & 0x3F) << 8;
            }
            else {
                t &= 0xFF00;
                t |= data;
                v = t;
            }
            w = !w;
        }
        else if (address == 0x2007) {

            ppuBus.Write(v, data);
            if ((PPUCTRL & 0x04) > 0) { // if vram address increment is set
                v += 32;
            }
            else {
                v += 1;
            }
            v &= 0x3FFF;
        }
        else {
            std::cout << std::format("Writing to {:X}", (int)address) << std::endl;
            throw std::runtime_error("Unsupported PPU register write");
        }
    }

    void Clock() {

        RenderPixel(); // render sprite and/or background pixel
        BackgroundPipelineHandler(); // prepare or load next pixel/palette
        ScrollingHandler(); // update next tile fetch position
        VblankHandler(); // tell CPU to update sprites, background, etc, when in VBlank
        AdvanceCounters(); // advance frame timing

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
    void DrawBGPixel() {

        uint16_t screenY = scanline;
        uint16_t screenX = dots - 1;

        uint8_t pixelValue = PullBackgroundPixel();
        uint8_t paletteSelection = PullPaletteSelection();

        uint8_t colorInd = ColorIndexLookupBackground(paletteSelection, pixelValue);

        frameBuffer[screenY][screenX] = colorInd;
        backgroundOpaque[screenY][screenX] = (pixelValue != 0);

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

    void DrawSpritePixel(){

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
                if (spriteNum == 0 && backgroundOpaque[screenY][screenX]) { // sprite 0 hit
                    PPUSTATUS |= (1 << 6);
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
                if (spriteNum == 0 && backgroundOpaque[screenY][screenX]) { // sprite 0 hit
                    PPUSTATUS |= (1 << 6);
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

    void IncrementCoarseX() { // handle horizontal crossing and updating nametable select bit in
        uint8_t coarseX = (v & 0x001F);
        if (coarseX == 0x1F) { // horizontal cross
            v ^= (1 << 10);
            v &= 0xFFE0;

        }
        else {
            v++;
        }

    }

    void IncrementCoarseY() {
        uint8_t coarseY = (v & 0x03E0) >> 5;
        v &= 0xFC1F;

        if (coarseY == 0x1D) {
            v ^= (1 << 11);
        }
        else if (coarseY == 0x1E) {
            coarseY++;
            v |= (coarseY << 5);
        }
        else if (coarseY == 0x1F) {
            // coarseY is set to 0, don't cross nametable
        }
        else {
            coarseY++;
            v |= (coarseY << 5);
        }
    }

    void IncrementFineY() {
        uint8_t fineY = (v & 0x7000) >> 12;
        v &= 0x0FFF;

        if (fineY == 0x07) {
            IncrementCoarseY();
        }
        else {
            fineY++;
            v |= (fineY << 12);
        }
    }

    void CopyHorizontal() { // copies coarseX and horizontal nametable from t => v
        uint8_t coarseX = (t & 0x001F);
        uint16_t horizontalNametableInd = (t & (1 << 10));
        v &= 0xFBE0;
        v |= coarseX;
        v |= horizontalNametableInd;
    }

    void CopyVertical() { // copies coarseY, vertical nametable, and fineY from t => v
        uint8_t coarseY = (t & 0x03E0) >> 5;
        uint8_t fineY = (t & 0x7000) >> 12;
        uint16_t verticalNametableInd = (t & (1 << 11));
        v &= 0x041F;
        v |= (coarseY << 5);
        v |= (fineY << 12);
        v |= verticalNametableInd;
    }

    void FetchNametableByte() {
        uint8_t coarseY = (v & 0x03E0) >> 5;
        uint8_t coarseX = (v & 0x001F);
        uint8_t nametableSelect = (v >> 10) & 0x03;

        uint16_t nametableByteAddress = 0x2000 + (0x0400 * nametableSelect) + (coarseY * 32) + coarseX;

        nextTileId = ppuBus.Read(nametableByteAddress);
    }

    void FetchAttributeByte() {
        uint8_t coarseY = (v & 0x03E0) >> 5;
        uint8_t coarseX = (v & 0x001F);
        uint8_t nametableSelect = (v >> 10) & 0x03;

        uint16_t attributeByteAddress = 0x2000 + (0x0400 * nametableSelect) + 960 + ((coarseY / 4) * 8) + (coarseX / 4);
        uint8_t attributeByte = ppuBus.Read(attributeByteAddress);

        uint8_t quadY = coarseY % 4;
        uint8_t quadX = coarseX % 4;

        if (quadY < 2 && quadX < 2) { // top left
            nextTileAttribute = (attributeByte & (1 << 1)) | (attributeByte & 0x01);
        }
        else if (quadY < 2 && quadX >= 2) { // top right
            nextTileAttribute = ((attributeByte & (1 << 3)) >> 2) | ((attributeByte & (1 << 2)) >> 2);
        }
        else if (quadY >= 2 && quadX < 2) { // bottom left
            nextTileAttribute = ((attributeByte & (1 << 5)) >> 4) | ((attributeByte & (1 << 4)) >> 4);
        }
        else { // bottom right
            nextTileAttribute = ((attributeByte & (1 << 7)) >> 6) | ((attributeByte & (1 << 6)) >> 6);
        }
    }

    void FetchPatternLowByte() {

        uint8_t fineY = (v & 0x7000) >> 12;

        uint16_t patternBaseAddress = 0x0000;
        if ((PPUCTRL & (1 << 4)) > 0) {
            patternBaseAddress = 0x1000;
        }
        uint16_t patternAddress = patternBaseAddress + (nextTileId * 16) + fineY;

        nextTileLow = ppuBus.Read(patternAddress);

    }

    void FetchPatternHighByte() {

        uint8_t fineY = (v & 0x7000) >> 12;

        uint16_t patternBaseAddress = 0x0000;
        if ((PPUCTRL & (1 << 4)) > 0) {
            patternBaseAddress = 0x1000;
        }
        uint16_t patternAddress = patternBaseAddress + (nextTileId * 16) + fineY + 8;

        nextTileHigh = ppuBus.Read(patternAddress);

    }

    void LoadBackgroundShiftRegisters() {

        uint8_t lowBit  = (nextTileAttribute & 0x01) > 0 ? 0xFF : 0x00;
        uint8_t highBit = (nextTileAttribute & 0x02) > 0 ? 0xFF : 0x00;
        backgroundShiftAttributeLow = (backgroundShiftAttributeLow & 0xFF00) | lowBit;
        backgroundShiftAttributeHigh = (backgroundShiftAttributeHigh & 0xFF00) | highBit;

        backgroundShiftPatternLow = (backgroundShiftPatternLow & 0xFF00) | nextTileLow;
        backgroundShiftPatternHigh = (backgroundShiftPatternHigh & 0xFF00) | nextTileHigh;

    }

    void ShiftBackgroundRegisters() { // bit shift left pipelines to process next pixel/palette
        backgroundShiftAttributeHigh = backgroundShiftAttributeHigh << 1;
        backgroundShiftAttributeLow = backgroundShiftAttributeLow << 1;
        backgroundShiftPatternHigh = backgroundShiftPatternHigh << 1;
        backgroundShiftPatternLow = backgroundShiftPatternLow << 1;
    }

    uint8_t PullBackgroundPixel() { // pull from fineX offset on pattern pipeline
        uint16_t mask = 0x8000 >> x;

        uint8_t lowBit = (backgroundShiftPatternLow & mask) > 0 ? 0x01 : 0x00;
        uint8_t highBit = (backgroundShiftPatternHigh & mask) > 0 ? 0x01 : 0x00;

        uint8_t pixelValue = (highBit << 1) | lowBit;

        return pixelValue;
    }

    uint8_t PullPaletteSelection() { // pull from fineX offset on palette pipeline
        uint16_t mask = 0x8000 >> x;

        uint8_t lowBit = (backgroundShiftAttributeLow & mask) > 0 ? 0x01 : 0x00;
        uint8_t highBit = (backgroundShiftAttributeHigh & mask) > 0 ? 0x01 : 0x00;

        uint8_t paletteSelection = (highBit << 1) | lowBit;

        return paletteSelection;
    }

    void AdvanceCounters() { // increments ppu dot and scanline

        dots++;

        if (dots >= 341) { // finished scanline
            dots = 0;
            scanline++;

            if (scanline >= 262) { // finished screen
                scanline = 0;
            }
        }
    }

    void VblankHandler() { // updates registers when entering and leaving Vblank
        if (scanline == 241 && dots == 1) { // in VBlank
            PPUSTATUS |= 0b10000000;

            if (PPUCTRL & 0x80) {
                send_NMI = true;
            }
        }

        else if (scanline == 261 && dots == 1) { // in Prerender
            PPUSTATUS &= 0b00111111; // clear vblank and sprite 0 hit
            ClearFrame();
        }
    }

    void RenderPixel() { // draws bg or sprite pixel when rendering visible portion of screen and does not conflict with registers
        if (((PPUMASK & 0x18) > 0) && (scanline < 240) && (dots >= 1) && (dots <= 256)) { // background/sprite rendering enabled and within visible rendering

            if ((PPUMASK & 0x08) && ((PPUMASK & 0x02) || dots > 8)) { // background rendering enabled and not in left-8 pixels if left-8 pixels rendering disabled
                DrawBGPixel();
            }
            if ((PPUMASK & 0x10) && ((PPUMASK & 0x04) || dots > 8)) { // sprite rendering enabled and not in left-8 pixels if left-8 pixels rendering disabled
                DrawSpritePixel();
            }

        }
    }

    void BackgroundPipelineHandler() { // orchrestrates shift registers depending on frame timing

        if (!(PPUMASK & 0x18)) { // exit early if bg and sprite rendering disabled
            return;
        }

        if (scanline >= 240 && scanline != 261) { // exit early if not in visible scanlines or pre-render scanline 261
            return;
        }

        if ((dots >= 2 && dots <= 257) || (dots >= 322 && dots <= 337)) {
            ShiftBackgroundRegisters();
        }

        if ((dots >= 1 && dots <= 256) || (dots >= 321 && dots <= 336)) { // visible or pre-render dots

            switch (dots % 8) { // eight dot fetch sequence
                case 1:
                    LoadBackgroundShiftRegisters();
                    FetchNametableByte();
                    break;
                case 3:
                    FetchAttributeByte();
                    break;
                case 5:
                    FetchPatternLowByte();
                    break;
                case 7:
                    FetchPatternHighByte();
                    break;
                case 0:
                    IncrementCoarseX();
                    break;
            }

        }

        if (dots == 337) { // load prefetched tile rows from dots 321-336
            LoadBackgroundShiftRegisters();
        }

    }

    void ScrollingHandler() { // updates register v depending on frame timing

        if (!(PPUMASK & 0x18)) { // exit early if bg and sprite rendering disabled
            return;
        }

        if (scanline >= 240 && scanline != 261) { // exit early if not in visible scanlines or pre-render scanline 261
            return;
        }

        if (dots == 256) { // next pixel row
            IncrementFineY();
        }
        else if (dots == 257) { // restore starting X with scroll offset from t
            CopyHorizontal();
        }
        else if ((scanline == 261) && (dots >= 280 && dots <= 304)) { // prerender
            CopyVertical();
        }
    }


};
