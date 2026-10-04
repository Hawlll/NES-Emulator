#include "6502.h"
#include "Bus.h"
#include <iostream>
#include <SDL3/SDL.h>
#include "NES.h"

int main()
{
    CPU cpu;
    Memory cpuRam(1024*2);

    Memory nametableRam(1024*2);
    Memory paletteRam(256);
    Memory OAM(256);

    Cartridge cart;

    PPUBus ppuBus(cart, nametableRam, paletteRam, OAM);
    PPU ppu(ppuBus);

    Bus bus(cpuRam, cart, ppu);

    NES nes(cpu, ppu, bus);

    cart.Load("rom_tests/ppu_dot_sprite_pipeline_test.nes");

    cpu.Reset(bus);
//
//    cpu.PC = 0x0000;
//    bus.Write(0x0000, 0xA9);
//    bus.Write(0x0001, 0x7E);
//    bus.Write(0x0002, 0xCE);
//    bus.Write(0x0003, 0xFE);
//    bus.Write(0x0004, 0x07);
//    bus.Write(0x07FE, 0x06);
//
//    for (int i = 0; i < 30; i++) {
//        std::cout << "PC: " << std::format("{:#X}", (int)cpu.PC) << " | ";
//        std::cout << "OP: " << std::format("{:#X}", (int)cpu.instruction_latch) << " | ";
//        std::cout << "A: " << std::format("{:#X}", (int)cpu.A) << " | ";
//        std::cout << "X: " << std::format("{:#X}", (int)cpu.X) << " | ";
//        std::cout << "Y: " << std::format("{:#X}", (int)cpu.Y) << " | ";
//        std::cout << "SP: " << std::format("{:#X}", (int)cpu.SP) << " | ";
//        std::cout << "Clocks: " << (int)cpu.cycles << " | ";
//        for (int j = 7; j >= 0; j--) {std::cout << (bool)(cpu.status & (1 << j));}
//        std::cout << std::endl;
//
//        cpu.Clock(bus);
//    }

    // test ppu graphics

    int width = 256;
    int height = 240;
    int SCALE = 3;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    SDL_CreateWindowAndRenderer(
        "NES Emulator",
        width * SCALE,
        height * SCALE,
        0,
        &window,
        &renderer
    );

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_INDEX8,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height
    );

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);



    SDL_Color colors[64] = {
    { 84,  84,  84, 255}, {  0,  30, 116, 255}, {  8,  16, 144, 255}, { 48,   0, 136, 255},
    { 68,   0, 100, 255}, { 92,   0,  48, 255}, { 84,   4,   0, 255}, { 60,  24,   0, 255},
    { 32,  42,   0, 255}, {  8,  58,   0, 255}, {  0,  64,   0, 255}, {  0,  60,   0, 255},
    {  0,  50,  60, 255}, {  0,   0,   0, 255}, {  0,   0,   0, 255}, {  0,   0,   0, 255},

    {152, 150, 152, 255}, {  8,  76, 196, 255}, { 48,  50, 236, 255}, { 92,  30, 228, 255},
    {136,  20, 176, 255}, {160,  20, 100, 255}, {152,  34,  32, 255}, {120,  60,   0, 255},
    { 84,  90,   0, 255}, { 40, 114,   0, 255}, {  8, 124,   0, 255}, {  0, 118,  40, 255},
    {  0, 102, 120, 255}, {  0,   0,   0, 255}, {  0,   0,   0, 255}, {  0,   0,   0, 255},

    {236, 238, 236, 255}, { 76, 154, 236, 255}, {120, 124, 236, 255}, {176,  98, 236, 255},
    {228,  84, 236, 255}, {236,  88, 180, 255}, {236, 106, 100, 255}, {212, 136,  32, 255},
    {160, 170,   0, 255}, {116, 196,   0, 255}, { 76, 208,  32, 255}, { 56, 204, 108, 255},
    { 56, 180, 204, 255}, { 60,  60,  60, 255}, {  0,   0,   0, 255}, {  0,   0,   0, 255},

    {236, 238, 236, 255}, {168, 204, 236, 255}, {188, 188, 236, 255}, {212, 178, 236, 255},
    {236, 174, 236, 255}, {236, 174, 212, 255}, {236, 180, 176, 255}, {228, 196, 144, 255},
    {204, 210, 120, 255}, {180, 222, 120, 255}, {168, 226, 144, 255}, {152, 226, 180, 255},
    {160, 214, 228, 255}, {160, 162, 160, 255}, {  0,   0,   0, 255}, {  0,   0,   0, 255}
    };

    SDL_Palette* palette = SDL_CreatePalette(256);
    SDL_SetPaletteColors(palette, colors, 0, 64);
    SDL_SetTexturePalette(texture, palette);
    SDL_DestroyPalette(palette);

    bool running = true;
    SDL_Event event;


    for (int i = 0; i < 500000; i++) {
        nes.Clock();
    }

    for (int sprite = 4; sprite < 64; sprite++) { // change depending on ROM. This is to ensure unused sprites are off screen
        uint8_t base = sprite * 4;
        ppuBus.OAMWrite(base, 0xFF);
    }

    for (int sprite = 63; sprite >= 0; sprite--) {
        ppu.DrawSprite(sprite);
    }


    std::vector<uint8_t> screenPixels(256*240);

    for (size_t y = 0; y < 240; y++) {
        for (size_t x = 0; x < 256; x++) {
            screenPixels[y * 256 + x] = ppu.frameBuffer[y][x];
        }
    }



    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }


        SDL_UpdateTexture(texture, nullptr, screenPixels.data(), width * (int)sizeof(Uint8));

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
