// ============================================================================
//	(C) 2006,2014  Robert Finch
//	All rights reserved.
//	rob<remove>@finitron.ca
//
//	fpMul.v
//		- floating point multiplier
//		- two cycle latency
//		- can issue every clock cycle (for 64 bits or less)
//		- parameterized width
//		- IEEE 754 representation
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
// ============================================================================
//
//	This multiplier/divider handles denormalized numbers.
//	The output format is of an internal expanded representation
//	in preparation to be fed into a normalization unit, then
//	rounding. Basically, it's the same as the regular format
//	except the mantissa is doubled in size, the leading two
//	bits of which are assumed to be whole bits.
//
//
//	Floating Point Multiplier
//
//	Properties:
//	+-inf * +-inf = -+inf	(this is handled by exOver)
//	+-inf * 0     = QNaN
//	
//	1 sign number
//	8 exponent
//	48 mantissa
//
//	Ref: Webpack8.1i Spartan3-4 xc3s1000-4ft256
//	174 LUTS / 113 slices / 24.7 ns
//	4 Mults
// ============================================================================

module fpMul (rst, clk, ce, ld, a, b, o, sign_exe, inf, overflow, underflow, done);
parameter WID = 32;
localparam MSB = WID-1;
localparam EMSB = WID==128 ? 14 :
                  WID==96 ? 14 :
				  WID==80 ? 14 :
                  WID==64 ? 10 :
				  WID==52 ? 10 :
				  WID==48 ? 10 :
				  WID==44 ? 10 :
				  WID==42 ? 10 :
				  WID==40 ?  9 :
				  WID==32 ?  7 :
				  WID==24 ?  6 : 4;
localparam FMSB = WID==128 ? 111:
                  WID==96 ? 79 :
                  WID==80 ? 63 :
                  WID==64 ? 51 :
				  WID==52 ? 39 :
				  WID==48 ? 35 :
				  WID==44 ? 31 :
				  WID==42 ? 29 :
				  WID==40 ? 28 :
				  WID==32 ? 22 :
				  WID==24 ? 15 : 9;

localparam FX = (FMSB+2)*2-1;	// the MSB of the expanded fraction
localparam EX = FX + 1 + EMSB + 1 + 1 - 1;

input rst;
input clk;
input ce;
input ld;
input  [WID:1] a, b;
output [EX:0] o;
output sign_exe;
output inf;
output overflow;
output underflow;
output done;

reg [EMSB:0] xo1;		// extra bit for sign
reg [FX:0] mo1;

// constants
wire [EMSB:0] infXp = {EMSB+1{1'b1}};	// infinite / NaN - all ones
// The following is the value for an exponent of zero, with the offset
// eg. 8'h7f for eight bit exponent, 11'h7ff for eleven bit exponent, etc.
wire [EMSB:0] bias = {1'b0,{EMSB{1'b1}}};	//2^0 exponent
// The following is a template for a quiet nan. (MSB=1)
wire [FMSB:0] qNaN  = {1'b1,{FMSB{1'b0}}};

// variables
wire [FX:0] fract1a;
wire [FX:0] fracto;
wire [EMSB+2:0] ex1;	// sum of exponents
wire [EMSB  :0] ex2;

// Decompose the operands
wire sa, sb;			// sign bit
wire [EMSB:0] xa, xb;	// exponent bits
wire [FMSB+1:0] fracta, fractb;
wire a_dn, b_dn;			// a/b is denormalized
wire az, bz;
wire aInf, bInf, aInf1, bInf1;
wire done1;

// -----------------------------------------------------------
// First clock
// - decode the input operands
// - derive basic information
// - calculate exponent
// - calculate fraction
// -----------------------------------------------------------

fpDecomp #(WID) u1a (.i(a), .sgn(sa), .exp(xa), .fract(fracta), .xz(a_dn), .vz(az), .inf(aInf) );
fpDecomp #(WID) u1b (.i(b), .sgn(sb), .exp(xb), .fract(fractb), .xz(b_dn), .vz(bz), .inf(bInf) );

// Compute the sum of the exponents.
// correct the exponent for denormalized operands
// adjust the sum by the exponent offset (subtract 127)
// mul: ex1 = xa + xb,	result should always be < 1ffh
assign ex1 = (az|bz) ? 0 : (xa|a_dn) + (xb|b_dn) - bias;
generate
case (WID)
64:
begin
	reg [FX:0] fract1;
	reg [35:0] p00,p01,p02;
	reg [35:0] p10,p11,p12;
	reg [35:0] p20,p21,p22;
	always @(posedge clk)
	if (ce) begin
		p00 <= fracta[17: 0] * fractb[17: 0];
		p01 <= fracta[35:18] * fractb[17: 0];
		p02 <= fracta[52:36] * fractb[17: 0];
		p10 <= fracta[17: 0] * fractb[35:18];
		p11 <= fracta[35:18] * fractb[35:18];
		p12 <= fracta[52:36] * fractb[35:18];
		p20 <= fracta[17: 0] * fractb[52:36];
		p21 <= fracta[35:18] * fractb[52:36];
		p22 <= fracta[52:36] * fractb[52:36];
		fract1 <= 	                            {p02,36'b0} + {p01,18'b0} + p00 +
								  {p12,54'b0} + {p11,36'b0} + {p10,18'b0} +
					{p22,72'b0} + {p21,54'b0} + {p20,36'b0}
				;
	end
	assign done = 1'b1;
end
48:
begin
	reg [FX:0] fract1;
	reg [35:0] p00,p01;
	reg [35:0] p10,p11;
	always @(posedge clk)
	if (ce) begin
		p00 <= fracta[17: 0] * fractb[17: 0];
		p01 <= fracta[34:18] * fractb[17: 0];
		p10 <= fracta[17: 0] * fractb[34:18];
		p11 <= fracta[34:18] * fractb[34:18];
		fract1 <= {p11,p00} + {p01,18'b0} + {p10,18'b0};
	end
	assign done = 1'b1;
end
40:
begin
	reg [FX:0] fract1;
	reg [35:0] p00,p01;
	reg [35:0] p10,p11;
	always @(posedge clk)
	if (ce) begin
		p00 <= fracta[17: 0] * fractb[17: 0];
		p01 <= fracta[27:18] * fractb[17: 0];
		p10 <= fracta[17: 0] * fractb[27:18];
		p11 <= fracta[27:18] * fractb[27:18];
		fract1 <= {p11,p00} + {p01,18'b0} + {p10,18'b0};
	end
	assign done = 1'b1;
end
32:
begin
	reg [FX:0] fract1;
	reg [35:0] p00,p01;
	reg [35:0] p10,p11;
	always @(posedge clk)
	if (ce) begin
		p00 <= fracta[17: 0] * fractb[17: 0];
		p01 <= fracta[23:18] * fractb[17: 0];
		p10 <= fracta[17: 0] * fractb[23:18];
		p11 <= fracta[23:18] * fractb[23:18];
		fract1 <= {p11,p00} + {p01,18'b0} + {p10,18'b0};
	end
	assign done = 1'b1;
end
24:
begin
	reg [FX:0] fract1;
	always @(posedge clk)
	if (ce)
		fract1 <= fracta[16:0] * fractb[16:0];
	assign done = 1'b1;
end
16:
begin
	reg [FX:0] fract1;
	always @(posedge clk)
	if (ce)
		fract1 <= fracta[10:0] * fractb[10:0];
	assign done = 1'b1;
end
default:
begin
wire [FX:0] fract1;
wire done1, done3;
fpmulr2 #(WID) umul1
(
	.rst(rst),
	.clk(clk),
	.ce(ce),
	.ld(ld),
	.a(a),
	.b(b),
	.p(fract1),
	.done(done1)
);
delay3 #(1) udone (.clk(clk), .ce(ce), .i(done1), .o(done3));
assign done = done3 & done1;
end
endcase

endgenerate

// Status
wire under1, over1;
wire under = ex1[EMSB+2];	// exponent underflow
wire over = (&ex1[EMSB:0] | ex1[EMSB+1]) & !ex1[EMSB+2];

delay2 #(EMSB+1) u3 (.clk(clk), .ce(ce), .i(ex1[EMSB:0]), .o(ex2) );
delay2 #(FX+1) u4 (.clk(clk), .ce(ce), .i(fract1), .o(fracto) );
delay2 u2a (.clk(clk), .ce(ce), .i(aInf), .o(aInf1) );
delay2 u2b (.clk(clk), .ce(ce), .i(bInf), .o(bInf1) );
delay2 u6  (.clk(clk), .ce(ce), .i(under), .o(under1) );
delay2 u7  (.clk(clk), .ce(ce), .i(over), .o(over1) );

// determine when a NaN is output
wire qNaNOut;
delay2 u5 (.clk(clk), .ce(ce), .i((aInf&bz)|(bInf&az)), .o(qNaNOut) );


// -----------------------------------------------------------
// Second clock
// - correct xponent and mantissa for exceptional conditions
// -----------------------------------------------------------

wire so1;
delay3 u8 (.clk(clk), .ce(ce), .i(sa ^ sb), .o(so1) );// two clock delay!

always @(posedge clk)
	if (ce)
		casex({qNaNOut,aInf1,bInf1,over1,under1})
		5'b1xxxx:	xo1 = infXp;	// qNaN - infinity * zero
		5'b01xxx:	xo1 = infXp;	// 'a' infinite
		5'b001xx:	xo1 = infXp;	// 'b' infinite
		5'b0001x:	xo1 = infXp;	// result overflow
		5'b00001:	xo1 = 0;		// underflow
		default:	xo1 = ex2[EMSB:0];	// situation normal
		endcase

always @(posedge clk)
	if (ce)
		casex({qNaNOut,aInf1,bInf1,over1})
		4'b1xxx:	mo1 = {1'b0,qNaN|3'd4,{FMSB+1{1'b0}}};	// multiply inf * zero
		4'b01xx:	mo1 = 0;	// mul inf's
		4'b001x:	mo1 = 0;	// mul inf's
		4'b0001:	mo1 = 0;	// mul overflow
		default:	mo1 = fracto;
		endcase

delay3 u10 (.clk(clk), .ce(ce), .i(sa & sb), .o(sign_exe) );
delay1 u11 (.clk(clk), .ce(ce), .i(over1),  .o(overflow) );
delay1 u12 (.clk(clk), .ce(ce), .i(over1),  .o(inf) );
delay1 u13 (.clk(clk), .ce(ce), .i(under1), .o(underflow) );

assign o = {so1,xo1,mo1};

endmodule

module fpMul_tb();
reg clk;

initial begin
	clk = 0;
end
always #10 clk <= ~clk;

fpMul u1 (.clk(clk), .ce(1'b1), .a(0), .b(0), .o(o1), .sign_exe(sgnx1), .inf(inf1), .overflow(of1), .underflow(uf1));
fpMul u2 (.clk(clk), .ce(1'b1), .a(0), .b({}), .o(o1), .sign_exe(sgnx1), .inf(inf1), .overflow(of1), .underflow(uf1));

endmodule
