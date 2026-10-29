#include "6502.h"
#include "Bus.h"
#include <iostream>
#include <SDL3/SDL.h>
#include "NES.h"
#include <chrono>
#include <thread>
#include <bitset>
#include "Controller.h"

int main()
{
    CPU cpu;
    Memory cpuRam(1024*2);

    Memory nametableRam(1024*2);
    Memory paletteRam(256);
    Memory primaryOAM(256);
    Memory secondaryOAM(32);

    Cartridge cart;
    Controller crl;

    PPUBus ppuBus(cart, nametableRam, paletteRam, primaryOAM, secondaryOAM);
    PPU ppu(ppuBus);

    Bus bus(cpuRam, cart, ppu, crl);

    NES nes(cpu, ppu, bus);

    cart.Load("rom_tests/cpu_all_official.nes");

    cpu.Reset(bus);

    int width = 256;
    int height = 240;
    int SCALE = 5;

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
    int numKeys;
    uint8_t buttonState = 0x00;

    std::vector<uint8_t> screenPixels(256*240);
    auto startTime = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> frameTime(1000.0 / 60.0);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        const bool* keystate = SDL_GetKeyboardState(&numKeys);
        buttonState |= (static_cast<uint8_t>(keystate[SDL_SCANCODE_Z])); // A
        buttonState |= (static_cast<uint8_t>(keystate[SDL_SCANCODE_X]) << 1); // A
        buttonState |= (static_cast<uint8_t>(keystate[SDL_SCANCODE_LSHIFT]) << 2); // select
        buttonState |= (static_cast<uint8_t>(keystate[SDL_SCANCODE_RSHIFT]) << 3); // start
        buttonState |= (static_cast<uint8_t>(keystate[SDL_SCANCODE_UP]) << 4); // UP button
        buttonState |= (static_cast<uint8_t>(keystate[SDL_SCANCODE_DOWN]) << 5); // DOWN button
        buttonState |= (static_cast<uint8_t>(keystate[SDL_SCANCODE_LEFT]) << 6); // LEFT button
        buttonState |= (static_cast<uint8_t>(keystate[SDL_SCANCODE_RIGHT]) << 7); // RIGHT button
        crl.UpdateLiveState(buttonState);


        while (!(ppu.scanline == 240 && ppu.dots == 0)) {
            nes.Clock();
        }
        nes.Clock();

        auto endTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(endTime - startTime); // ensuring 1 frame every 16.6 milliseconds or 60 FPS

        if (elapsedTime < frameTime) {
            std::this_thread::sleep_for(frameTime - elapsedTime);
        }

        for (size_t y = 0; y < 240; y++) {
            for (size_t x = 0; x < 256; x++) {
                screenPixels[y * 256 + x] = ppu.frameBuffer[y][x];
            }
        }
        startTime = std::chrono::steady_clock::now();

        SDL_UpdateTexture(texture, nullptr, screenPixels.data(), width * (int)sizeof(Uint8));

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        buttonState = 0x00;
        std::cout << "Test Num: " <<  std::format("{:#X}", bus.Read(0x00F1)) << std::endl;
        std::cout << "Opcode: " << std::format("{:#X}", bus.Read(0x00F2)) << std::endl;
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
