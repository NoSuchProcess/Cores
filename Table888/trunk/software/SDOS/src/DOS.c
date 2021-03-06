#include "ff.h"
#include <stdio.h>
#include <elf64.h>

static FATFS fat1;
static char cmdbuf[200];
static char currentPath[300];
static DIR dir;
static FILINFO fno;

enum {
	E_Ok = 0,
	E_BadPriority,
};

typedef struct {
	__int64 regs[255];
	__int64 cs;
	__int64 ds;
	__int64 ss;
	TCB *nextRdy;
	TCB *prevRdy;
	TCB *nextTCB;
	TCB *prevTCB;
	TCB *nextTo;
	TCB *prevTo;
	__int64 timeout;
	__int32 priority;
	__int32 hJob;
} TCB;

TCB tempTCB;
TCB tcbs[256];
TCB *readyQ[8];
TCB *runningTCB;
TCB *freeTCB;
__int64 sysstack[1024];

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

private void initFMTK()
{
	int nn;

	for (nn = 0; nn < 8; nn++)
		readyQ[nn] = 0;
	for (nn = 0; nn < 256; nn++) {
		tcbs[nn].nextTCB = &tcbs[nn+1];
	}
	tcbs[256].nextTCB = 0;
	freeTCB = &tcbs[0];
	tempTCB.nextTCB = 0;
	tempTCB.prevTCB = 0;
	tempTCB.nextRdy = 0;
	tempTCB.prevRdy = 0;
	tempTCB.priority = 7;
	runningTCB = &tempTCB;
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

private int insertIntoReadyList(TCB *tcb)
{
	TCB *p;

	if (tcb->priority < 0 or tcb->priority > 7)
		return E_BadPriority;
	p = readyQ[tcb->priority];
	if (p==0) {
		p = tcb;
		tcb->nextRdy = tcb;
		tcb->prevRdy = tcb;
		return E_Ok;
	}
	// Insert at tail of list
	p->prevRdy->nextRdy = tcb;
	tcb->prevRdy = p->prevRdy;
	tcb->nextRdy = p;
	p->prevRdy = tcb;
}


// ----------------------------------------------------------------------------
// Reschedule tasks.
// ----------------------------------------------------------------------------

private naked Reschedule()
{
	// Switch to the system segments and stack
	asm {
		sei							; disable interrupts
		mfspr	r1,ds				; get current data and stack segments
		mfspr	r2,ss				; code segment was stored already on task's stack
		mtspr	ds,r0				; select OS segments
		mtspr	ss,r0
		lw		r250,runningICB
		sw		r1,256*8[r250]		; save off stack and data segments
		sw		r2,257*8[r250]
		sw		sp,254*8[r250]		; save off stack pointer
		lea		sp,sysstack			; setup system stack
		cli
	}
	// Save the running context
	asm {
		lw		tr,runningTCB
		smr		r1,r15,[tr]
		add		tr,tr,#15*8
		smr		r16,r31,[tr]
		add		tr,tr,#16*8
		smr		r32,r47,[tr]
		add		tr,tr,#16*8
		smr		r48,r63,[tr]
		add		tr,tr,#16*8
		smr		r64,r79,[tr]
		add		tr,tr,#16*8
		smr		r80,r95,[tr]
		add		tr,tr,#16*8
		smr		r96,r111,[tr]
		add		tr,tr,#16*8
		smr		r112,r127,[tr]
		add		tr,tr,#16*8
		smr		r128,r143,[tr]
		add		tr,tr,#16*8
		smr		r144,r159,[tr]
		add		tr,tr,#16*8
		smr		r160,r175,[tr]
		add		tr,tr,#16*8
		smr		r176,r191,[tr]
		add		tr,tr,#16*8
		smr		r192,r207,[tr]
		add		tr,tr,#16*8
		smr		r208,r223,[tr]
		add		tr,tr,#16*8
		smr		r224,r239,[tr]
		add		tr,tr,#16*8
		smr		r240,r254,[tr]
		add		tr,tr,#15*8
	}
	SelectTaskToRun();
	asm {
		mov		tr,r1
		push	tr
		lmr		r1,r15,[tr]
		add		tr,tr,#15*8
		lmr		r16,r31,[tr]
		add		tr,tr,#16*8
		lmr		r32,r47,[tr]
		add		tr,tr,#16*8
		lmr		r48,r63,[tr]
		add		tr,tr,#16*8
		lmr		r64,r79,[tr]
		add		tr,tr,#16*8
		lmr		r80,r95,[tr]
		add		tr,tr,#16*8
		lmr		r96,r111,[tr]
		add		tr,tr,#16*8
		lmr		r112,r127,[tr]
		add		tr,tr,#16*8
		lmr		r128,r143,[tr]
		add		tr,tr,#16*8
		lmr		r144,r159,[tr]
		add		tr,tr,#16*8
		lmr		r160,r175,[tr]
		add		tr,tr,#16*8
		lmr		r176,r191,[tr]
		add		tr,tr,#16*8
		lmr		r192,r207,[tr]
		add		tr,tr,#16*8
		lmr		r208,r223,[tr]
		add		tr,tr,#16*8
		lmr		r224,r239,[tr]
		add		tr,tr,#16*8
		lmr		r240,r251,[tr]
		add		tr,tr,#12*8
		lw		r253,8[tr]
		pop		tr
		sei
		sw		tr,runningTCB
		lw		sp,254*8[tr]
		; After restoring the data segment the OS vars will be inaccessible,
		; so the CS reg is used to access the vars.
		cs:lw	r250,256*8[tr]	; we can use the cs because they all point to zero
		mtspr	ds,r250
		cs:lw	r250,257*8[tr]	; for the OS
		mtspr	ss,r250
		rti
	}
}

// ----------------------------------------------------------------------------
// Select a task to run.
// ----------------------------------------------------------------------------

private __int8 startQ[32] = { 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 1, 0, 4, 0, 0, 0, 5, 0, 0, 0, 6, 0, 0, 0, 7, 0, 0, 0, 0 };
private __int8 startQNdx;

private TCB *SelectTaskToRun()
{
	int nn;
	TCB *p;
	int qToCheck;

	startQNdx++;
	startQNdx &= 31;
	qToCheck = startQ[startQNdx];
	for (nn = 0; nn < 8; nn++) {
		p = readyQ[qToCheck];
		if (p) {
			readyQ[qToCheck] = p->nextRdy;
			return p;
		}
		qToCheck++;
		qToCheck &= 7;
	}
	panic("No entries in ready queue.");
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void rand_scr()
{
	asm {
		mfspr	r2,tick
		mtspr	srand1,r2
		mfspr	r3,tick
		mtspr	srand2,r3
		ldi		r3,#$FFD00000
		ldi		r2,#1736
.j1:
		gran	r1
		mfspr	r1,rand
		sh		r1,[r3]
		add		r3,r3,#4
		dbnz	r2,.j1
	}
}

// ----------------------------------------------------------------------------
// Test the RAM in the machine above $400000 (Above the DOS area).
// If there is bad memory detected, then the DOS area is likely bad as well
// because it's the same DRAM. However we may be able to tell a specific
// bank or range of addresses that are bad.
// ----------------------------------------------------------------------------

private void ram_test()
{
	int *mem = 0x400000;
	int badcnt;
	char ch;

	badcnt = 0;
	for (mem = (int *)0x400000; mem < (int *)0x2000000; mem++) {
		*mem = 0xAAAAAAAA55555555L;
		if ((mem & 0xfff)==0) {
			printf("%x\r", mem >> 12);
			ch = getcharNoWait();
			if (ch==3)
				goto j1;
		}
	}
	printf("\r\n");
	for (mem = (int *)0x400000; mem < (int *)0x2000000; mem++) {
		if (*mem != 0xAAAAAAAA55555555L) {
			printf("bad at address: %x=%x\r\n", mem, *mem);
			badcnt++;
		}
		if (badcnt > 10)
			break;
		if ((mem & 0xfff)==0) {
			printf("%x\r", mem >> 12);
			ch = getcharNoWait();
			if (ch==3)
				goto j1;
		}
	}
j1:	;
}

void DisplayPrompt()
{
	printf("\r\n%s>",currentPath);
}

void GetCmdLine()
{
	int ch;
	int nn;

	nn = 0;
	memset(cmdbuf, 0, sizeof(cmdbuf));
	asm {
		sb		r0,$7C	; turn off keyboard echo
	}
	do {
		ch = getchar();
		switch (ch) {
		case '\r':
			putch(ch);
			putch(0x0a);
			break;
		case '\b':
			if (nn > 0) {
				nn--;
				putstr("\b \b",3);
			}
			cmdbuf[nn] = 0;
			break;
		default:
			putch(ch);
			cmdbuf[nn] = ch;
			nn++;
		}
	} while (ch != '\r');
	cmdbuf[nn] = 0;
}

void DumpWindow(BYTE *p)
{
	int nn;
	int jj;

	for (nn = 0; nn < 512; nn++) {
		if (nn%16==0)
			printf("\r\n");
		printf("%2x",p[nn]);
		if (nn==256)
			getchar();
		if (nn%16==15) {
			printf("   ");
			for (jj = 0; jj < 16; jj++) {
				if (p[nn-15+jj] < 20 || p[nn-15+jj] > 'Z')
					putch('.');
				else
					putch(p[nn-15+jj]);
			}
		}
	}
}

// Execute code loaded at $C00200

private int xcode()
{
	asm {
		ldi		r1,#$C00000
		;mtspr	ds,r1
		;mrspr	ss,r1
		push	r1
		ldi		r1,#$200
		push	r1
		rti
	}
}

private int get_biterrs()
{
	asm {
		mfspr	r1,biterr
	}
}

private int get_bithist()
{
	asm {
		mfspr	r1,bithist
	}
}

void do_biterr()
{
	int nn;

	printf("Biterrs: %d\r\n", get_biterrs());
	for (nn = 0; nn < 64; nn++)
		printf("%x ",get_bithist());
}

// ----------------------------------------------------------------------------
// Run a file.
// ----------------------------------------------------------------------------

void do_run()
{
	FIL fp;
	FRESULT res;
	UINT sz;
	UINT br;
	int er;
	int nn, ii;
	int inquotes;
	Elf64Header eh;

	static char fname[200];
	char *mem = 0xC00000;

	printf("eh.e_shstrndx=%d\r\n", &eh.e_shstrndx - &eh);
	printf("sizeof eh=%d\r\n", sizeof(eh));

	inquotes = 0;
	nn = ii = 0;
	while (isspace(cmdbuf[nn])) nn++;	// skip leading spaces
	// skip over 'run'
	if (cmdbuf[nn]=='r') nn++; else return;
	if (cmdbuf[nn]=='u') nn++; else return;
	if (cmdbuf[nn]=='n') nn++; else return;
	while (isspace(cmdbuf[nn])) nn++;	// skip any spaces
	do {
		if (cmdbuf[nn] == 0) break;
		if (cmdbuf[nn]=='"') { nn++; inquotes = !inquotes; continue; }
		if (!inquotes && isspace(cmdbuf[nn])) break;
		fname[ii] = cmdbuf[nn];
		nn++; ii++;
	} while (ii < 200);
	if (ii == 0) return;	// no file name

	res = f_open(&fp,fname,FA_OPEN_EXISTING | FA_READ);
	if (res) goto err1;
	sz = f_size(&fp);
	printf("file is %d bytes.\r\n", sz);
	res = f_read(&fp,(void *)&eh,sizeof(eh),&br);
	printf("eh.e_phoff=%d\r\n", eh.e_phoff);
	res = f_read(&fp,(void *)mem,sz-sizeof(eh),&br);
	if (res) goto err1;
	f_close(&fp);
	if (br==0) {
		printf("Can't run file %s.\r\n", fname);
		return;
	}
	if (mem[0]=='R' && mem[1]=='U' && mem[2]=='N') {
		strcpy(mem,cmdbuf);
		res = xcode();
		if (res != 0)
			printf("%s returned %d\r\n", res);
	}
	else {
		printf("Not a runnable file.\r\n");
	}
	return;
err1:
	f_close(&fp);
	printf("Can't run file(%d) %s.\r\n", er, fname);
}

// ----------------------------------------------------------------------------
// Get a listing of a directory.
// ----------------------------------------------------------------------------

void do_dir(char *pth)
{
	FRESULT res;
	char *fn;
	int nn;
	static char lfn[300];
	static TCHAR path[300];

	memset(path, 0, sizeof(path));
	strcpy(path,pth);
	trim(path);
	if (strlen(path)==0)
		res = f_getcwd(path, (UINT)sizeof(path)/sizeof(TCHAR));
	fno.lfname = lfn;
	fno.lfsize = sizeof lfn;
	res = f_opendir(&dir, path);
	putstr("\r\n",2);
	if (res == FR_OK) {
		do {
			res = f_readdir(&dir, &fno);
			if (res != FR_OK || fno.fname[0]==0)
				break;
			fn = *fno.lfname ? fno.lfname : fno.fname;
			putnum(fno.fsize,15,',');
			printf(" %s\r\n", fn);
			if (getcharNoWait()==3)
				break;
		} while (1);
	}
	f_closedir(&dir);
}


// ----------------------------------------------------------------------------
// Change the current directory.
// ----------------------------------------------------------------------------

FRESULT do_cd()
{
	DIR dir;
	FRESULT res;
	static char npath[300];
	static char newpath[300];
	int nn;

	strcpy(newpath, currentPath);
	strcpy(npath, &cmdbuf[2]);
	trim(npath);
	//if (npath[0]=='.') {
	//	if (npath[1]=='.') {
	//		for (nn = strlen(newpath)-1; nn >= 0; nn--) {
	//			if (newpath[nn]=='/')
	//				strcpy(&newpath[nn],&npath[2]);
	//		}
	//	}
	//}
	//printf("newpath:%s\r\n",npath);
	res = f_chdir(npath);

//	res = f_opendir(&dir, npath);
	if (res == FR_OK)
		strcpy(currentPath, npath);
	else
		printf("Can't change directory.");
//	f_closedir(&dir);
	return res;
}

private do_clk(int nn)
{
	asm {
		bra		.j1
		align	8
.t1:
		dw		%0000000000_0000000000_0000000000_0000000000_0000000001	; 2%
		dw		%0000000000_0000000000_0000000000_0000000000_0000011111	; 10%
		dw		%0000000000_0000000000_0000000000_0000000000_1111111111	; 20%
		dw		%0000000000_0000000000_0000000000_0000011111_1111111111	; 30%
		dw		%0000000000_0000000000_0000000000_1111111111_1111111111	; 40%
		dw		%0000000000_0000000000_0000011111_1111111111_1111111111	; 50%
		dw		%0000000000_0000000000_1111111111_1111111111_1111111111	; 60%
		dw		%0000000000_0000011111_1111111111_1111111111_1111111111	; 70%
		dw		%0000000000_1111111111_1111111111_1111111111_1111111111	; 80%
		dw		%0000011111_1111111111_1111111111_1111111111_1111111111	; 90%
		dw		%1111111111_1111111111_1111111111_1111111111_1111111111	; 100%
		nop
.j1:
		lw		r1,32[bp]
		div		r1,r1,#10	; round down to single digit
		mod		r1,r1,#11	; limit to 0-10
		shli	r1,r1,#3	; multiply by word size
		lw		r1,.t1[r1]
		mtspr	clk,r1
	}
}


private int ParseCmdLine()
{
	static char nm[300];
	int nn;

	if (strncmp(cmdbuf,"dir",3)==0) {
		do_dir(&cmdbuf[3]);
	}
	else if (strncmp(cmdbuf,"cd",2)==0)
		do_cd();
	else if (strncmp(cmdbuf,"cls",3)==0) {
		ClearScreen();
		HomeCursor();
	}
	else if (strncmp(cmdbuf,"bye",3)==0)
		return 1;
	else if (strncmp(cmdbuf,"run",3)==0)
		do_run();
	else if (strncmp(cmdbuf,"biterr",6)==0)
		do_biterr();
	else if (strncmp(cmdbuf, "ramtest", 7)==0)
		ram_test();
	else if (strncmp(cmdbuf, "clk", 3)==0) {
		nn = atoi(&cmdbuf[3]);
		do_clk(nn);
	}
	else if (strncmp(cmdbuf, "rs", 2)==0)
		rand_scr();
	return 0;
}

#define DATETIME_TIME	0xFFDC0400
#define DATETIME_DATE	0xFFDC0404
#define DATETIME_SNAPSHOT	0xFFDC0414

private naked datetime_snapshot()
{
	asm {
		sh	r0,DATETIME_SNAPSHOT
		rts
	}
}

private unsigned int get_date()
{
	asm {
		lhu	r1,DATETIME_DATE
	}
}

private unsigned int get_time()
{
	asm {
		lhu	r1,DATETIME_TIME
	}
}


private int ToJul(int year, int month, int day)
{
   int
      JulDay,
      LYear = year,
      LMonth = month,
      LDay = day;

   JulDay = LDay - 32075L + 1461L * (LYear + 4800 + (LMonth - 14L) / 12L) /
      4L + 367L * (LMonth - 2L - (LMonth - 14L) / 12L * 12L) /
      12L - 3L * ((LYear + 4900L + (LMonth - 14L) / 12L) / 100L) / 4L;
   return(JulDay);
}

// Get a 64 bit datetime serial number
// Months are assumed to contain 31 days.

private int get_time_serial()
{
	int ii,nn;
	int year, month, day;
	int hours, minutes, seconds, centiseconds;

	datetime_snapshot();
	ii = get_time() | (get_date() << 32);
	date_split(ii, &year, &month, &day, &hours, &minutes, &seconds, &centiseconds);
	nn = centiseconds + seconds * 100 + minutes * 6000 + hours * 360000 +
		ToJul(year,month,day) * 8640000L;
	return nn;
}

void date_split(int date, int *year, int *month, int *day,
	int *hours, int *minutes, int *seconds, int *centiseconds)
{
	// BCD to binary convert
	if (day) {
		*day = (date >> 32) & 15;
		*day = *day + ((date >> 36) & 15) * 10;
	}
	if (month) {
		*month = (date >> 40) & 15;
		*month = *month + ((date >> 44) & 15) * 10;
	}
	if (year) {
		*year = (date >> 48) & 15;
		*year = *year + ((date >> 52) & 15) * 10;
		*year = *year + ((date >> 56) & 15) * 100;
		*year = *year + ((date >> 60) & 15) * 1000;
	}
	if (centiseconds) {
		*centiseconds = date & 15;
		*centiseconds = *centiseconds + ((date >> 4) & 15) * 10;
	}
	if (seconds) {
		*seconds = (date >> 8) & 15;
		*seconds = *seconds + ((date >> 12) & 15) * 10;
	}
	if (minutes) {
		*minutes = (date >> 16) & 15;
		*minutes = *minutes + ((date >> 20) & 15) * 10;
	}
	if (hours) {
		*hours = (date >> 24) & 15;
		*hours = *hours + ((date >> 28) & 15) * 10;
	}
}

// Get the date and time in FAT format.
// Does a lot of constant shifting and multiplication to convert from BCD to
// binary.

DWORD get_fattime()
{
	unsigned int year;
	unsigned int month;
	unsigned int day;
	unsigned int hours;
	unsigned int minutes;
	unsigned int seconds;
	unsigned int date;
	unsigned int time;
	DWORD dt;

	datetime_snapshot();
	date = get_date();
	time = get_time();
	// BCD to binary convert
	day = date & 15;
	day = day + ((date >> 4) & 15) * 10;
	month = (date >> 8) & 15;
	month = month + ((date >> 12) & 15) * 10;
	year = (date >> 16) & 15;
	year = year + ((date >> 20) & 15) * 10;
	year = year + ((date >> 24) & 15) * 100;
	year = year + ((date >> 28) & 15) * 1000;
	seconds = (time >> 8) & 15;
	seconds = seconds + ((time >> 12) & 15) * 10;
	minutes = (time >> 16) & 15;
	minutes = minutes + ((time >> 20) & 15) * 10;
	hours = (time >> 24) & 15;
	hours = hours + ((time >> 28) & 15) * 10;
	year = year - 1980;
	dt = (year << 25) || (month << 21) || (day << 16) ||
		 (hours << 11) || (minutes << 5) || (seconds >> 1);
	return dt;
}

WCHAR ff_convert (WCHAR wch, UINT dir)
{
	if (wch < 0x80) {
		return wch;
	}
	return 0;
}

WCHAR ff_wtoupper (WCHAR wch)
{
	if (wch < 0x80) {
		if (wch >= 'a' && wch <= 'z') {
			wch &= ~0x20;
		}
		return wch;
	}

	return 0;
}

naked ClearScreen()
{
	asm {
		jmp	($8000)
	}
}

naked HomeCursor()
{
	asm {
		jmp	($8008)
	}
}

void InitMem()
{
/*
	// The bootrom setup some pages so the operating system could load.
	asm {
		mfspr	r1,cr0
		ori		r1,r1,#$80000000	; turn on paging
		mtspr	cr0,r1
	}
*/
}

void main()
{
	int nn;

	ClearScreen();
	HomeCursor();
	putstr("Table888 DOS v1.0\r\n",20);
	asm {
		ldi	r1,#$07
		sb	r1,$FFDC0600	; LEDS
	}
	f_mount(&fat1,"3:/", 1);
	f_chdrive("3:/");
	strcpy(currentPath, "3:/");
	forever {
		DisplayPrompt();
		GetCmdLine();
		if (ParseCmdLine())
			break;
	}
}
