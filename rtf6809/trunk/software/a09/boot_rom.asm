; ============================================================================
;        __
;   \\__/ o\    (C) 2013  Robert Finch, Stratford
;    \  __ /    All rights reserved.
;     \/_//     robfinch<remove>@opencores.org
;       ||
;  
;
; This source file is free software: you can redistribute it and/or modify 
; it under the terms of the GNU Lesser General Public License as published 
; by the Free Software Foundation, either version 3 of the License, or     
; (at your option) any later version.                                      
;                                                                          
; This source file is distributed in the hope that it will be useful,      
; but WITHOUT ANY WARRANTY; without even the implied warranty of           
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            
; GNU General Public License for more details.                             
;                                                                          
; You should have received a copy of the GNU General Public License        
; along with this program.  If not, see <http://www.gnu.org/licenses/>.    
;                                                                          
; ============================================================================
;
CR	EQU	$0D		;ASCII equates
LF	EQU	$0A
TAB	EQU	$09
CTRLC	EQU	$03
CTRLH	EQU	$08
CTRLI	EQU	$09
CTRLJ	EQU	$0A
CTRLK	EQU	$0B
CTRLM   EQU $0D
CTRLS	EQU	$13
CTRLX	EQU	$18
XON		EQU	$11
XOFF	EQU	$13

MAX_TASKNO	EQU 63
DRAM_BASE	EQU $10000000

ScreenLocation		EQU		$10
ColorCodeLocation	EQU		$14
ScreenLocation2		EQU		$18
BlkcpySrc			EQU		$1C
BlkcpyDst			EQU		$20
Strptr				EQU		$24

; Task control blocks, room for 256 tasks
TCB_NxtRdy		EQU		$17EF8400	; next task on ready / timeout list
TCB_PrvRdy		EQU		$17EF8500	; previous task on ready / timeout list
TCB_NxtTCB		EQU		$17EF8600
TCB_Timeout		EQU		$17EF8700
TCB_Priority	EQU		$17EF8800
TCB_MSGPTR_D1	EQU		$17EF8900
TCB_MSGPTR_D2	EQU		$17EF8A00
TCB_hJCB		EQU		$17EF8B00
TCB_Status		EQU		$17EF8C00
TCB_CursorRow	EQU		$17EF4400
TCB_CursorCol	EQU		$17EF4500
TCB_hWaitMbx	EQU		$17EF4600	; handle of mailbox task is waiting at
TCB_mbq_next	EQU		$17EF4700	; mailbox queue next
TCB_mbq_prev	EQU		$17EF4800	; mailbox queue previous
TCB_iof_next	EQU		$17EF4900
TCB_iof_prev	EQU		$17EF4A00
TCB_SP8Save		EQU		$17EF4B00	; TCB_SP8Save area 
TCB_SPSave		EQU		$17EF4C00	; TCB_SPSave area
TCB_ABS8Save	EQU		$17EF4D00
TCB_mmu_map		EQU		$17EF4E00

KeybdHead		EQU		$17EFA800
KeybdTail		EQU		$17EFA900
KeybdEcho		EQU		$17EFAA00
KeybdBad		EQU		$17EFAB00
KeybdAck		EQU		$17EFAC00
KeybdLocks		EQU		$17EFAD00
KeybdBuffer		EQU		$17EFB000	; buffer is 16 chars

LEDS		EQU		$FFDC0600
TEXTSCR		EQU		$FFD00000
TEXTREG		EQU		$FFDA0000
TEXT_COLS	EQU		0
TEXT_ROWS	EQU		2
TEXT_CURPOS	EQU		22
KEYBD		EQU		$FFDC0000
KEYBDCLR	EQU		$FFDC0002
BIOS_SCREENS	EQU	$17000000	; $17000000 to $171FFFFF

; EhBASIC vars:
;
NmiBase		EQU		$DC
IrqBase		EQU		$DF

QNdx0		EQU		$780
QNdx1		EQU		QNdx0+2
QNdx2		EQU		QNdx1+2
QNdx3		EQU		QNdx2+2
QNdx4		EQU		QNdx3+2
FreeTCB		EQU		QNdx4+2
TimeoutList	EQU		FreeTCB+2
RunningTCB		EQU		TimeoutList+2
FreeMbx		EQU		RunningTCB + 2
nMailbox	EQU		FreeMbx + 2
FreeMsg		EQU		nMailbox + 2
nMsgBlk		EQU		FreeMsg + 2

; The IO focus list is a doubly linked list formed into a ring.
;
IOFocusNdx	EQU		nMsgBlk + 2
IrqSource	EQU		$79A

CharColor	EQU		$7C0
ScreenColor	EQU		$7C2
CursorFlash	EQU		$7C4
IRQFlag		EQU		$7C6

	org		$0000F000

start:
	lda		#1
	sta		LEDS
	leas	$3FFF
	andcc	#$EF			; unmask irq
	ldd		#$CE
	std		ScreenColor
	std		CharColor
	jsr		ClearScreen
	jsr		HomeCursor
	ldd		#<msgStartup
	std		Strptr+2
	ldd		#>msgStartup
	std		Strptr
	lda		#5
	sta		LEDS
	jsr		DisplayString
	jmp		Monitor

msgStartup
	fcb		"RTF6809 System Starting.",CR,LF,0

;------------------------------------------------------------------------------
; Convert ASCII character to screen display character.
; Parameter
;	acca = ascii character
; Returns:
;	d = screen character
;------------------------------------------------------------------------------
;
AsciiToScreen:
	clrb
	cmpa	#'A'
	blo		atoscr1
	cmpa	#'Z'
	bls		atoscr1
	cmpa	#'z'
	bhi		atoscr1
	cmpa	#'a'
	blo		atoscr1
	suba	#$60
atoscr1:
	orb		#$1
TestRts:
	rts

;------------------------------------------------------------------------------
; Convert screen character to ascii character
;------------------------------------------------------------------------------
;
ScreenToAscii:
	cmpa	#26+1
	bcs		stasc1
	adda	#$60
stasc1:
	rts

;------------------------------------------------------------------------------
;------------------------------------------------------------------------------
ShiftLeft5:
	aslb
	rola
	aslb
	rola
	aslb
	rola
	aslb
	rola
	aslb
	rola
	rts

;------------------------------------------------------------------------------
;------------------------------------------------------------------------------
;
BlockCopyWords:
	ldy		#0
bcw1:
	ldd		far [BlkcpySrc],y
	std		far [BlkcpyDst],y
	leay	2,y
	leax	-1,x
	bne		bcw1
	rts

;------------------------------------------------------------------------------
;------------------------------------------------------------------------------
;
CopyVirtualScreenToScreen
	pshs	d,x,y
	ldd		IOFocusNdx			; compute virtual screen location
	bmi		cvss3
	; 8192 bytes per screen
	clr		BlkcpySrc+3
	clr		BlkcpySrc
	jsr		ShiftLeft5
	std		BlkcpySrc+1

	; add in screens array base address
	ldd		BlkcpySrc+2
	addd	#<BIOS_SCREENS
	std		BlkcpySrc+2
	ldd		BlkcpySrc
	adcb	#0
	adca	#0
	addd	#>BIOS_SCREENS
	std		BlkcpySrc

	; set destination pointer
	ldd		#<TEXTSCR
	std		BlkcpyDst+2
	ldd		#>TEXTSCR
	std		BlkcpyDst

	ldx		#2047				; number of words to copy-1
	jsr		BlockCopyWords

	; now copy the color codes
	ldd		IOFocusNdx			; compute virtual screen location
	; 8192 bytes per screen
	clr		BlkcpySrc+3
	clr		BlkcpySrc
	jsr		ShiftLeft5
	std		BlkcpySrc+1

	; add in screens array base address
	ldd		BlkcpySrc+2
	addd	#<BIOS_SCREENS+4096
	std 	BlkcpySrc+2
	ldd		BlkcpySrc
	adcb	#0
	adca	#0
	addd	#>BIOS_SCREENS+4096
	std		BlkcpySrc

	; set destination pointer
	ldd		#<TEXTSCR+$10000
	std		BlkcpyDst+2
	ldd		#>TEXTSCR+$10000
	std		BlkcpyDst

	ldx		#2047				; number of words to copy-1
	jsr		BlockCopyWords

cvss3:
	; reset the cursor position in the text controller
	ldy		IOFocusNdx
	ldb		TCB_CursorRow,y
	lda		TEXTREG+TEXT_COLS
	mul
	tfr		d,x
	ldb		TCB_CursorCol,y
	abx
	stx		TEXTREG+TEXT_CURPOS
	puls	d,x,y
	rts

CopyScreenToVirtualScreen
	pshs	d,x,y
	ldd		IOFocusNdx			; compute virtual screen location
	bmi		csvs3
	; 8192 bytes per screen
	clr		BlkcpyDst+3
	clr		BlkcpyDst
	jsr		ShiftLeft5
	std		BlkcpyDst+1

	; add in screens array base address
	ldd		BlkcpyDst+2
	addd	#<BIOS_SCREENS
	std		BlkcpyDst+2
	ldd		BlkcpyDst
	adcb	#0
	adca	#0
	addd	#>BIOS_SCREENS
	std		BlkcpyDst

	; set destination pointer
	ldd		#<TEXTSCR
	std		BlkcpySrc+2
	ldd		#>TEXTSCR
	std		BlkcpySrc

	ldx		#2047				; number of words to copy-1
	jsr		BlockCopyWords

	; now copy the color codes
	ldd		IOFocusNdx			; compute virtual screen location
	; 8192 bytes per screen
	clr		BlkcpyDst+3
	clr		BlkcpyDst
	jsr		ShiftLeft5
	std		BlkcpyDst+1

	; add in screens array base address
	ldd		BlkcpyDst+2
	addd	#<BIOS_SCREENS+4096
	std		BlkcpyDst+2
	ldd		BlkcpyDst
	adcb	#0
	adca	#0
	addd	#>BIOS_SCREENS+4096
	std		BlkcpyDst

	; set destination pointer
	ldd		#<TEXTSCR+$10000
	std		BlkcpySrc+2
	ldd		#>TEXTSCR+$10000
	std		BlkcpySrc

	ldx		#2047				; number of words to copy-1
	jsr		BlockCopyWords

	puls	d,x,y
csvs3:
	rts

;------------------------------------------------------------------------------
; Clear the screen and the screen color memory
; We clear the screen to give a visual indication that the system
; is working at all.
;------------------------------------------------------------------------------
;
far ClearScreen:
	pshs	d,x,y,u
	lda		#2
	sta		LEDS
	lda		TEXTREG+TEXT_COLS	; calc number to clear
	ldb		TEXTREG+TEXT_ROWS
	mul							; d = # chars to clear
	tfr		d,x
	tfr		d,u
	jsr		GetScreenLocation
	lda		#3
	sta		LEDS
	lda		#' '				; space char
	jsr		AsciiToScreen
	ldy		#0
cs1
	std		far [ScreenLocation],y	; clear the memory
	leay	2,y					; increment y
	leax	-1,x				; decrement x
	bne		cs1
	lda		#4
	sta		LEDS
	jsr		GetColorCodeLocation
	ldd		ScreenColor			; x = value to use
	exg		a,b
	ldy		#0
	tfr		u,x
cs2
	std		far [ColorCodeLocation],y	; clear the memory
	leay	2,y					; increment y
	leax	-1,x				; decrement x
	bne		cs2
	puls	d,x,y,u
	rtf

;------------------------------------------------------------------------------
; Scroll text on the screen upwards
;------------------------------------------------------------------------------
;
far ScrollUp:
	pshs	d,x,y,u
	lda		TEXTREG+TEXT_COLS	; acc = # text columns
	ldb		TEXTREG+TEXT_ROWS
	decb						; one less row
	mul							; calc number of chars to scroll
	tfr		d,y					; y = count of chars to move
	jsr		GetScreenLocation
	jsr		GetColorCodeLocation
	ldu		#0					; u = index to target row
	ldx		#0
	ldb		TEXTREG+TEXT_COLS
	abx							; x = index to source row
scrup1:
	ldd		far [ScreenLocation],x		; move character
	std		far [ScreenLocation],u
	ldd		far [ColorCodeLocation],x	; and move color code
	std		far [ColorCodeLocation],u
	leax	2,x
	leau	2,u
	leay	-1,y
	bne		scrup1
	lda		TEXTREG+TEXT_ROWS
	deca
	jsr		BlankLine
	puls	d,x,y,u
	rtf

;------------------------------------------------------------------------------
; Blank out a line on the display
; line number to blank is in acca
;------------------------------------------------------------------------------
;
far BlankLine:
	pshs	d,x,y
	ldb		TEXTREG+TEXT_COLS	; b = # chars to blank out from video controller
	mul							; d = screen index (row# * #cols)
	jsr		GetScreenLocation
	tfr		d,y
	lda		#' '
	jsr		AsciiToScreen
	tfr		d,x
	ldb		TEXTREG+TEXT_COLS	; b = # chars to blank out from video controller
blnkln1:
	stx		far [ScreenLocation],y
	leay	2,y
	decb
	bne		blnkln1
	puls	d,x,y
	rtf

;------------------------------------------------------------------------------
; Get the location of the screen and screen attribute memory. The location
; depends on whether or not the task has the output focus.
;------------------------------------------------------------------------------
; private
GetScreenLocation:
	pshs	d
	lda		#10
	sta		LEDS
	ldd		RunningTCB
	cmpd	IOFocusNdx
	beq		gsl1
	; 8192 words per screen
	clra
	jsr		ShiftLeft5
	clr		ScreenLocation+3	; low 8 bits=0
	std		ScreenLocation+1	; bits 23:8
	clr		ScreenLocation
	ldd		ScreenLocation+2
	addd	#<BIOS_SCREENS
	std		ScreenLocation+2
	ldd		ScreenLocation
	adcb	#0
	adca	#0
	addd	#>BIOS_SCREENS
	std		ScreenLocation
	lda		#13
	sta		LEDS
	puls	d
	rts
gsl1:
	lda		#11
	sta		LEDS
	ldd		#>TEXTSCR
	std		ScreenLocation
	ldd		#<TEXTSCR
	std		ScreenLocation+2
	lda		#12
	sta		LEDS
	puls	d
	rts

GetColorCodeLocation:
	pshs	d
	ldd		RunningTCB
	cmpd	IOFocusNdx
	beq		gccl1
	; 8192 words per screen
	clra
	jsr		ShiftLeft5
	clr		ColorCodeLocation+3	; low 8 bits=0
	stb		ColorCodeLocation+2	; bits 15:8
	sta		ColorCodeLocation+1	; bits 23:16
	clr		ColorCodeLocation
	ldd		ColorCodeLocation+2
	addd	#<BIOS_SCREENS+4096
	std		ColorCodeLocation+2
	ldd		ColorCodeLocation
	adcb	#0
	adca	#0
	addd	#>BIOS_SCREENS+4096
	std		ColorCodeLocation
	puls	d
	rts
gccl1:
	ldd		#<TEXTSCR+$10000
	std		ColorCodeLocation+2
	ldd		#>TEXTSCR+$10000
	std		ColorCodeLocation
	puls	d
	rts

;------------------------------------------------------------------------------
; HomeCursor
; Set the cursor location to the top left of the screen.
;------------------------------------------------------------------------------
HomeCursor:
	pshs	x
	ldx		RunningTCB
	clr		TCB_CursorRow,x
	clr		TCB_CursorCol,x
	cmpx	IOFocusNdx
	bne		hc1
	clr		TEXTREG+TEXT_CURPOS
	clr		TEXTREG+TEXT_CURPOS+1
hc1:
	puls	x
	rts

;------------------------------------------------------------------------------
; Update the cursor position in the text controller based on the
;  CursorRow,CursorCol.
;------------------------------------------------------------------------------
;
UpdateCursorPos:
	pshs	d,x,y
	ldx		RunningTCB
	cmpx	IOFocusNdx				; update cursor position in text controller
	bne		ucp1					; only for the task with the output focus
	lda		TCB_CursorRow,x
	anda	#$3F					; limit of 63 rows
	ldb		TEXTREG+TEXT_COLS
	mul
	tfr		d,y
	ldb		TCB_CursorCol,x
	tfr		y,x
	abx
	stx		TEXTREG+TEXT_CURPOS
ucp1:
	puls	d,x,y
	rts

;------------------------------------------------------------------------------
; Calculate screen memory location from CursorRow,CursorCol.
; Also refreshes the cursor location.
; Returns:
; r1 = screen location
;------------------------------------------------------------------------------
;
CalcScreenLoc:
	pshs	d,x,y
	ldy		RunningTCB
	lda		TCB_CursorRow,y
	anda	#$3F					; limit to 63 rows
	ldb		TEXTREG+TEXT_COLS
	mul
	tfr		d,x
	ldb		TCB_CursorCol,y
	andb	#$7F					; limit to 127 cols
	abx
	cmpy	IOFocusNdx				; update cursor position in text controller
	bne		csl1					; only for the task with the output focus
	stx		TEXTREG+TEXT_CURPOS
csl1:
	tfr		x,d						
	aslb							; * 2 for 2 bytes per char displayed.
	rola
	jsr		GetScreenLocation
	addd	ScreenLocation+2
	std		ScreenLocation+2
	bcc		csl3
	ldd		ScreenLocation
	addd	#1
	std		ScreenLocation
csl3:
	puls	d,x,y
	rts

;------------------------------------------------------------------------------
; Display a character on the screen.
; If the task doesn't have the I/O focus then the character is written to
; the virtual screen.
; a = char to display
;------------------------------------------------------------------------------
;
far DisplayChar:
	pshs	a,x
	ldx		RunningTCB
	cmpa	#CR					; carriage return ?
	bne		dccr
	clr		TCB_CursorCol,x		; just set cursor column to zero on a CR
	jsr		UpdateCursorPos
dcx14:
	puls	a,x
	rtf
dccr:
	cmpa	#$91				; cursor right ?
	bne		dcx6
	lda		TCB_CursorCol,x
	cmpa	#55
	bhs		dcx7
	inca
	sta		TCB_CursorCol,x
dcx7:
	jsr		UpdateCursorPos
	puls	a,x
	rtf
dcx6:
	cmpa	#$90				; cursor up ?
	bne		dcx8		
	lda		TCB_CursorRow,x
	beq		dcx7
	deca
	sta		TCB_CursorRow,x
	bra		dcx7
dcx8:
	cmpa	#$93				; cursor left ?
	bne		dcx9
	lda		TCB_CursorCol,x
	beq		dcx7
	deca
	sta		TCB_CursorCol,x
	bra		dcx7
dcx9:
	cmpa	#$92				; cursor down ?
	bne		dcx10
	lda		TCB_CursorRow,x
	cmpa	#46
	beq		dcx7
	inca
	sta		TCB_CursorRow,x
	bra		dcx7
dcx10:
	cmpa	#$94				; cursor home ?
	bne		dcx11
	lda		TCB_CursorCol,x
	beq		dcx12
	clr		TCB_CursorCol,x
	bra		dcx7
dcx12:
	clr		TCB_CursorRow,x
	bra		dcx7
dcx11:
	pshs	y,u
	cmpa	#$99				; delete ?
	bne		dcx13
	jsr		CalcScreenLoc
	lda		TCB_CursorCol,x		; acc = cursor column
	bra		dcx5
dcx13
	cmpa	#CTRLH				; backspace ?
	bne		dcx3
	lda		TCB_CursorCol,x
	beq		dcx4
	deca
	sta		TCB_CursorCol,x
	jsr		CalcScreenLoc
	ldy		#2
	ldu		#0
dcx5:
	ldx		far [ScreenLocation],y
	stx		far [ScreenLocation],u
	leay	2,y
	leau	2,u
	inca
	cmpa	#55
	blo		dcx5
	lda		#' '
	jsr		AsciiToScreen
	leay	-2,y
	std		far [ScreenLocation],y
	bra		dcx4
dcx3:
	cmpa	#LF				; linefeed ?
	beq		dclf
	jsr 	CalcScreenLoc
	jsr		AsciiToScreen	; convert ascii char to screen char
	std		far [ScreenLocation]
	jsr		GetScreenLocation
	jsr		GetColorCodeLocation
	ldx		ScreenLocation+2
	stx		ColorCodeLocation+2
	ldd		CharColor
	std		far [ColorCodeLocation]
	jsr		IncCursorPos
	bra		dcx4
dclf:
	jsr		IncCursorRow
dcx4:
	puls	y,u
	puls	a,x
	rtf

;------------------------------------------------------------------------------
; Increment the cursor position, scroll the screen if needed.
;------------------------------------------------------------------------------
;
IncCursorPos:
	pshs	a,x
	ldx		RunningTCB
	lda		TCB_CursorCol,x
	inca
	sta		TCB_CursorCol,x
	cmpa	#55
	blo		icc1
	clr		TCB_CursorCol,x		; column = 0
	bra		icr1
IncCursorRow:
	pshs	a,x
	ldx		RunningTCB
icr1:
	lda		TCB_CursorRow,x
	inca
	sta		TCB_CursorRow,x
	cmpa	#31
	blo		icc1
	deca							; backup the cursor row, we are scrolling up
	sta		TCB_CursorRow,x
	jsr		ScrollUp
icc1:
	jsr		UpdateCursorPos
icc2:
	puls	a,x
	rts

;------------------------------------------------------------------------------
; Display a string on the screen.
; Parameters:
;	Strptr = pointer to string
;------------------------------------------------------------------------------
;
far DisplayString:
	pshs	a,x
	ldx		#0
dspj1B:
	lda		far [Strptr],x	; move string char into acc
	beq		dsretB			; is it end of string ?
	jsr		DisplayChar		; display character
	leax	1,x
	bra		dspj1B
dsretB:
	puls	a,x
	rtf

far CRLF
	lda		#CR
	jsr		DisplayChar
	lda		#LF
	jsr		DisplayChar
	rtf

;==============================================================================
; Keyboard I/O
;==============================================================================

;------------------------------------------------------------------------------
; Check if there is a keyboard character available. If so return true (1)
; otherwise return false (0) in acca.
;------------------------------------------------------------------------------
;
KeybdCheckForKeyDirect:
	ldd		KEYBD
	clrb
	anda	#$80
	beq		kcfkd1
	lda		#1
kcfkd1
	rts

;------------------------------------------------------------------------------
; Get character directly from keyboard. This routine blocks until a key is
; available.
;------------------------------------------------------------------------------
;
KeybdGetCharDirect:
	pshs	x
kgc1:
	ldd		KEYBD
	bita	#$80			; is there a char available ?
	beq		kgc1			; no, check again
	std		KEYBD+2			; clear keyboard strobe
	bita	#$8				; is it a keydown event ?
	bne		kgc1			; no, go back check for another char
	tfr		b,a				; tranfser ascii code to acca
	ldb		KeybdEcho		; is keyboard echo on ?
	beq		gk1				; no keyboard echo, just return char
	cmpa	#CR
	bne		gk2				; convert CR keystroke into CRLF
	jsr		CRLF
	bra		gk1
gk2:
	jsr		DisplayChar
gk1:
	puls	x
	rts

;------------------------------------------------------------------------------
; r1 0=echo off, non-zero = echo on
;------------------------------------------------------------------------------
;
far SetKeyboardEcho:
	pshs	x
	ldx		RunningTCB
	sta		KeybdEcho,x
	puls	x
	rtf

;==============================================================================
;==============================================================================
;
Monitor:
	leas	$3FFF
	lda		#0					; turn off keyboard echo
	jsr		SetKeyboardEcho
;	jsr		RequestIOFocus
PromptLn:
	jsr		CRLF
	lda		#'$'
	jsr		DisplayChar

; Get characters until a CR is keyed
;
Prompt3:
	jsr		KeybdGetCharDirect
	cmpa	#CR
	beq		Prompt1
	jsr		DisplayChar
	bra		Prompt3

; Process the screen line that the CR was keyed on
;
Prompt1:
	ldy		#0				; index to start of line
	ldd		#$5050
	std		LEDS
	ldx		RunningTCB
	cmpx	#MAX_TASKNO
	bhi		Prompt3
	ldd		#$5151
	std		LEDS
	clr		TCB_CursorCol,x	; go back to the start of the line
	jsr		CalcScreenLoc	; calc screen memory location
	ldd		#$5252
	std		LEDS
	jsr		MonGetch
	cmpa	#'$'
	bne		Prompt2			; skip over '$' prompt character
	lda		#$5353
	std		LEDS
	jsr		MonGetch

; Dispatch based on command character
;
Prompt2:
	cmpa	#'?'			; $? - display help
	bne		Prompt3
	ldd		#<HelpMsg
	std		Strptr+2
	ldd		#>HelpMsg
	std		Strptr
	jsr		DisplayString
	jmp		Monitor

MonGetch:
	ldd		far [ScreenLocation],y
	exg		a,b
	leay	2,y
	jsr		ScreenToAscii
	rts

HelpMsg:
	fcb		"? = Display help",CR,LF
;	db	"CLS = clear screen",CR,LF
;	db	"S = Boot from SD Card",CR,LF
;	db	": = Edit memory bytes",CR,LF
;	db	"L = Load sector",CR,LF
;	db	"W = Write sector",CR,LF
;	db  "DR = Dump registers",CR,LF
;	db	"D = Dump memory",CR,LF
;	db	"F = Fill memory",CR,LF
;	db  "FL = Dump I/O Focus List",CR,LF
;	db  "FIG = start FIG Forth",CR,LF
;	db	"KILL n = kill task #n",CR,LF
;	db	"B = start tiny basic",CR,LF
;	db	"b = start EhBasic 6502",CR,LF
;	db	"J = Jump to code",CR,LF
;	db	"R[n] = Set register value",CR,LF
;	db	"r = random lines - test bitmap",CR,LF
;	db	"e = ethernet test",CR,LF
;	db	"T = Dump task list",CR,LF
;	db	"TO = Dump timeout list",CR,LF
;	db	"TI = display date/time",CR,LF
;	db	"TEMP = display temperature",CR,LF
;	db	"P = Piano",CR,LF,0
	fcb		0

;------------------------------------------------------------------------------
;------------------------------------------------------------------------------
irq_rout:
	lda		#3				; Timer is IRQ #3
	sta		IrqSource		; stuff a byte indicating the IRQ source for PEEK()
	lda		IrqBase			; get the IRQ flag byte
	lsra
	ora		IrqBase
	anda	#$E0
	sta		IrqBase

	inc		TEXTSCR+110		; update IRQ live indicator on screen
	
	; flash the cursor
	ldx		RunningTCB
	cpx		IOFocusNdx		; only bother to flash the cursor for the task with the IO focus.
	bne		tr1a
	lda		CursorFlash		; test if we want a flashing cursor
	beq		tr1a
	jsr		CalcScreenLoc	; compute cursor location in memory
	ldy		ScreenLocation+2
	lda		$FFD10000,y		; get color code $10000 higher in memory
	ldb		IRQFlag			; get counter
	lsrb
	lsra
	lsra
	lsra
	lsra
	lsrb
	rola
	lsrb
	rola
	lsrb
	rola
	lsrb
	rola
	sta		$FFD10000,y		; store the color code back to memory
tr1a
	rti

	org		$FFF8
	fcw		irq_rout
	fcw		start		; SWI
	fcw		start		; NMI
	fcw		start		; RST
