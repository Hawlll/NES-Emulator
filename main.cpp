#include "6502.h"
#include "Bus.h"
#include <iostream>

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

    ppu.PrintTile(ppu.DecodeTile(0x0010)); // tile 1
    ppu.PrintTile(ppu.DecodeTile(0x0020)); // tile 2
    ppu.PrintTile(ppu.DecodeTile(0x0030)); // tile 3

    return 0;
}
