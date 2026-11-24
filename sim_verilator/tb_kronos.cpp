#include "Vkronos_core.h"
#include "verilated.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

static uint8_t ram[256 * 1024];

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vkronos_core* top = new Vkronos_core;

    if (argc < 2) {
        std::cerr << "Usage: tb_kronos <program.bin>" << std::endl;
        return 1;
    }

    std::ifstream bin(argv[1], std::ios::binary);
    if (!bin.is_open()) {
        std::cerr << "Failed to open " << argv[1] << std::endl;
        return 1;
    }
    bin.read(reinterpret_cast<char*>(&ram[0x10000]), sizeof(ram) - 0x10000);
    size_t bytes_read = bin.gcount();
    bin.close();
    std::cout << "Loaded " << bytes_read << " bytes into memory at 0x10000\n";

    top->clk = 0;
    top->rstz = 0;
    top->instr_ack = 0;
    top->data_ack = 0;
    top->software_interrupt = 0;
    top->timer_interrupt = 0;
    top->external_interrupt = 0;

    for (int i = 0; i < 20; i++) {
        top->clk = !top->clk;
        top->eval();
    }
    top->rstz = 1;

    uint64_t cycles = 0;
    const uint64_t MAX_CYCLES = 5000000ULL;
    bool done = false;

    bool prev_instr_req = false;
    uint32_t prev_instr_addr = 0;

    bool prev_data_req = false;
    bool prev_data_we = false;
    uint32_t prev_data_addr = 0;
    uint32_t prev_data_wdata = 0;
    uint8_t prev_data_mask = 0;

    while (!Verilated::gotFinish() && cycles < MAX_CYCLES && !done) {
        // Evaluate inputs from previous cycle
        top->instr_ack = prev_instr_req ? 1 : 0;
        if (prev_instr_req) {
            if (prev_instr_addr < sizeof(ram) - 4) {
                top->instr_data = *reinterpret_cast<uint32_t*>(&ram[prev_instr_addr]);
            } else {
                top->instr_data = 0x00000013;
            }
        } else {
            top->instr_data = 0;
        }

        top->data_ack = prev_data_req ? 1 : 0;
        if (prev_data_req) {
            if (prev_data_we) {
                if (prev_data_addr == 0x800100 || prev_data_addr == 0x10000000) {
                    char c = static_cast<char>(prev_data_wdata & 0xff);
                    std::cout << c << std::flush;
                } else if (prev_data_addr == 0x800000) {
                    std::cout << "\n[TB] Exit/Trap code written: 0x" << std::hex << prev_data_wdata << std::dec << "\n";
                    done = true;
                } else if (prev_data_addr < sizeof(ram) - 4) {
                    for (int b = 0; b < 4; b++) {
                        if (prev_data_mask & (1 << b)) {
                            ram[prev_data_addr + b] = (prev_data_wdata >> (b * 8)) & 0xff;
                        }
                    }
                }
                top->data_rd_data = 0;
            } else {
                if (prev_data_addr == 0x800018) {
                    top->data_rd_data = 0; // UART TX queue empty
                } else if (prev_data_addr < sizeof(ram) - 4) {
                    top->data_rd_data = *reinterpret_cast<uint32_t*>(&ram[prev_data_addr]);
                } else {
                    top->data_rd_data = 0;
                }
            }
        } else {
            top->data_rd_data = 0;
        }

        // Posedge clock
        top->clk = 1;
        top->eval();

        // Sample outputs for next cycle
        prev_instr_req = (top->instr_req != 0);
        prev_instr_addr = top->instr_addr;

        if (top->data_req && top->data_wr_en && top->data_addr >= 0x800000) std::cout << "[IO WRITE] addr: 0x" << std::hex << top->data_addr << " val: 0x" << top->data_wr_data << std::dec << "
"; prev_data_req = (top->data_req != 0);
        prev_data_we = (top->data_wr_en != 0);
        prev_data_addr = top->data_addr;
        prev_data_wdata = top->data_wr_data;
        prev_data_mask = top->data_mask;

        // Negedge clock
        top->clk = 0;
        top->eval();
        cycles++;

        if (cycles % 500000 == 0) {
            std::cout << "[TB] Cycle " << cycles << "...\n" << std::flush;
        }
    }

    std::cout << "\nKronos cycle-by-cycle simulation finished at " << cycles << " cycles\n";
    delete top;
    return 0;
}
