#include "Bus.h"
#include "PPU.h"
#include "6502.h"

struct NES {

    PPU& ppu;
    CPU& cpu;
    Bus& bus;
    uint64_t cycles = 0;


    NES (CPU& c, PPU& p, Bus& b) : ppu(p), cpu(c), bus(b) {}

    void Clock() {

        ppu.Clock();

        if (ppu.send_NMI == true) {
            ppu.send_NMI = false;
            cpu.nmi_pending = true;
        }

        if (cycles % 3 == 0) {
            cpu.Clock(bus);
        }

        cycles ++;

    }

};
