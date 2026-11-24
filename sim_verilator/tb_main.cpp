#include "Vtb_kronos.h"
#include "Vtb_kronos___024root.h"
#include "verilated.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vtb_kronos* top = new Vtb_kronos;

    if (argc < 2) {
        std::cerr << "Usage: tb_main <program.bin>" << std::endl;
        return 1;
    }

    std::ifstream bin(argv[1], std::ios::binary);
    if (!bin.is_open()) {
        std::cerr << "Failed to open " << argv[1] << std::endl;
        return 1;
    }

    // Read binary into memory buffer
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(bin)), std::istreambuf_iterator<char>());
    bin.close();
    std::cout << "Loaded " << buffer.size() << " bytes into memory at 0x10000\n";

    // Initialize root mem array (word address 0x10000 >> 2 = 0x4000)
    size_t base_word = 0x10000 / 4;
    for (size_t i = 0; i < buffer.size(); i += 4) {
        uint32_t word = 0;
        for (int b = 0; b < 4 && (i + b) < buffer.size(); b++) {
            word |= static_cast<uint32_t>(buffer[i + b]) << (b * 8);
        }
        top->rootp->tb_kronos__DOT__mem[base_word + (i / 4)] = word;
    }

    // Reset sequence
    top->clk = 0;
    top->rstz = 0;
    for (int i = 0; i < 20; i++) {
        top->clk = !top->clk;
        top->eval();
    }
    top->rstz = 1;

    uint64_t cycles = 0;
    const uint64_t MAX_CYCLES = 5000000ULL;

    while (!Verilated::gotFinish() && cycles < MAX_CYCLES) {
        top->clk = 1;
        top->eval();

        if (top->uart_valid) {
            std::cout << static_cast<char>(top->uart_char) << std::flush;
        }

        if (top->exit_valid) {
            std::cout << "\n[TB] Exit code: 0x" << std::hex << top->exit_code << std::dec << "\n";
            break;
        }

        top->clk = 0;
        top->eval();
        cycles++;

        if (cycles % 1000000 == 0) {
            std::cout << "[TB] Cycle " << cycles << " (PC=0x" << std::hex << top->pc_out << std::dec << ")...\n" << std::flush;
        }
    }

    std::cout << "\nKronos cycle-by-cycle simulation finished at " << cycles << " cycles\n";
    delete top;
    return 0;
}

