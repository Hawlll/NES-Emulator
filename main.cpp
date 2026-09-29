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

    Cartridge cart;

    PPUBus ppuBus(cart, nametableRam, paletteRam);
    PPU ppu(ppuBus);

    Bus bus(cpuRam, cart, ppu);

    NES nes(cpu, ppu, bus);

    cart.Load("rom_tests/nametable_render_test.nes");

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
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    SDL_CreateWindowAndRenderer("NES NameTable Viewer", width, height, 0, &window, &renderer);

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING, width, height);

    SDL_Color colors[256]{};

    colors[0] = {0,   0,   0,   255};
    colors[1] = {85,  85,  85,  255};
    colors[2] = {170, 170, 170, 255};
    colors[3] = {255, 255, 255, 255};

    SDL_Palette* palette = SDL_CreatePalette(256);
    SDL_SetPaletteColors(palette, colors, 0, 256);
    SDL_SetTexturePalette(texture, palette);
    SDL_DestroyPalette(palette);

    bool running = true;
    SDL_Event event;

    for (int i = 0; i < 500000; i++) {
        nes.Clock();
    }

    ppu.DrawNameTable(0x2000);

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
