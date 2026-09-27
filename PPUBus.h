#include "Cartridge.h"
#include "Memory.h"


struct PPUBus {

    Cartridge& cart;
    Memory& nametableRam;
    Memory& paletteRam;

    PPUBus(Cartridge& c, Memory& nR, Memory& pR) : cart(c), nametableRam(nR), paletteRam(pR) {}

    uint16_t PaletteAddress(uint16_t Address) {

        uint8_t offset = Address - 0x3F00;
        if (Address >= 0x3F00 && Address <= 0x3FFF) {

            if (offset == 0x10 || offset == 0x14 || offset == 0x18 || offset == 0x1C) {
                return offset - 0x10;
            }
            else {
                return offset & 0x01F;
            }
        }
        else {
            throw std::runtime_error("Unsupported palette Address");
        }
    }

    uint16_t NametableAddress(uint16_t Address) {

        if (Address >= 0x3000 && Address <= 0x3EFF) {
            Address -= 0x1000;
        }

        uint16_t offset = Address - 0x2000;

        switch (cart.nametableMirroring) {
                case 0x00: {  // horizontal ( A A B B )
                    if (offset > 0x07FF) {
                        return (offset & 0x03FF) + 0x0400;
                    }
                    else {
                        return offset & 0x03FF;
                    }
                }
                case 0x01: { // vertical (A B A B)
                    return offset & 0x07FF;
                }
                default:
                    throw std::runtime_error("Unsupported nametable mirroring");
            }


    }

    uint8_t Read(uint16_t Address) {
        if (Address <= 0x1FFF) { // cartridge CHR data
            return cart.PPURead(Address);
        }

        else if (Address <= 0x3EFF) { // nametables
            return nametableRam.Read(NametableAddress(Address));
        }

        else if (Address <= 0x3FFF) { // palettes
            return paletteRam.Read(PaletteAddress(Address));
        }

        else {
            throw std::runtime_error("Unsupported PPU Read address");
        }

    }

    void Write(uint16_t Address, uint8_t Value) {

        if (Address <= 0x1FFF) { // cartridge CHR write

        }
        else if (Address <= 0x3EFF) { // nametable write

            nametableRam.Write(NametableAddress(Address), Value);
        }

        else if (Address <= 0x3FFF) { // palette write
            paletteRam.Write(PaletteAddress(Address), Value);
        }
        else {
            throw std::runtime_error("Unsupported PPU Write address");
        }
    }


};
