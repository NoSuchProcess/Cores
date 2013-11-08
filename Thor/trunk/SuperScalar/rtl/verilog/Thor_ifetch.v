// ============================================================================
//        __
//   \\__/ o\    (C) 2013  Robert Finch, Stratford
//    \  __ /    All rights reserved.
//     \/_//     robfinch<remove>@finitron.ca
//       ||
//
// This source file is free software: you can redistribute it and/or modify 
// it under the terms of the GNU Lesser General Public License as published 
// by the Free Software Foundation, either version 3 of the License, or     
// (at your option) any later version.                                      
//                                                                          
// This source file is distributed in the hope that it will be useful,      
// but WITHOUT ANY WARRANTY; without even the implied warranty of           
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            
// GNU General Public License for more details.                             
//                                                                          
// You should have received a copy of the GNU General Public License        
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    
//
//
// Thor SuperScalar
// Instruction fetch logic
//
// ============================================================================
//
//
// FETCH
//
// fetch at least two instructions from memory into the fetch buffer
// unless either one of the buffers is still full, in which case we
// do nothing (kinda like alpha approach)
//
	if (branchmiss) begin
	    pc <= misspc;
	    fetchbuf <= 1'b0;
	    fetchbufA_v <= 1'b0;
	    fetchbufB_v <= 1'b0;
	    fetchbufC_v <= 1'b0;
	    fetchbufD_v <= 1'b0;
	end
	else if (take_branch) begin
	    if (fetchbuf == 1'b0) begin
			case ({fetchbufA_v,fetchbufB_v,fetchbufC_v,fetchbufD_v})
			4'b0000:
				begin
					fetchbufC_instr <= insn0;
					fetchbufC_pc <= pc;
					fetchbufC_v <= ihit;
					fetchbufD_instr <= insn1;
					fetchbufD_pc <= pc + fnInsnLength(insn);
					fetchbufD_v <= ihit;
					if (ihit) pc <= pc + fnInsnLength(insn) + fnInsnLength1(insn);
					fetchbuf <= 1'b1;
				end
			4'b0100:
				begin
					fetchbufC_instr <= insn0;
					fetchbufC_pc <= pc;
					fetchbufC_v <= ihit;
					fetchbufD_instr <= insn1;
					fetchbufD_pc <= pc + fnInsnLength(insn);
					fetchbufD_v <= ihit;
					if (ihit) pc <= pc + fnInsnLength(insn) + fnInsnLength1(insn);
					if (iqentry_v[tail0]==`INV) begin
						fetchbufA_v <= `INV;
						fetchbuf <= 1'b1;
					end
				end
			4'b0111:
				if (iqentry_v[tail0]==`INV) begin
					fetchbufB_v <= `INV;
					fetchbuf <= 1'b1;
				end
			4'b1000:
				begin
					fetchbufC_instr <= insn0;
					fetchbufC_v <= ihit;
					fetchbufC_pc <= pc;
					fetchbufD_instr <= insn1;
					fetchbufD_v <= ihit;
					fetchbufD_pc <= pc + fnInsnLength(insn);
					if (ihit) pc <= pc + fnInsnLength(insn) + fnInsnLength1(insn);
					fetchbufA_v <= iqentry_v[tail0];
					fetchbuf <= fetchbuf + ~iqentry_v[tail0];
				end
			4'b1011:
				if (iqentry_v[tail0]==`INV) begin
					fetchbufA_v <= `INV;
					fetchbuf <= 1'b1;
				end
			4'b1100: 
				if (fnIsBranch(opcodeA) && predict_takenA) begin
					pc <= branch_pc;
					fetchbufA_v <= iqentry_v[tail0];
					fetchbufB_v <= `INV;		// stomp on it
					if (~iqentry_v[tail0])	fetchbuf <= 1'b0;
				end
				else begin
					if (did_branchback) begin
						fetchbufC_instr <= insn0;
						fetchbufC_v <= ihit;
						fetchbufC_pc <= pc;
						fetchbufD_instr <= insn1;
						fetchbufD_v <= ihit;
						fetchbufD_pc <= pc + fnInsnLength(insn);
						if (ihit) pc <= pc + fnInsnLength(insn) + fnInsnLength1(insn);
						fetchbufA_v <= iqentry_v[tail0];
						fetchbufB_v <= iqentry_v[tail1];
						fetchbuf <= fetchbuf + (~iqentry_v[tail0] & ~iqentry_v[tail1]);
					end
					else begin
						pc <= branch_pc;
						fetchbufA_v <= iqentry_v[tail0];
						fetchbufB_v <= iqentry_v[tail1];
						if (~iqentry_v[tail0] & ~iqentry_v[tail1])	fetchbuf <= 1'b0;
					end
				end
			4'b1111:
				begin
					fetchbufA_v <= iqentry_v[tail0];
					fetchbufB_v <= iqentry_v[tail1];
					fetchbuf <= fetchbuf + (~iqentry_v[tail0] & ~iqentry_v[tail1]);
				end
			default: panic <= `PANIC_INVALIDFBSTATE;
			endcase
		end
		else begin	// fetchbuf==1'b1
			case ({fetchbufC_v,fetchbufD_v,fetchbufA_v,fetchbufB_v})
			4'b0000:
				begin
					fetchbufA_instr <= insn0;
					fetchbufA_pc <= pc;
					fetchbufA_v <= ihit;
					fetchbufB_instr <= insn1;
					fetchbufB_pc <= pc + fnInsnLength(insn);
					fetchbufB_v <= ihit;
					if (ihit) pc <= pc + fnInsnLength(insn) + fnInsnLength1(insn);
					fetchbuf <= 1'b1;
				end
			4'b0100:
				begin
					fetchbufA_instr <= insn0;
					fetchbufA_pc <= pc;
					fetchbufA_v <= ihit;
					fetchbufB_instr <= insn1;
					fetchbufB_pc <= pc + fnInsnLength(insn);
					fetchbufB_v <= ihit;
					if (ihit) pc <= pc + fnInsnLength(insn) + fnInsnLength1(insn);
					if (iqentry_v[tail0]==`INV) begin
						fetchbufD_v <= `INV;
						fetchbuf <= 1'b1;
					end
				end
			4'b0111:
				if (iqentry_v[tail0]==`INV) begin
					fetchbufD_v <= `INV;
					fetchbuf <= 1'b1;
				end
			4'b1000:
				begin
					fetchbufA_instr <= insn0;
					fetchbufA_v <= ihit;
					fetchbufA_pc <= pc;
					fetchbufB_instr <= insn1;
					fetchbufB_v <= ihit;
					fetchbufB_pc <= pc + fnInsnLength(insn);
					if (ihit) pc <= pc + fnInsnLength(insn) + fnInsnLength1(insn);
					fetchbufC_v <= iqentry_v[tail0];
					fetchbuf <= fetchbuf + ~iqentry_v[tail0];
				end
			4'b1011:
				if (iqentry_v[tail0]==`INV) begin
					fetchbufC_v <= `INV;
					fetchbuf <= 1'b1;
				end
			4'b1100:
				if (fnIsBranch(opcodeC) && predict_takenC) begin
					pc <= branch_pc;
					fetchbufC_v <= iqentry_v[tail0];
					fetchbufD_v <= `INV;		// stomp on it
					if (~iqentry_v[tail0])	fetchbuf <= 1'b0;
				end
				else begin
					if (did_branchback) begin
						fetchbufA_instr <= insn0;
						fetchbufA_v <= ihit;
						fetchbufA_pc <= pc;
						fetchbufB_instr <= insn1;
						fetchbufB_v <= ihit;
						fetchbufB_pc <= pc + fnInsnLength(insn);
						if (ihit) pc <= pc + fnInsnLength(insn) + fnInsnLength1(insn);
						fetchbufC_v <= iqentry_v[tail0];
						fetchbufD_v <= iqentry_v[tail1];
						fetchbuf <= fetchbuf + (~iqentry_v[tail0] & ~iqentry_v[tail1]);
					end
					else begin
						pc <= branch_pc;
						fetchbufC_v <= iqentry_v[tail0];
						fetchbufD_v <= iqentry_v[tail1];
						if (~iqentry_v[tail0] & ~iqentry_v[tail1])	fetchbuf <= 1'b0;
					end
				end
			4'b1111:
				begin
					fetchbufC_v <= iqentry_v[tail0];
					fetchbufD_v <= iqentry_v[tail1];
					fetchbuf <= fetchbuf + (~iqentry_v[tail0] & ~iqentry_v[tail1]);
				end
			default: panic <= `PANIC_INVALIDFBSTATE;
			endcase
		end
	end
	else begin
		if (fetchbuf == 1'b0)
			case ({fetchbufA_v, fetchbufB_v, ~iqentry_v[tail0], ~iqentry_v[tail1]})
			4'b00_00 : ;
			4'b00_01 : panic <= `PANIC_INVALIDIQSTATE;
			4'b00_10 : ;
			4'b00_11 : ;
			4'b01_00 : ;
			4'b01_01 : panic <= `PANIC_INVALIDIQSTATE;
			4'b01_10,
			4'b01_11 : begin
				fetchbufB_v <= `INV;
				fetchbuf <= ~fetchbuf;
				end
			4'b10_00 : ;
			4'b10_01 : panic <= `PANIC_INVALIDIQSTATE;
			4'b10_10,
			4'b10_11 : begin
				fetchbufA_v <= `INV;
				fetchbuf <= ~fetchbuf;
				end
			4'b11_00 : ;
			4'b11_01 : panic <= `PANIC_INVALIDIQSTATE;
			4'b11_10 : begin
				fetchbufA_v <= `INV;
				end
			4'b11_11 : begin
				$display("case 11_11");
				fetchbufA_v <= `INV;
				fetchbufB_v <= `INV;
				fetchbuf <= ~fetchbuf;
				end
			endcase
		else
			case ({fetchbufC_v, fetchbufD_v, ~iqentry_v[tail0], ~iqentry_v[tail1]})
			4'b00_00 : ;
			4'b00_01 : panic <= `PANIC_INVALIDIQSTATE;
			4'b00_10 : ;
			4'b00_11 : ;
			4'b01_00 : ;
			4'b01_01 : panic <= `PANIC_INVALIDIQSTATE;

			4'b01_10,
			4'b01_11 : begin
				fetchbufD_v <= `INV;
				fetchbuf <= ~fetchbuf;
				end

			4'b10_00 : ;
			4'b10_01 : panic <= `PANIC_INVALIDIQSTATE;

			4'b10_10,
			4'b10_11 : begin
				fetchbufC_v <= `INV;
				fetchbuf <= ~fetchbuf;
				end

			4'b11_00 : ;
			4'b11_01 : panic <= `PANIC_INVALIDIQSTATE;

			4'b11_10 : begin
				fetchbufC_v <= `INV;
				end

			4'b11_11 : begin
				fetchbufC_v <= `INV;
				fetchbufD_v <= `INV;
				fetchbuf <= ~fetchbuf;
				end
			endcase
		if (fetchbufA_v == `INV && fetchbufB_v == `INV) begin
			fetchbufA_instr <= insn0;
			fetchbufA_v <= ihit;
			fetchbufA_pc <= pc;
			fetchbufB_instr <= insn1;
			fetchbufB_v <= ihit;
			fetchbufB_pc <= pc + fnInsnLength(insn);
			if (ihit) pc <= pc + fnInsnLength(insn) + fnInsnLength1(insn);
		end
		else if (fetchbufC_v == `INV && fetchbufD_v == `INV) begin
			fetchbufC_instr <= insn0;
			fetchbufC_v <= ihit;
			fetchbufC_pc <= pc;
			fetchbufD_instr <= insn1;
			fetchbufD_v <= ihit;
			fetchbufD_pc <= pc + fnInsnLength(insn);
			if (ihit) pc <= pc + fnInsnLength(insn) + fnInsnLength1(insn);
		end
	end
