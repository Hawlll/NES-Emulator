#include "Bus.h"
#include <iostream>

struct CPU { // emulated after 6502. 8 bit data, 16 bit memory address space. little endian multi-byte ordering (low => High)
    uint16_t PC; // Program Counter
    uint8_t SP; // Stack pointer, just an offset of page 1
    uint8_t A; // Accumulator register
    uint8_t X; // Index X register
    uint8_t Y; // Index Y register
    uint8_t status; // Processor status register
    uint8_t cycles; // emulator representation, not programmatically visible, used to track cycles for instruction
    uint8_t instruction_latch; // emulator representation, not programmatically visible, used to store current opcode
    uint16_t address_latch; // emulator representation, not programmatically visible, used to store constructed address
    uint8_t N_FLAG; // negative flag
    uint8_t Z_FLAG; // zero flag
    uint8_t C_FLAG; // carry flag (unsigned over/under flow)
    uint8_t V_FLAG; // overflow flag (signed over/under flow)
    uint8_t B_FLAG; // // break flag
    uint8_t I_FLAG; // interrupt disabled flag


    void Reset(Bus& bus) { // reset vector

        B_FLAG = 0b00010000;
        V_FLAG = 0b01000000;
        C_FLAG = 0b00000001;
        Z_FLAG = 0b00000010;
        N_FLAG = 0b10000000;
        I_FLAG = 0b00000100;

        uint8_t low_byte = bus.Read(0xFFFC);
        uint8_t high_byte = bus.Read(0xFFFD);

        PC = (high_byte << 8) + low_byte;
        A = 0x00;
        X = 0x00;
        Y = 0x00;
        status = I_FLAG;
        instruction_latch = 0x00;
        cycles = 0x00;
        address_latch = 0x0000;
        SP = 0xFD;

    }

    void Clock(Bus& bus) {
        if (cycles > 0) { // we are performing instruction
            Execute(bus);
        }
        else {
            uint8_t opcode = Fetch(PC, bus);
            cycles = Decode(opcode, bus);
            instruction_latch = opcode;
            PC++;
        }
        cycles--;
    }

    uint8_t Fetch(uint16_t Address, Bus& bus) {
        uint8_t value = bus.Read(Address);
        return value;
    }

    uint8_t Decode(uint8_t opcode, Bus& bus) { // defined opcodes, return cycles
        switch (opcode) {
            case 0xA9: // LDA (Load into Accumulator register) - take immediate 1 byte after opcode and place into accumulator register
                return 2;
                break;
            case 0xA5: // LDA (Zero Page addressing mode) (Load into Accumulator register) - take immediate, which is offset of zero page, load value from address and put into accumluator
                return 3;
                break;
            case 0xB5: // LDA (Zero Page index addressing mode) (Load into Accummulator Register) - take immediate, which is offset of zero page, add the value from x register, load the value from that address and put into accummulator
                return 4;
                break;
            case 0xBD: { // LDA (Absolute index X addressing mode) (Load into Accumulator register) - take value from sum of (absolute address formed by next two bytes) + (X register) and put into accumulator. grants another cycle if page cross
                uint8_t low_byte = bus.Read(PC+1);
                uint8_t result = low_byte + X;
                if (result >= low_byte) {
                    return 4;
                }
                else{
                    return 5;
                }
                break;
            }
            case 0xA2: // LDX(Load into X register) - take immediate value and load into x register
                return 2;
                break;
            case 0xA0: // LDY (load immediate into Y register) - take immediate value and load into y register
                return 2;
                break;
            case 0x8D: // STA (Store Accumulator) - take value from accumulator and store into 16 bit address formed with next two bytes
                return 4;
                break;
            case 0x85: // STA (Zero Page addressing mode) (Store Accumulator) - take value from accumulator and store into address formed by immediate offset of zero page
                return 3;
                break;
            case 0x95: // STA (Zero Page index X addressing mode) (Store Accumulator) - take value from accumulator and store into address formed by the sum of the immediate offset of zero page and X register
                return 4;
                break;
            case 0x9D: { // STA (Absolute index X addressing mode) (Store Accumulator) - take value from accumulator and store into address formed by the next two bytes added to the X register
                return 5;
                break;
            }
            case 0x8E: // STX (Store X register) - take value from X register and store into 16 bit address formed with next two bytes
                return 4;
                break;
            case 0x8C: // STY (Store Y register) - take value from Y register and store into 16 bit address formed with next two bytes
                return 4;
                break;
            case 0x69: // ADC (Add with carry) - take immediate value, carry, and add into accumulator register
                return 2;
                break;
            case 0x6D: // ADC (Absolute Addressing mode) - add value from address formed by next two bytes with carry into accumulator
                return 4;
                break;
            case 0xE9: // SBC  (Subtract with carry) take immediate value, inverse carry, and subtract from accumulator register
                return 2;
                break;
            case 0x18: // CLC (Clear Carry) - Clears the carry flag in status register
                return 2;
                break;
            case 0x38: // SEC (Set Carry) - Sets the carry flag in status register
                return 2;
                break;
            case 0xC9: // CMP (Compare Accumulator) - compare accumulator value with immediate
                return 2;
                break;
            case 0xE0: // CPX (Compare X) - compare X value with immediate
                return 2;
                break;
            case 0xE8: // INX (Increment X) - Add one to the X register
                return 2;
                break;
            case 0xC8: // INY (Increment Y) - Add one to the Y register
                return 2;
                break;
            case 0xCA: // DEX (Decrement X) - Subtract one from the X register
                return 2;
                break;
            case 0x88: // DEY (Decrement Y) - Subtract one from the Y register
                return 2;
                break;
            case 0x4C: // JMP (Jump) - Update PC to address formed by next two bytes
                return 3;
                break;
            case 0x20: // JSR (Jump to Sub routine) - Push PC to stack and set PC to address formed by next two bytes
                return 6;
                break;
            case 0x60: // RTS (Return from subroutine) - pull 16 bit address from stack and set PC to it
                return 6;
                break;
            case 0xF0: // BEQ (Branch if equal) - Increment PC by immediate signed offset if zero flag is on
                if (status & Z_FLAG) {
                    return 3;
                }
                else {
                    return 2;
                }
                break;
            case 0xD0: // BNE (Branch if not equal) - Increment PC by immediate signed offset if zero flag is clear
                if ((status & Z_FLAG) < 1) {
                    return 3;
                }
                else {
                    return 2;
                }
                break;
            case 0xAA: // TAX (Transfer A to X) - Load the value in the accumualtor into the X register
                return 2;
                break;
            case 0x48: // PHA (Push A) - Push accumulator value to stack
                return 3;
                break;
            case 0x68: // PLA (Pull A) - Pulls value from stack into the accumulator
                return 4;
                break;
            case 0x08:  // PHP (Push Processor Status) - Push processor status to stack
                return 3;
                break;
            case 0x28: // PLP (Pull Processor Status) - Pull processor status from stack and load into status register
                return 4;
                break;
            case 0x00: // BRK (Break) - push return address to stack, push processor status to stack, point PC to 0xFFFE (interrupt handler)
                return 7;
                break;
            case 0x40: // RTI (Return from Interrupt) - pull return address from stack and set to PC, pull status from stack
                return 6;
                break;
            case 0x58: // CLI (Clear interrupt flag) - clears interrupt flag
                return 2;
                break;
            case 0x78: // SEI (Set interrupt flag) - sets interrupt flag
                return 2;
                break;
            default:
                throw std::runtime_error("Instruction does not exist: " + std::format("{:#X}\n", (int)opcode));
                break;

        }
    }

    void Execute(Bus& bus) {

        switch (instruction_latch) {

            case 0xA9: // LDA (Load into Accumulator register) - take immediate 1 byte after opcode and place into accumulator register

                switch (cycles) {
                    case 1:
                        A = Fetch(PC, bus);
                        SetZFLAG(A);
                        SetNFLAG(A);
                        PC++;
                        break;
                    default:
                        break;
                }
                break;

            case 0xA5: // LDA (Zero Page addressing mode) (Load into Accumulator register) - take immediate, which is offset of zero page, load value from address and put into accumluator

                switch (cycles) {
                    case 2:
                        address_latch = 0x0000 + Fetch(PC, bus);
                        PC++;
                        break;
                    case 1:
                        A = bus.Read(address_latch);
                        SetZFLAG(A);
                        SetNFLAG(A);
                        break;
                    default:
                        break;
                }
                break;

            case 0xB5: // LDA (Zero Page index X addressing mode) (Load into Accummulator Register) - take immediate, which is offset of zero page, add the value from x register, load the value from that address and put into accummulator

                switch (cycles) {
                    case 3:
                        address_latch = 0x0000 + Fetch(PC, bus);
                        PC++;
                        break;
                    case 2: {
                        uint8_t low_byte = (address_latch & 0x00FF) + X;
                        address_latch  = 0x0000 + low_byte;
                        break;
                    }
                    case 1:
                        A = bus.Read(address_latch);
                        SetZFLAG(A);
                        SetNFLAG(A);
                        break;
                    default:
                        break;
                }
                break;


            case 0xBD: // LDA (Absolute index X addressing mode) (Load into Accumulator register) - take value from sum of (absolute address formed by next two bytes) + (X register) and put into accumulator. grants another cycle if page cross

                switch (cycles) {
                    case 4:
                        break; // extra cycle due to page cross
                    case 3:
                        address_latch = 0x0000 + Fetch(PC, bus) + X;
                        PC++;
                        break;
                    case 2:
                        address_latch = (Fetch(PC, bus) << 8) + address_latch;
                        PC++;
                        break;
                    case 1:
                        A = bus.Read(address_latch);
                        SetZFLAG(A);
                        SetNFLAG(A);
                        break;
                    default:
                        break;
                }
                break;

            case 0xA2: // LDX(Load into X register) - take immediate value and load into x register

                switch (cycles) {
                    case 1:
                        X = Fetch(PC, bus);
                        SetZFLAG(X);
                        SetNFLAG(X);
                        PC++;
                        break;
                    default:
                        break;
                }
                break;


            case 0xA0: // LDY (load immediate into Y register) - take immediate value and load into y register

                switch (cycles) {
                    case 1:
                        Y = Fetch(PC, bus);
                        SetZFLAG(Y);
                        SetNFLAG(Y);
                        PC++;
                        break;
                    default:
                        break;
                }
                break;


            case 0x8D: { // STA (Store Accumulator) - take value from accumulator and store into 16 bit address formed with next two bytes

                switch (cycles) {
                    case 3:
                        address_latch = Fetch(PC, bus); // low byte
                        PC++;
                        break;
                    case 2:
                        address_latch |= Fetch(PC, bus) << 8; // high byte
                        PC++;
                        break;
                    case 1:
                        bus.Write(address_latch, A);
                        break;
                    default:
                        break;
                }
                break;

            }

            case 0x85: // STA (Zero Page addressing mode) (Store Accumulator) - take value from accumulator and store into address formed by immediate offset of zero page

                switch (cycles) {
                    case 2:
                        address_latch = 0x0000 + Fetch(PC, bus);
                        PC++;
                        break;
                    case 1:
                        bus.Write(address_latch, A);
                        break;
                    default:
                        break;
                }
                break;

            case 0x95: // STA (Zero Page index X addressing mode) (Store Accumulator) - take value from accumulator and store into address formed by the sum of the immediate offset of zero page and X register

                switch (cycles) {
                    case 3:
                        address_latch = 0x0000 + Fetch(PC, bus);
                        PC++;
                        break;
                    case 2: {
                        uint8_t low_byte = (address_latch & 0x00FF) + X;
                        address_latch = 0x0000 + low_byte;
                        break;
                    }
                    case 1:
                        bus.Write(address_latch, A);
                        break;
                    default:
                        break;
                }
                break;

            case 0x9D: // STA (Absolute index X addressing mode) (Store Accumulator) - take value from accumulator and store into address formed by the next two bytes added to the X register

                switch (cycles) {
                    case 4:
                        address_latch = 0x0000 + Fetch(PC, bus) + X;
                        PC++;
                        break;
                    case 3:
                        address_latch = (Fetch(PC, bus) << 8) + address_latch;
                        PC++;
                        break;
                    case 2:
                        break;
                    case 1:
                        bus.Write(address_latch, A);
                        break;
                    default:
                        break;
                }
                break;

            case 0x8E: // STX (Store X register) - take value from X register and store into 16 bit address formed with next two bytes

                switch (cycles) {
                    case 3:
                        address_latch = 0x0000 + Fetch(PC, bus);
                        PC++;
                        break;
                    case 2:
                        address_latch |= (Fetch(PC, bus) << 8);
                        PC++;
                        break;
                    case 1:
                        bus.Write(address_latch, X);
                        break;
                    default:
                        break;
                }
                break;

            case 0x8C: // STY (Store Y register) - take value from Y register and store into 16 bit address formed with next two bytes

                switch (cycles) {
                    case 3:
                        address_latch = 0x0000 + Fetch(PC, bus);
                        PC++;
                        break;
                    case 2:
                        address_latch |= (Fetch(PC, bus) << 8);
                        PC++;
                        break;
                    case 1:
                        bus.Write(address_latch, Y);
                        break;
                    default:
                        break;
                }
                break;


            case 0x69: { // ADC (Add with carry) - take immediate value, carry, and add into accumulator register

                switch (cycles) {
                    case 1: {
                        uint8_t immediate = Fetch(PC, bus);

                        uint8_t carry = (status & C_FLAG) > 0;

                        uint16_t operation_result = A + immediate + carry;
                        uint8_t A_prev = A;

                        A = operation_result & 0xFF;

                        SetNFLAG(A);
                        SetZFLAG(A);
                        SetCFLAG(operation_result);
                        SetVFLAG(immediate, A_prev, A);
                        PC++;
                        break;
                    }
                    default:
                        break;
                }
                break;

            }


            case 0x6D: // ADC (Absolute Addressing mode) - add value from address formed by next two bytes with carry into accumulator

                switch (cycles) {
                    case 3:
                        address_latch = 0x0000 +  Fetch(PC, bus);
                        PC++;
                        break;
                    case 2:
                        address_latch |= (Fetch(PC, bus) << 8);
                        PC++;
                        break;
                    case 1: {
                        uint8_t carry = (status & C_FLAG) > 0;
                        uint16_t operation_result = A + bus.Read(address_latch) + carry;
                        uint8_t A_prev = A;

                        A = operation_result & 0x00FF;

                        SetNFLAG(A);
                        SetZFLAG(A);
                        SetCFLAG(operation_result);
                        SetVFLAG(bus.Read(address_latch), A_prev, A);
                        break;
                    }
                    default:
                        break;
                }
                break;

            case 0xE9: // SBC (Subtract with Carry) - take immediate value, NOT carry, and subtract from accumulator register
                       // It is NOT carry because its the flag is intrepreted as its inverse for substraction (C = 1 means no borrow, C = 0 means borrow happened)
                       // That is why usually SEC is followed before SBC
                switch (cycles) {
                    case 1: {
                        uint8_t imm = Fetch(PC, bus);
                        uint8_t carry = status & C_FLAG;
                        uint8_t A_prev = A;

                        A = A - imm - !carry;

                        SetNFLAG(A);
                        SetZFLAG(A);
                        SetCFLAG( A_prev >= (imm + !carry) );
                        SetVFLAG(((A_prev ^ A) & (A_prev ^ imm) & 0x80) != 0);
                        PC++;
                        break;

                    }
                    default:
                        break;
                }
                break;
            case 0x18: // CLC (Clear Carry) - Clears the carry flag in status register

                switch (cycles) {
                    case 1:
                        SetCFLAG(false);
                        break;
                    default:
                        break;
                }
                break;

            case 0x38: // SEC (Set Carry) - Sets the carry flag in status register

                switch (cycles) {
                    case 1:
                        SetCFLAG(true);
                        break;
                    default:
                        break;
                }
                break;


            case 0xC9: // CMP (Compare Accumulator) - compare accumulator value with immediate

                switch (cycles) {
                    case 1: {
                        uint8_t imm = Fetch(PC, bus);
                        uint8_t result = A - imm;

                        SetCFLAG(A >= imm);
                        SetNFLAG(result);
                        SetZFLAG(result);
                        PC++;
                        break;
                    }
                    default:
                        break;

                }
                break;

            case 0xE0: // CPX (Compare X) - compare X value with immediate

                switch (cycles) {
                    case 1: {
                        uint8_t imm = Fetch(PC, bus);
                        uint8_t result = X - imm;

                        SetCFLAG(X >= imm);
                        SetNFLAG(result);
                        SetZFLAG(result);
                        PC++;
                        break;
                    }
                    default:
                        break;

                }
                break;


            case 0xE8: // INX (Increment X) - Add one to the X register

                switch (cycles) {
                    case 1:
                        X += 1;
                        SetZFLAG(X);
                        SetNFLAG(X);
                        break;
                    default:
                        break;
                }
                break;

            case 0xC8: // INY (Increment Y) - Add one to the Y register

                switch (cycles) {
                    case 1:
                        Y += 1;
                        SetZFLAG(Y);
                        SetNFLAG(Y);
                        break;
                    default:
                        break;
                }
                break;

            case 0xCA: // DEX (Decrement X) - Subtract one from the X register

                switch (cycles) {
                    case 1:
                        X -= 1;
                        SetZFLAG(X);
                        SetNFLAG(X);
                        break;
                    default:
                        break;
                }
                break;

            case 0x88: // DEY (Decrement Y) - Subtract one from the Y register

                switch (cycles) {
                    case 1:
                        Y -= 1;
                        SetZFLAG(Y);
                        SetNFLAG(Y);
                        break;
                    default:
                        break;
                }
                break;


            case 0x4C: // JMP (Jump) - Update PC to address formed by next two bytes

                switch (cycles) {
                    case 2:
                        address_latch = Fetch(PC, bus);
                        PC++;
                        break;
                    case 1:
                        address_latch |= (Fetch(PC, bus) << 8);
                        PC = address_latch;
                        break;
                    default:
                        break;
                }
                break;

            case 0x20: // JSR (Jump to Sub routine) - Push PC to stack and set PC to address formed by next two bytes

                switch (cycles) {
                    case 5: {
                        uint16_t result_addr = PC + 1;
                        Push(result_addr >> 8, bus);
                        Push(result_addr & 0x00FF, bus);
                        break;
                    }
                    case 4:
                        address_latch = Fetch(PC, bus);
                        PC++;
                        break;
                    case 3:
                        address_latch |= (Fetch(PC, bus) << 8);
                        PC++;
                        break;
                    case 2:
                        PC = address_latch;
                        break;
                    case 1:
                        break;
                    default:
                        break;
                }
                break;

            case 0x60: // RTS (Return from subroutine) - pull 16 bit address from stack and set PC to it

                switch (cycles) {

                    case 5:
                        address_latch = Pull(bus);
                        break;
                    case 4:
                        address_latch |= (Pull(bus) & 0x00FF) << 8;
                        break;
                    case 3:
                        PC = address_latch;
                        PC++;
                        break;
                    case 2:
                        break;
                    case 1:
                        break;
                    default:
                        break;
                }
                break;


            case 0xF0: // BEQ (Branch if equal) - Increment PC by immediate signed offset if zero flag is on

                switch (cycles) {
                    case 2: {
                        uint8_t imm = Fetch(PC, bus);
                        uint8_t sign = imm & (1 << 7);
                        uint8_t magnitude = ~imm + 1;

                        if (sign) {
                            address_latch = (PC+1) - magnitude;
                        }
                        else {
                            address_latch = (PC+1) + imm;
                        }
                        PC++;
                        break;
                    }
                    case 1:
                        if (status & Z_FLAG) {
                            PC = address_latch;
                        }
                        else {
                            PC++;
                        }
                        break;
                    default:
                        break;
                }
                break;

            case 0xD0: // BNE (Branch if not equal) - Increment PC by immediate signed offset if zero flag is clear

                switch (cycles) {
                    case 2: {
                        uint8_t imm = Fetch(PC, bus);
                        uint8_t sign = imm & (1 << 7);
                        uint8_t magnitude = ~imm + 1;

                        if (sign) {
                            address_latch = (PC+1) - magnitude;
                        }
                        else {
                            address_latch = (PC+1) + imm;
                        }
                        PC++;
                        break;
                    }
                    case 1:
                        if ((status & Z_FLAG) < 1) {
                            PC = address_latch;
                        }
                        else {
                            PC++;
                        }
                        break;
                    default:
                        break;
                }
                break;


            case 0xAA: // TAX (Transfer A to X) - Load the value in the accumualtor into the X register

                switch (cycles) {
                    case 1:
                        X = A;
                        SetZFLAG(X);
                        SetNFLAG(X);
                        break;
                    default:
                        break;
                }
                break;

            case 0x48: // PHA (Push A) - Push accumulator value to stack

                switch (cycles) {
                    case 2:
                        break;
                    case 1:
                        Push(A, bus); // push happens on third cycle
                        break;
                    default:
                        break;
                }
                break;

            case 0x68: // PLA (Pull A) - Loads top of stack into the accumulator

                switch (cycles) {
                    case 3:
                        break;
                    case 2:
                        break;
                    case 1:
                        A = Pull(bus); // pull happens on 4th cycle
                        SetZFLAG(A);
                        SetNFLAG(A);
                        break;
                    default:
                        break;
                }
                break;

            case 0x08: // PHP (Push Processor Status) - Push processor status to stack

                switch (cycles) {
                    case 2:
                        break;
                    case 1:
                        Push(status | 0b00110000, bus);
                        break;
                    default:
                        break;
                }
                break;

            case 0x28: // PLP (Pull Processor Status) - Pull processor status from stack and load into status register

                switch (cycles) {
                    case 3:
                        break;
                    case 2:
                        break;
                    case 1:
                        status = Pull(bus) & 0b11001111; // ignore break and extra bit
                        break;
                    default:
                        break;
                }
                break;

            case 0x00: // BRK (Break) - push return address to stack, push processor status to stack, point PC to 0xFFFE (interrupt handler)

                switch (cycles) {
                    case 6:
                        PC++;
                        break;
                    case 5:
                        Push((PC & 0xFF00) >> 8, bus);
                        break;
                    case 4:
                        Push(PC & 0x00FF, bus);
                        break;
                    case 3:
                        Push(status | 0b00110000, bus);
                        SetIFLAG(true);
                        break;
                    case 2:
                        address_latch = 0x0000 + bus.Read(0xFFFE);
                        break;
                    case 1:
                        address_latch |= bus.Read(0xFFFF) << 8;
                        PC = address_latch;
                        break;
                    default:
                        break;
                }
                break;

            case 0x40: // RTI (Return from Interrupt) - pull return address from stack and set to PC, pull status from stack

                switch (cycles) {
                    case 5:
                        status = Pull(bus) & 0b11001111;
                        break;
                    case 4:
                        address_latch = 0x0000 + Pull(bus);
                        break;
                    case 3:
                        address_latch |= Pull(bus) << 8;
                        break;
                    case 2:
                        PC = address_latch;
                        break;
                    case 1:
                        break;
                    default:
                        break;
                }
                break;

            case 0x58: // CLI (Clear interrupt flag) - clears interrupt flag

                switch (cycles) {
                    case 1:
                        SetIFLAG(false);
                        break;
                    default:
                        break;
                }
                break;


            case 0x78: // SEI (Set interrupt flag) - sets interrupt flag

                switch (cycles) {
                    case 1:
                        SetIFLAG(true);
                        break;
                    default:
                        break;
                }
                break;

            default:
                throw std::runtime_error("Instruction does not exist: " + std::format("{:#X}\n", (int)instruction_latch));
                break;
        }
    }

    void Push(uint8_t data, Bus& bus) {
        bus.Write(0x0100 + SP, data);
        SP--;
    }

    uint8_t Pull(Bus& bus) {
        SP++;
        return bus.Read(0x0100 + SP);
    }

    void IRQ(Bus& bus) {
        if ((status & I_FLAG) == 0) {
            Push((PC & 0xFF00) >> 8, bus);
            Push(PC & 0x00FF, bus);
            Push(status & 0b11101111, bus);
            SetIFLAG(true);
            address_latch = 0x0000 + bus.Read(0xFFFE);
            address_latch |= bus.Read(0xFFFF) >> 8;
            PC = address_latch;
        }
    }

    void NMI(Bus& bus) {
        Push((PC & 0xFF00) >> 8, bus);
        Push(PC & 0x00FF, bus);
        Push(status & 0b11101111, bus);
        SetIFLAG(true);
        address_latch = 0x0000 + bus.Read(0xFFFA);
        address_latch |= bus.Read(0xFFFB) >> 8;
        PC = address_latch;
    }

    void SetZFLAG(uint8_t reg) {
        if (reg == 0) {
            status |= Z_FLAG;
        }
        else {
            status &= ~Z_FLAG;
        }
    }


    void SetNFLAG(uint8_t reg) {
        if ((reg & (1 << 7)) > 0) {
            status |= N_FLAG;
        }
        else {
            status &= ~N_FLAG;
        }
    }

    void SetCFLAG(uint16_t operation_result) {
        if ((operation_result & 0x100) > 0) {
            status |= C_FLAG;
        }
        else {
            status &= ~C_FLAG;
        }
    }

    void SetCFLAG(bool value) {
        if (value) {
            status |= C_FLAG;
        }
        else {
            status &= ~C_FLAG;
        }
    }

    void SetVFLAG(bool value) {
        if (value) {
            status |= V_FLAG;
        }
        else {
            status &= ~V_FLAG;
        }
    }

    void SetVFLAG(uint8_t operand_1, uint8_t operand_2, uint8_t reg) {
        uint8_t operand_1_sign = (operand_1 & (1 << 7)) > 0;
        uint8_t operand_2_sign = (operand_2 & (1 << 7)) > 0;
        uint8_t reg_sign = (reg & (1 << 7)) > 0;

        if ((operand_1_sign == operand_2_sign) && (operand_1_sign != reg_sign)) {
            status |= V_FLAG;
        }
        else {
            status &= ~V_FLAG;
        }
    }

    void SetBFLAG(bool value) {
        if (value) {
            status |= B_FLAG;
        }
        else {
            status &= ~B_FLAG;
        }
    }

    void SetIFLAG(bool value) {
        if (value) {
            status |= I_FLAG;
        }
        else {
            status &= ~I_FLAG;
        }
    }
};
