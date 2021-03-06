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
// Thor Scaler
//
// ============================================================================
//
`ifndef THOR_DEFINES
`define THOR_DEFINES	1'b1

`define SEGMENTATION	1'b1
//`define FLOATING_POINT	1'b1

`define TRUE	1'b1
`define FALSE	1'b0
`define INV		1'b0
`define VAL		1'b1
`define ZERO		64'd0


`define TST			8'b0000xxxx
`define CMP			8'b0001xxxx
`define CMPI		8'b0010xxxx
`define BR			8'b0011xxxx

`define RR			8'h40
`define ADD				6'h00
`define SUB				6'h01
`define MUL				6'h02
`define DIV				6'h03
`define ADDU			6'h04
`define SUBU			6'h05
`define MULU			6'h06
`define DIVU			6'h07
`define _2ADDU			6'h08
`define _4ADDU			6'h09
`define _8ADDU			6'h0A
`define _16ADDU			6'h0B
`define ADDI		8'h48
`define SUBI		8'h49
`define MULI		8'h4A
`define DIVI		8'h4B
`define ADDUI		8'h4C
`define SUBUI		8'h4D
`define MULUI		8'h4E
`define DIVUI		8'h4F
`define LOGIC		8'h50
`define AND				6'h0
`define OR				6'h1
`define EOR				6'h2
`define NAND			6'h3
`define NOR				6'h4
`define ENOR			6'h5
`define ANDC			6'h6
`define ORC				6'h7
`define ANDI		8'h53
`define ORI			8'h54
`define EORI		8'h55

`define SHIFT		8'h58
`define SHL				6'h00
`define SHR				6'h01
`define SHLU			6'h02
`define SHRU			6'h03
`define ROL				6'h04
`define ROR				6'h05
`define SHLI			6'h10
`define SHRI			6'h11
`define SHLUI			6'h12
`define SHRUI			6'h13
`define ROLI			6'h14
`define RORI			6'h15

`define _2ADDUI		8'h6B
`define _4ADDUI		8'h6C
`define _8ADDUI		8'h6D
`define _16ADDUI	8'h6E
`define LDI			8'h6F

`define NEG			8'h70
`define NOT			8'h71
`define MUX			8'h72

`define FSTAT		8'h73
`define FRM			8'h74
`define FTX			8'h75
`define ITOF		8'h76
`define FTOI		8'h77
`define DOUBLE		8'h78
`define FNEG			6'h04
`define FABS			6'h05
`define FSIGN			6'h06
`define FADD			6'h08
`define FSUB			6'h09
`define FMUL			6'h0A
`define FDIV			6'h0B

`define LB			8'h80
`define LBU			8'h81
`define LC			8'h82
`define LCU			8'h83
`define LH			8'h84
`define LHU			8'h85
`define LW			8'h86
`define LFS			8'h87
`define LFD			8'h88
`define LEA			8'h8D
`define LWS			8'h8E
`define PFLD		8'h8F

`define SB			8'h90
`define SC			8'h91
`define SH			8'h92
`define SW			8'h93
`define SFS			8'h94
`define SFD			8'h95
`define STI			8'h96
`define CAS			8'h97
`define STSB		8'h98
`define STSC		8'h99
`define STSH		8'h99
`define STSW		8'h9A

`define LDIT10		8'h9C
`define LDIS		8'h9D
`define SWS			8'h9E
`define CACHE		8'h9F

// Flow control Opcodes
`define JSR			8'hA2
`define RTS			8'hA3
`define LOOP		8'hA4
`define SYS			8'hA5
`define INT			8'hA6

`define MOV			8'hA7
`define MFSPR		8'hA8
`define MTSPR		8'hA9

`define BITFIELD	8'hAA
`define BFINS			4'h0
`define BFSET			4'h1
`define BFCLR			4'h2
`define BFCHG			4'h3
`define BFEXTU			4'h4
`define BFEXT			4'h5

`define MOVS		8'hAB

`define LBX			8'hB0
`define LBUX		8'hB1
`define LCX			8'hB2
`define LCUX		8'hB3
`define LHX			8'hB4
`define LHUX		8'hB5
`define LWX			8'hB6

`define SBX			8'hC0
`define SCX			8'hC1
`define SHX			8'hC2
`define SWX			8'hC3

`define FNABS		8'hC8
`define FMOV		8'hC9
`define FMAN		8'hCA
`define FCX			8'hCC
`define FEX			8'hCD
`define FDX			8'hCE

// Uncached access instructions
`define LVB			8'hD0
`define LVC			8'hD1
`define LVH			8'hD2
`define LVW			8'hD3

`define NOP			8'hE1

`define TLB			8'hF0
`define TLB_NOP			4'd0
`define TLB_P			4'd1
`define TLB_RD			4'd2
`define TLB_WR			4'd3
`define TLB_WI			4'd4
`define TLB_EN			4'd5
`define TLB_DIS			4'd6
`define TLB_RDREG		4'd7
`define TLB_WRREG		4'd8
`define TLB_INVALL		4'd9

`define TLBWired		4'h0
`define TLBIndex		4'h1
`define TLBRandom		4'h2
`define TLBPageSize		4'h3
`define TLBVirtPage		4'h4
`define TLBPhysPage		4'h5
`define TLBASID			4'h7
`define TLBDMissAdr		4'd8
`define TLBIMissAdr		4'd9
`define TLBPageTblAddr	4'd10
`define TLBPageTblCtrl	4'd11


`define RTE			8'hF3
`define RTI			8'hF4
`define BCD			8'hF5
`define BCDADD			8'h00
`define BCDSUB			8'h01
`define BCDMUL			8'h02
`define MEMSB		8'hF8	// synchronization barrier
`define MEMDB		8'hF9	// data barrier
`define CLI			8'hFA
`define SEI			8'hFB
`define IMM			8'hFF

`define PREDC	3:0
`define PREDR	7:4
`define OPCODE	15:8
`define RA		21:16
`define RB		27:22
`define INSTRUCTION_RA	21:16
`define INSTRUCTION_RB	27:22
`define INSTRUCTION_RC	33:28

`define XTBL	4'd12
`define EPC		4'd13
`define IPC		4'd14

// Special Registers
`define TICK			8'h02
`define LCTR			8'h03
`define PREGS			8'h04
`define ASID			8'h06
`define SR				8'h07
`define BREGS			8'h1x
`define SREGS			8'h2x
		
// exception types:
`define EXC_NONE	4'd0
`define EXC_HALT	4'd1
`define EXC_TLBMISS	4'd2
`define EXC_SIGSEGV	4'd3
`define EXC_INVALID	4'd4
`define EXC_SYS		4'd5
`define EXC_INT		4'd6
`define EXC_OFL		4'd7
`define EXC_DBE		4'd8		// databus error
`define EXC_DBZ		4'd9		// divide by zero

//
// define PANIC types
//
`define PANIC_NONE		4'd0
`define PANIC_FETCHBUFBEQ	4'd1
`define PANIC_INVALIDISLOT	4'd2
`define PANIC_MEMORYRACE	4'd3
`define PANIC_IDENTICALDRAMS	4'd4
`define PANIC_OVERRUN		4'd5
`define PANIC_HALTINSTRUCTION	4'd6
`define PANIC_INVALIDMEMOP	4'd7
`define PANIC_INVALIDFBSTATE	4'd9
`define PANIC_INVALIDIQSTATE	4'd10
`define PANIC_BRANCHBACK	4'd11
`define PANIC_BADTARGETID	4'd12

`define DRAMSLOT_AVAIL	2'b00
`define DRAMREQ_READY	2'b11

`endif
