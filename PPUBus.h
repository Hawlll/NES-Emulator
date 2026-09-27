#include "Cartridge.h"
#include "Memory.h"


struct PPUBus {

    Cartridge& cart;
    Memory& mem;

    PPUBus(Cartridge& c, Memory& m) : cart(c), mem(m) {}

    uint8_t Read(uint16_t Address) {
        if (Address <= 0x1FFF) { // cartridge CHR data
            return cart.PPURead(Address);
        }

        else if (Address <= 0x2FFF) { // nametables
            uint16_t offset = Address - 0x2000;

            switch (cart.nametableMirroring) {
                case 0x00: {  // horizontal ( A A B B )
                    if (offset > 0x07FF) {
                        return mem.Read((offset & 0x03FF) + 0x0400);
                    }
                    else {
                        return mem.Read(offset & 0x03FF);
                    }
                }
                case 0x01: { // vertical (A B A B)
                    return mem.Read(offset & 0x07FF);
                }
                default:
                    throw std::runtime_error("Unsupported nametable mirroring");
            }
        }

        else {
            throw std::runtime_error("Unsupported PPU Read address");
        }

    }

    void Write(uint16_t Address, uint8_t Value) {
        if (Address <= 0x2FFF) {

            uint16_t offset = Address - 0x2000;

            switch (cart.nametableMirroring) {
                case 0x00: {  // horizontal ( A A B B )
                    if (offset > 0x07FF) {
                         mem.Write((offset & 0x03FF) + 0x0400, Value);
                    }
                    else {
                        mem.Write(offset & 0x03FF, Value);
                    }
                    break;
                }
                case 0x01: { // vertical (A B A B)
                    mem.Write(offset & 0x07FF, Value);
                    break;
                }
                default:
                    throw std::runtime_error("Unsupported nametable mirroring");
            }
        }
        else {
            throw std::runtime_error("Unsupported PPU Write address");
        }
    }


};
