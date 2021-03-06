#define TRUE    1
#define FALSE   0
#define RR      2
#define MTSPR       0x1E
#define MFSPR       0x1F
#define PCTRL       0x37
#define CLI             0x00
#define SEI             0x01
#define WAI             0x03
#define RTD             0x1D
#define RTE             0x1E
#define RTI             0x1F
#define ADD     4
#define SUB     5
#define CMP     6
#define MUL     7
#define DIV     8
#define MOD     9
#define LDI     0x0A
#define AND     12
#define OR      13
#define EOR     14
#define ADDU    0x14
#define SUBU    0x15
#define CMPU    0x16
#define MULU    0x17
#define DIVU    0x18
#define MODU    0x19
#define Bcc     0x3D
#define BEQ         0
#define BNE         1
#define BGT         2
#define BGE         3
#define BLT         4
#define BLE         5
#define RTL     0x37
#define BRK     0x38
#define BSR     0x39
#define BRA     0x3A
#define RTS     0x3B
#define JALI    0x3E
#define NOP     0x3F
#define LB      0x40
#define LBU     0x41
#define LC      0x42
#define LCU     0x43
#define LH      0x44
#define LHU     0x45
#define LW      0x46
#define LEA     0x47
#define LBX     0x48
#define LBUX    0x49
#define LCX     0x4A
#define LCUX    0x4B
#define LHX     0x4C
#define LHUX    0x4D
#define LWX     0x4E
#define LEAX    0x4F
#define POP     0x57
#define LWAR    0x5C
#define SB      0x60
#define SC      0x61
#define SH      0x62
#define SW      0x63
#define INC     0x64
#define PUSH    0x67
#define SBX     0x68
#define SCX     0x69
#define SHX     0x6A
#define SWX     0x6B
#define SWCR    0x6E
#define IMM     0x7C

extern pascal putstr2(char *);

void SetNormAttr(unsigned short int attr)
{
     asm {
         push  r6
         lw    r1,24[bp]
         ldi   r6,#$22
         sys   #410
         pop   r6
     }
}

unsigned short int GetNormAttr()
{
     asm {
         push  r6
         ldi   r6,#$23
         sys   #410
         pop   r6
     }
}

void reverse_video() {
    unsigned short int attr;
    attr = GetNormAttr();
    attr = ((attr >> 10) << 19) | ((attr >> 19) << 10);
    SetNormAttr(attr);
}

void DumpInsnBytes(unsigned int ad, unsigned int ir)
{
     printf("%06X %02X %02X %02X %02X\t", ad, (ir & 0xff), ((ir >> 8) & 0xFF), ((ir >> 16) & 0xFF),((ir>>24)&0xFF));
}

static void DispRst(ir) {
    printf("r%d", ((ir >> 12) & 0x1F));
}

static void DispRstc(ir) {
    printf("R%d,", ((ir >> 12) & 0x1F));
}

static void DispRac(ir) {
    printf("R%d,", ((ir >> 7) & 0x1F));
}

static void DispRa(ir) {
    printf("R%d", ((ir >> 7) & 0x1F));
}

static void DispRb(ir) {
    printf("R%d", ((ir >> 17) & 0x1F));
}

static void DispSpr(int ir)
{
    int nn;
    
    nn = (ir >> 17) & 0xFF;
    switch(nn) {
    case 0: printf("CR0"); break;
    case 3: printf("CR3"); break;
    case 4: printf("TICK"); break;
    case 6: printf("CLK"); break;
    case 7: printf("DPC"); break;
    case 8: printf("IPC"); break;
    case 9: printf("EPC"); break;
    case 10: printf("VBR"); break;
    case 50: printf("DBAD0"); break;
    case 51: printf("DBAD1"); break;
    case 52: printf("DBAD2"); break;
    case 53: printf("DBAD3"); break;
    case 54: printf("DBCTRL"); break;
    case 55: printf("DBSTAT"); break;
    default: printf("SPR%d", nn);
    }
}

static void DispMemAddress(hasPrefix, prefix, ir)
{
    short int sir;

    sir = ir;
    if (hasPrefix) {
        printf("$%X", (prefix << 15)|(ir>>17));
    }
    else
        printf("$%X", (sir >> 17));
    if (((ir >> 7) & 0x1F) != 0)
        printf("[R%d]\r\n",((ir >> 7) & 0x1F));
    else
        printf("\r\n");
}

static void DispInc(hasPrefix, prefix, ir)
{
    short int sir;
    int Ra;
    int imm;
    
    sir = ir;
    Ra = (ir >> 7) & 0x1F;
    imm = (ir >> 12) & 0x1f;
    if ((imm & 0x10)==0x10) {
       imm |= 0xFFFFFFFFFFFFFFF0L;
       imm = -imm;
       printf("DEC   ");
    }
    else
       printf("INC   ");
    if (hasPrefix) {
        printf("$%X", (prefix << 15)|(ir>>17));
    }
    else
        printf("$%X", (sir >> 17));
    if (Ra == 0)
        printf(",");
    else
        printf("[R%d],",Ra);
    printf("#%d\r\n", imm);
}

static void PrintSc(sc)
{
    if (sc > 1)
       printf("*%d]\r\n", sc);
    else
        printf("]\r\n");
}

static DispBrk(unsigned short int ir)
{
    int type;
    int vecno;

    type = (ir >> 30) & 3;
    vecno = (ir >> 17) & 0x1ff;
    switch(type) {
    case 0: printf("SYS   #%X\r\n", vecno);
    case 1: printf("DBG   #%X\r\n", vecno);
    case 2: printf("INT   #%X\r\n", vecno);
    case 3: printf("BRK?  #%X\r\n", vecno);
    }
}

static void DispIndexedAddr(unsigned int ad, char *mne, unsigned short int ir)
{
    int offset;
    int Ra, Rb, Rst;
    int sc;
    
    offset = ir >> 24;
    Ra = (ir >> 7) & 0x1F;
    Rst = (ir >> 12) & 0x1F;
    Rb = (ir >> 17) & 0x1F;
    sc = (ir >> 22) & 3;
    sc = 1 << sc;

    DumpInsnBytes(ad, ir);
    printf("%s R%d,", mne, Rst);
    if (offset != 0)
        printf("$%X", offset); 
    if (Ra != 0 && Rb != 0) {
        printf("[R%d+R%d");
        PrintSc(sc);
    }
    else if (Ra==0) {
         printf("[R%d",Rb);
         PrintSc(sc);
    }
    else if (Rb==0) {
         printf("[R%d]\r\n");
    }
}

static void DispLS(int ad, char *mne, int hasPrefix, int prefix, int ir)
{
    DumpInsnBytes(ad, ir);
    printf("%s ", mne);
    DispRstc(ir);
    DispMemAddress(hasPrefix, prefix, ir);
}
static void DispRI(int ad, char *mne, int hasPrefix, int prefix, int ir)
{
    short int sir;

    sir = ir;
    DumpInsnBytes(ad, ir);
    printf("%s ", mne);
    DispRstc(ir);
    DispRac(ir);
    if (hasPrefix)
        printf("#$%X\r\n", (prefix << 15)|(ir>>17));
    else
        printf("#$%X\r\n", (sir >> 17));
}

void DispBcc(int ad, char *mne, int ir)
{
     int nn;
     int disp;

     disp = ((ir >> 17) & 0x7fff) << 2;
     if (ir & 0x80000000)
         disp |= 0xFFFFFFFFFFFF0000L;
     DumpInsnBytes(ad, ir);
     printf("%s ", mne);
     DispRac(ir);
     printf("%06X\r\n", ad + disp);
}
void DispRR(int ad, char *mne, unsigned short int ir)
{
     int oc;
     int Rb;
     
     oc = ir >> 25;
     Rb = (ir >> 17) & 0x1F;
     DumpInsnBytes(ad, ir);
     if (oc==0x0D && Rb==0) {
         printf("MOV   ");
         DispRstc(ir);
         DispRa(ir);
         printf("\r\n");
         return;
     }
     printf("%s ", mne);
     DispRstc(ir);
     DispRac(ir);
     DispRb(ir);
     printf("\r\n");
}
 
void DispJALI(int ad, unsigned short int ir, int hasPrefix, int prefix)
{
     int Rt;
     int Ra;
     short int sir;

     sir = ir;     
     Rt = (ir >> 12) & 0x1F;
     Ra = (ir >> 7) & 0x1F;
     DumpInsnBytes(ad, ir);
     if (Rt != 0) {
         if (Rt != 31) {
             printf("JAL   ");
             DispRstc(ir);
             printf("(");
         }
         else
             printf("JSR   (");
     }
     else
         printf("JMP   (");
     if (hasPrefix)
         printf("$%X", (prefix << 15)|(ir>>17));
     else {
         if ((sir >> 17) != 0)
             printf("$%X", (sir >> 17));
     }
     if (Ra != 0)
         printf("[R%d]", Ra);
     printf(")\r\n");
}

void disassem(unsigned int *ad, unsigned int hilite_ad)
{
    unsigned short int *mem;
    unsigned short int ir;
    unsigned int opcode;
    unsigned int funct;
    int hasPrefix;
    int prefix;
    int nn;
    int disp;
    unsigned int ad1;
    short int sir;
    int isFirstPrefix;
    int isRV;

    mem = (unsigned short int *)0;
    hasPrefix = FALSE;
    prefix = 0;
    isFirstPrefix = TRUE;
    isRV = FALSE;

    do {
        if ((*ad)==hilite_ad) {
            reverse_video();
            isRV = TRUE;
        }
        ad1 = (*ad) >> 2;
        ir = mem[ad1];
        sir = ir;
        opcode = ir & 0x7F;
        funct = (ir >> 25) & 0x7F;
        switch(opcode) {
        case IMM:
             hasPrefix = TRUE;
             if (isFirstPrefix) {
                 prefix = sir >> 7;
                 if (prefix & 0x1000000)
                     prefix |= 0xFFFFFFFFFF000000L;
             }
             else
                 prefix = (prefix << 25) | (ir >> 7);
             DumpInsnBytes(*ad, ir);
             printf("IMM\r\n");
             isFirstPrefix = FALSE;
             break;
        case RR:
             switch (funct) {
             case PCTRL:
                  switch((ir >> 17) & 0x1f) {
                  case CLI:  DumpInsnBytes(*ad,ir); printf("CLI\r\n"); break;
                  case SEI:  DumpInsnBytes(*ad,ir); printf("SEI\r\n"); break;
                  case WAI:  DumpInsnBytes(*ad,ir); printf("WAI\r\n"); break;
                  case RTD:  DumpInsnBytes(*ad,ir); printf("RTD\r\n"); break;
                  case RTE:  DumpInsnBytes(*ad,ir); printf("RTE\r\n"); break;
                  case RTI:  DumpInsnBytes(*ad,ir); printf("RTI\r\n"); break;
                  default:   DumpInsnBytes(*ad,ir); printf("???\r\n"); break;
                  }
                  break;
             case ADD: DispRR(*ad, "ADD  ", ir); break;
             case ADDU: DispRR(*ad, "ADDU ", ir); break; 
             case SUB: DispRR(*ad, "SUB  ", ir); break;
             case SUBU: DispRR(*ad, "SUBU ", ir); break; 
             case CMP: DispRR(*ad, "CMP  ", ir); break;
             case CMPU: DispRR(*ad, "CMPU ", ir); break; 
             case MUL: DispRR(*ad, "MUL  ", ir); break;
             case MULU: DispRR(*ad, "MULU ", ir); break; 
             case DIV: DispRR(*ad, "DIV  ", ir); break;
             case DIVU: DispRR(*ad, "DIVU ", ir); break; 
             case AND: DispRR(*ad, "AND  ", ir); break;
             case OR: DispRR(*ad, "OR   ", ir); break;
             case EOR: DispRR(*ad, "EOR  ", ir); break;
             case MFSPR:
                  DumpInsnBytes(*ad, ir);
                  printf("MFSPR ");
                  DispRstc(ir);
                  DispSpr(ir);
                  printf("\r\n");
                  break;
             case MTSPR:
                  DumpInsnBytes(*ad, ir);
                  printf("MTSPR ");
                  DispSpr(ir);
                  printf(",");
                  DispRa(ir);
                  printf("\r\n");
                  break;
             }
             break;
        case ADD:  DispRI(*ad, "ADD  ", hasPrefix, prefix, ir); break;
        case ADDU:  DispRI(*ad, "ADDU ", hasPrefix, prefix, ir); break;
        case SUB:  DispRI(*ad, "SUB  ", hasPrefix, prefix, ir); break;
        case SUBU:  DispRI(*ad, "SUBU ", hasPrefix, prefix, ir); break;
        case CMP:  DispRI(*ad, "CMP  ", hasPrefix, prefix, ir); break;
        case CMPU:  DispRI(*ad, "CMPU ", hasPrefix, prefix, ir); break;
        case MUL:  DispRI(*ad, "MUL  ", hasPrefix, prefix, ir); break;
        case MULU:  DispRI(*ad, "MULU ", hasPrefix, prefix, ir); break;
        case DIV:  DispRI(*ad, "DIV  ", hasPrefix, prefix, ir); break;
        case DIVU:  DispRI(*ad, "DIVU ", hasPrefix, prefix, ir); break;
        case AND:  DispRI(*ad, "AND  ", hasPrefix, prefix, ir); break;
        case OR:  DispRI(*ad, "OR   ", hasPrefix, prefix, ir); break;
        case EOR:  DispRI(*ad, "EOR  ", hasPrefix, prefix, ir); break;
        case Bcc:
             switch((ir >> 12) & 7) {
             case BEQ:  DispBcc(*ad, "BEQ  ", ir); break;
             case BNE:  DispBcc(*ad, "BNE  ", ir); break;
             case BLT:  DispBcc(*ad, "BLT  ", ir); break;
             case BLE:  DispBcc(*ad, "BLE  ", ir); break;
             case BGT:  DispBcc(*ad, "BGT  ", ir); break;
             case BGE:  DispBcc(*ad, "BGE  ", ir); break;
             case 6:
             case 7:    DispBcc(*ad, "???  ", ir); break;
             }
             break;
        case JALI: DispJALI(*ad, ir, hasPrefix, prefix); break;
        case BRK:  DispBrk(ir); break;
        case BSR:
             DumpInsnBytes(*ad, ir);
             disp = ir >> 7;
             if (ir & 0x80000000) disp |= 0xFFFFFFFFFF000000L;
             nn = (*ad) + (disp << 2);
             printf("BSR   $%X\r\n", nn);
             break;
        case BRA:
             DumpInsnBytes(*ad, ir);
             disp = ir >> 7;
             if (ir & 0x80000000) disp |= 0xFFFFFFFFFF000000L;
             nn = (*ad) + (disp << 2);
             printf("BRA   $%X\r\n", nn);
             break;
        case RTL:
             DumpInsnBytes(*ad, ir);
             nn = ir >> 17;
             printf("RTL   #%X\r\n",nn);
             break;
        case RTS:
             DumpInsnBytes(*ad, ir);
             nn = ir >> 17;
             printf("RTS   #%X\r\n",nn);
             break;
        case LB: DispLS(*ad, "LB   ", hasPrefix, prefix, ir); break;
        case LBU: DispLS(*ad, "LBU  ", hasPrefix, prefix, ir); break;
        case LC: DispLS(*ad, "LC   ", hasPrefix, prefix, ir); break;
        case LCU: DispLS(*ad, "LCU  ", hasPrefix, prefix, ir); break;
        case LH: DispLS(*ad, "LH   ", hasPrefix, prefix, ir); break;
        case LHU: DispLS(*ad, "LHU  ", hasPrefix, prefix, ir); break;
        case LW: DispLS(*ad, "LW   ", hasPrefix, prefix, ir); break;
        case LEA: DispLS(*ad, "LEA  ", hasPrefix, prefix, ir); break;
        case LBX: DispIndexedAddr(*ad, "LB   ", ir); break;
        case LBUX: DispIndexedAddr(*ad, "LBU  ", ir); break;
        case LCX: DispIndexedAddr(*ad, "LC   ", ir); break;
        case LCUX: DispIndexedAddr(*ad, "LCU  ", ir); break;
        case LHX: DispIndexedAddr(*ad, "LH   ", ir); break;
        case LHUX: DispIndexedAddr(*ad, "LHU  ", ir); break;
        case LWX: DispIndexedAddr(*ad, "LW   ", ir); break;
        case LEAX: DispIndexedAddr(*ad, "LEA  ", ir); break;
        case INC: DumpInsnBytes(*ad, ir); DispInc(hasPrefix, prefix, ir); break;
        case SB: DispLS(*ad, "SB   ", hasPrefix, prefix, ir); break;
        case SC: DispLS(*ad, "SC   ", hasPrefix, prefix, ir); break;
        case SH: DispLS(*ad, "SH   ", hasPrefix, prefix, ir); break;
        case SW: DispLS(*ad, "SW   ", hasPrefix, prefix, ir); break;
        case SBX: DispIndexedAddr(*ad, "SB   ", ir); break;
        case SCX: DispIndexedAddr(*ad, "SC   ", ir); break;
        case SHX: DispIndexedAddr(*ad, "SH   ", ir); break;
        case SWX: DispIndexedAddr(*ad, "SW   ", ir); break;
        case LWAR: DispLS(*ad, "LWAR ", hasPrefix, prefix, ir); break;
        case SWCR: DispLS(*ad, "SWCR ", hasPrefix, prefix, ir); break;
        case PUSH: DumpInsnBytes(*ad, ir); printf("PUSH  "); DispRa(ir); printf("\r\n"); break;
        case POP:  DumpInsnBytes(*ad, ir); printf("POP   "); DispRst(ir); printf("\r\n"); break;
        case NOP: DumpInsnBytes(*ad, ir); printf("NOP\r\n"); break;
        case LDI:
             DumpInsnBytes(*ad, ir); 
             printf("LDI   ");
             DispRstc(ir);
             if (hasPrefix)
                 printf("#$%X\r\n", (prefix << 15)|(ir>>17));
             else
                 printf("#$%X\r\n", (sir >> 17));
             break;
             
        default: DumpInsnBytes(*ad, ir); printf("?????\r\n"); break;
        }
        *ad = *ad + 4;
        if (isRV) {
           reverse_video();
           isRV = FALSE;
        }
    } while (opcode==IMM);
}

void disassem20(unsigned int ad, unsigned int hilite_ad)
{
    int nn;
    unsigned int ad1;

    printf("Disassem:\r\n");
    for (nn = 0; nn < 16; nn++) {
        disassem(&ad, hilite_ad);
    }
}

