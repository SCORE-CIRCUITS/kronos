// Copyright (c) 2026 SCORE Benchmark Suite
// Cycle-accurate SystemVerilog testbench for Kronos RV32I core

module tb_kronos (
  input  logic        clk,
  input  logic        rstz,
  output logic [31:0] pc_out,
  output logic [7:0]  uart_char,
  output logic        uart_valid,
  output logic        exit_valid,
  output logic [31:0] exit_code
);

  localparam MEM_WORDS = 65536; // 256 KB memory
  logic [31:0] mem [0:MEM_WORDS-1];

  logic [31:0] instr_addr;
  logic [31:0] instr_data;
  logic        instr_req;
  logic        instr_ack;

  logic [31:0] data_addr;
  logic [31:0] data_rd_data;
  logic [31:0] data_wr_data;
  logic [3:0]  data_mask;
  logic        data_wr_en;
  logic        data_req;
  logic        data_ack;

  assign pc_out = instr_addr;

  // Instruction bus synchronous SRAM response
  always_ff @(posedge clk or negedge rstz) begin
    if (!rstz) begin
      instr_ack  <= 1'b0;
      instr_data <= 32'h0;
    end else begin
      instr_ack  <= instr_req;
      if (instr_addr[17:2] < MEM_WORDS) begin
        instr_data <= mem[instr_addr[17:2]];
      end else begin
        instr_data <= 32'h00000013; // nop
      end
    end
  end

  // Data bus synchronous SRAM response and MMIO
  always_ff @(posedge clk or negedge rstz) begin
    if (!rstz) begin
      data_ack     <= 1'b0;
      data_rd_data <= 32'h0;
      uart_valid   <= 1'b0;
      exit_valid   <= 1'b0;
      exit_code    <= 32'h0;
      uart_char    <= 8'h0;
    end else begin
      data_ack   <= data_req;
      uart_valid <= 1'b0;

      if (data_req && data_wr_en) begin
        if (data_addr == 32'h800100 || data_addr == 32'h10000000) begin
          uart_char  <= data_wr_data[7:0];
          uart_valid <= 1'b1;
        end else if (data_addr == 32'h800000) begin
          exit_code  <= data_wr_data;
          exit_valid <= 1'b1;
        end else if (data_addr[17:2] < MEM_WORDS) begin
          if (data_mask[0]) mem[data_addr[17:2]][ 7: 0] <= data_wr_data[ 7: 0];
          if (data_mask[1]) mem[data_addr[17:2]][15: 8] <= data_wr_data[15: 8];
          if (data_mask[2]) mem[data_addr[17:2]][23:16] <= data_wr_data[23:16];
          if (data_mask[3]) mem[data_addr[17:2]][31:24] <= data_wr_data[31:24];
        end
      end else if (data_req) begin
        if (data_addr == 32'h800018) begin
          data_rd_data <= 32'h0; // UART queue empty
        end else if (data_addr[17:2] < MEM_WORDS) begin
          data_rd_data <= mem[data_addr[17:2]];
        end else begin
          data_rd_data <= 32'h0;
        end
      end
    end
  end

  // Instantiate Kronos RV32I Core
  kronos_core #(
    .BOOT_ADDR(32'h00010000),
    .FAST_BRANCH(1),
    .EN_COUNTERS(1),
    .EN_COUNTERS64B(1)
  ) u_core (
    .clk(clk),
    .rstz(rstz),
    .instr_addr(instr_addr),
    .instr_data(instr_data),
    .instr_req(instr_req),
    .instr_ack(instr_ack),
    .data_addr(data_addr),
    .data_rd_data(data_rd_data),
    .data_wr_data(data_wr_data),
    .data_mask(data_mask),
    .data_wr_en(data_wr_en),
    .data_req(data_req),
    .data_ack(data_ack),
    .software_interrupt(1'b0),
    .timer_interrupt(1'b0),
    .external_interrupt(1'b0)
  );

endmodule

