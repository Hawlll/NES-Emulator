#pragma once
#include <iostream>
#include <fstream>


struct Cartridge {

    // Mapping 0
    // 0x6000-0x7FFF optional cartridge ram
    // 0x8000 - 0xBFFF 16KiB PRG ROM bank
    // 0xC000 - 0xFFFF 16KiB bank

    std::vector<uint8_t> prgROM; // program read only memory (program code)
    std::vector<uint8_t> chrROM; // character/tile read only memory (graphics data)
    uint8_t mapper;


    Cartridge() : prgROM{}, chrROM{}, mapper{0x00} {};

    uint8_t CPURead(uint16_t Address) {

        switch (mapper) {
            case 0x00: { // Mapper 0
                if (Address < 0x8000) {
                    throw std::runtime_error("Outside cartridge address range");
                }

                uint16_t offset = Address - 0x8000;
                uint16_t prgIndex;


                if (prgROM.size() == 16384) {
                    prgIndex = offset & 0x3FFF; // mirrors after 16KiB
                }
                else if (prgROM.size() == 32768) {
                    prgIndex = offset;
                }
                else {
                    throw std::runtime_error("Invalid PRGROM size for Mapper 0");
                }

                return prgROM[prgIndex];
                break;

            }

            default:
                throw std::runtime_error("Unsupported Mapper");
                break;
        }
    }

    bool Load(const std::string& filepath) {

        std::ifstream file(filepath, std::ios::binary); // open file in binary mode

        if (!file) {
            throw std::runtime_error("Could not read cartridge");
        }

        uint8_t header[16]; // header of file
        // Bytes 0-3: magic number. INES format would be 0x4E(N) 0x45(E) 0x53(S) followed by EOF 0x1A
        // Bytes 4: PRG-ROM size in KiB units
        // Bytes 5: CHR-ROM size in KiB units
        // Bytes 6: low part of Mapper byte, trainer flag
        // Bytes 7: high part of Mapper byte


        if (!file.read(reinterpret_cast<char*>(header), 16)) {
            throw std::runtime_error("Could not read header of cartridge");
        }

        if (!(header[0] == 0x4E && header[1] == 0x45 && header[2] == 0x53 && header[3] == 0x1A)) {
            throw std::runtime_error("ROM not in INES format");
        }

        uint8_t lowMap = (0xF0 & header[6]) >> 4;
        uint8_t highMap = 0xF0 & header[7];

        mapper = highMap | lowMap;

        size_t prgSize = header[4] * 16384;
        size_t chrSize = (int)header[5] * 8192;

        prgROM.resize(prgSize);
        chrROM.resize(chrSize);

        if (header[6] & 0x04) { // if has trainer
            file.seekg(512, std::ios::cur);
        }


        if (prgSize > 0 && !file.read(reinterpret_cast<char*>(prgROM.data()), (int)prgSize)) { // read program data
            throw std::runtime_error("Failed to read PRGROM");
        }

        if (chrSize > 0 && !file.read(reinterpret_cast<char*>(chrROM.data()), (int)chrSize)) { // read graphics data
            throw std::runtime_error("Failed to read CHRROM");
        }



        return true;
    }
};
