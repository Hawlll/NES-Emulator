#include "Cartridge.h"


struct PPUBus {

    Cartridge& cart;

    PPUBus(Cartridge& c) : cart(c) {}

    uint8_t PPURead(uint16_t Address) {
        if (Address <= 0x1FFF) {
            return cart.PPURead(Address);
        }

        else {
            throw std::runtime_error("Unsupported PPU address");
        }

    }

};
