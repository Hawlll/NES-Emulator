#include "6502.h"
#include "Bus.h"
#include <iostream>
#include <SDL3/SDL.h>

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



    cart.Load("rom_tests/color_test.nes");

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

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL initialization failed: "
                  << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "NES Emulator",
        256 * 3,
        240 * 3,
        0
    );

    if (window == nullptr) {
        std::cerr << "Window creation failed: "
                  << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

    if (renderer == nullptr) {
        std::cerr << "Renderer creation failed: "
                  << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_SetRenderLogicalPresentation(
        renderer,
        256,
        240,
        SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
    );

    bool running = true;
    ppu.DrawPatternTable(0x0000);

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        for (size_t y = 0; y < 240; y++) {
            for (size_t x = 0; x < 256; x++) {
                uint8_t pxl_val = ppu.frameBuffer[y][x];
                uint8_t intensity = pxl_val * 85;

                SDL_SetRenderDrawColor(
                    renderer,
                    intensity,
                    intensity,
                    intensity,
                    255
                );

                SDL_RenderPoint(renderer, x, y);
            }
        }
        SDL_RenderPresent(renderer);


    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
