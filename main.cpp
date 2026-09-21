#include "6502.h"
#include "Bus.h"
#include "Memory.h"
#include <iostream>
#include "Cartridge.h"

int main()
{
    CPU cpu;
    Memory ram(64);
    Video gpu;
    Cartridge cart;
    Bus bus(ram, gpu, cart);

    cart.Load("rom_tests/nes_mapper0_test.nes");

    cpu.Reset(bus);

    std::cout << "Mapper: " << static_cast<int>(cart.mapper) << '\n';
    std::cout << "PRG size: " << cart.prgROM.size() << '\n';
    std::cout << "CHR size: " << cart.chrROM.size() << '\n';

    std::cout << std::hex;

    std::cout << "First opcode: "
              << static_cast<int>(cart.CPURead(0x8000)) << '\n';

    std::cout << "Reset low: "
              << static_cast<int>(cart.CPURead(0xFFFC)) << '\n';

    std::cout << "Reset high: "
              << static_cast<int>(cart.CPURead(0xFFFD)) << '\n';







    //inline program - draw diagonal line
    /*loop

      // Initialize A, X, Y to 0
      LDA 0x00
      LDX 0x00
      LDY 0x00

       write to gpu mapped memory
      PHA
      TAX
      LDA 0x01
      STA 0x00 0xE0
      PLA

      CMP
      BEQ

      LDX 0x00
      JSR
        INX
        CPX 0x0F
        BEQ
        JMP
      RTS
      STX
      ADC
      STY
      ADC
      JMP


    */
//
//    bus.Write(0x8000, 0xA9); // LDA
//    bus.Write(0x8001, 0x00);
//
//    bus.Write(0x8002, 0xA2); // LDX
//    bus.Write(0x8003, 0x00);
//
//    bus.Write(0x8004, 0xA0); // LDY
//    bus.Write(0x8005, 0x01);
//
//    bus.Write(0x8006, 0x48); // PHA
//    bus.Write(0x8007, 0xAA); // TAX
//    bus.Write(0x8008, 0xA9); // LDA
//    bus.Write(0x8009, 0x01);
//    bus.Write(0x800A, 0x9D); // STA
//    bus.Write(0x800B, 0x00);
//    bus.Write(0x800C, 0xE0);
//
//    bus.Write(0x800D, 0x68); // PLA
//
//    bus.Write(0x800E, 0xC9); // CMP
//    bus.Write(0x800F, 0xFF);
//    bus.Write(0x8010, 0xF0); // BEQ
//    bus.Write(0x8011, 0x0F);
//
//    bus.Write(0x8012, 0xA2); // LDX
//    bus.Write(0x8013, 0x00);
//
//    bus.Write(0x8014, 0x20); // JSR
//    bus.Write(0x8015, 0x00);
//    bus.Write(0x8016, 0x81);
//
//
//    bus.Write(0x8100, 0xE8); // INX
//    bus.Write(0x8101, 0xE0); // CPX
//    bus.Write(0x8102, 0x0F);
//    bus.Write(0x8103, 0xF0); // BEQ
//    bus.Write(0x8104, 0x03);
//    bus.Write(0x8105, 0x4C); // JMP
//    bus.Write(0x8106, 0x00);
//    bus.Write(0x8107, 0x81);
//    bus.Write(0x8108, 0x60); // RTS
//
//    bus.Write(0x8017, 0x8E); // STX
//    bus.Write(0x8018, 0x00);
//    bus.Write(0x8019, 0x90);
//    bus.Write(0x801A, 0x6D); // ADC
//    bus.Write(0x801B, 0x00);
//    bus.Write(0x801C, 0x90);
//    bus.Write(0x801D, 0x8C); // STY
//    bus.Write(0x801E, 0x00);
//    bus.Write(0x801F, 0x90);
//    bus.Write(0x8020, 0x6D); // ADC
//    bus.Write(0x8021, 0x00);
//    bus.Write(0x8022, 0x90);
//    bus.Write(0x8023, 0x4C); // JMP
//    bus.Write(0x8024, 0x06);
//    bus.Write(0x8025, 0x80);
//
//
//
//    int num_clocks = 0;
//
//    while (true) {
//
//        cpu.Clock(bus);
//        num_clocks++;
//
//        std::cout << "\033[2J\033[H";
//        std::cout << "Num Clocks: " << num_clocks <<  std::endl;
//        for (int x = 0; x < 16; x++) {
//            for (int y = 0; y < 16; y++) {
//                uint8_t val_in_vram = bus.Read(0xE000 + (x*16) + y);
//                if (val_in_vram == 0x01) {
//                    std::cout << "#";
//                }
//                else {
//                    std::cout << ".";
//                }
//            }
//            std::cout << std::endl;
//        }
//    }

    return 0;
}
