// ============================================================================
//        __
//   \\__/ o\    (C) 2014  Robert Finch, Stratford
//    \  __ /    All rights reserved.
//     \/_//     robfinch<remove>@finitron.ca
//       ||
//
// A64 - Assembler
//  - 64 bit CPU
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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "a64.h"
#include "token.h"
#include "symbol.h"
#include "elf.h"
#include "NameTable.hpp"

#define MAX_PASS  6

int gCpu = 888;
int verbose = 1;
int debug = 1;
int listing = 1;
int binary_out = 1;
int verilog_out = 1;
int elf_out = 1;
int rel_out = 0;
int code_bits = 32;
int data_bits = 32;
int pass;
int lineno;
char *inptr;
char *stptr;
int token;
int phasing_errors;
int bGen = 0;
char fSeg = 0;
int segment;
int segprefix = -1;
int64_t code_address;
int64_t data_address;
int64_t bss_address;
int64_t start_address;
FILE *ofp, *vfp;
int regno;
char current_label[500];
char first_org = 1;

char buf[10000];
char masterFile[10000000];
char segmentFile[10000000];
int NumSections = 12;
clsElf64Section sections[12];
NameTable nmTable;
char codebuf[10000000];
char rodatabuf[10000000];
char databuf[10000000];
char bssbuf[10000000];
char tlsbuf[10000000];
uint8_t binfile[10000000];
int binndx;
int binstart;
int mfndx;
int codendx;
int datandx;
int rodatandx;
int tlsndx;
int bssndx;
SYM *lastsym;

void emitCode(int cd);
void emitAlignedCode(int cd);
void process_shifti(int oc);
void processFile(char *fname, int searchincl);
void bump_address();
extern void Table888_bump_address();
extern void searchenv(char *filename, char *envname, char **pathname);
extern void Table888mmu_processMaster();
extern void Friscv_processMaster();
extern void FISA64_processMaster();

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------

void displayHelp()
{
     printf("a64 [options] file\r\n");
     printf("    +v      = verbose output\r\n");
     printf("    +r      = relocatable output\r\n");
     printf("    -s      = non-segmented\r\n");
     printf("    +g[n]   = cpu version 8=Table888, 9=Table888mmu V=RISCV 6=FISA64\r\n");
     printf("    -o[bvl] = suppress output file b=binary, v=verilog, l=listing\r\n");
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------

int processOptions(int argc, char **argv)
{
    int nn, mm;
    
    nn = 1;
    do {
        if (nn >= argc-1)
           break;
        if (argv[nn][0]=='-') {
           if (argv[nn][1]=='o') {
               mm = 2;
               while(argv[nn][mm] && !isspace(argv[nn][mm])) {
                   if (argv[nn][mm]=='b')
                       binary_out = 0;
                   else if (argv[nn][mm]=='l')
                       listing = 0;
                   else if (argv[nn][mm]=='v')
                       verilog_out = 0;
                   else if (argv[nn][mm]=='e')
                       elf_out = 0;
               }
           }
           if (argv[nn][1]=='s')
               fSeg = 0;
           nn++;
        }
        else if (argv[nn][0]=='+') {
           mm = 2;
           if (argv[nn][1]=='r') {
               rel_out = 1;
           }
           if (argv[nn][1]=='s')
               fSeg = 1;
           if (argv[nn][1]=='g') {
              if (argv[nn][2]=='9') {
                 gCpu=889;
                 fSeg = 1;
              }
              if (argv[nn][2]=='V') {
                 gCpu = 5;
              }
              if (argv[nn][2]=='6') {
                 gCpu = 64;
              }
           }
           nn++;
        }
        else break;
    } while (1);
    return nn;
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------

void emitByte(int64_t cd)
{
     if (segment < 5)
        sections[segment].AddByte(cd);
    if (segment == codeseg || segment == dataseg || segment == rodataseg) {
        binfile[binndx] = cd & 255LL;
        binndx++;
    }
    if (segment==bssseg) {
       bss_address++;
    }
    else
        code_address++;
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------

void emitChar(int64_t cd)
{
     emitByte(cd & 255LL);
     emitByte((cd >> 8) & 255LL);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------

void emitHalf(int64_t cd)
{
     emitChar(cd & 65535LL);
     emitChar((cd >> 16) & 65535LL);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------

void emitWord(int64_t cd)
{
     emitHalf(cd & 0xFFFFFFFFLL);
     emitHalf((cd >> 32) & 0xFFFFFFFFLL);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------

void emitCode(int cd)
{
     emitByte(cd);
}

// ----------------------------------------------------------------------------
// Process a public declaration.
//     public code myfn
// ----------------------------------------------------------------------------

void process_public()
{
    SYM *sym;
    int64_t ca;

    NextToken();
    if (token==tk_code) {
        segment = codeseg;
    }
    else if (token==tk_rodata) {
         segment = rodataseg;
    }
    else if (token==tk_data) {
         segment = dataseg;
    }
    else if (token==tk_bss) {
         segment = bssseg;
    }
    bump_address();
//    if (segment==bssseg)
//        ca = bss_address;
//    else
//        ca = code_address;
    switch(segment) {
    case codeseg:
         ca = code_address;
         ca = sections[0].address;
         break;
    case rodataseg:
         ca = sections[1].address;
         break;
    case dataseg:
         ca = sections[2].address;
         break;
    case bssseg:
         ca = sections[3].address;
         break;
    case tlsseg:
         ca = sections[4].address;
         break;
    }
    NextToken();
    if (token != tk_id) {
        printf("Identifier expected. Token %d\r\n", token);
        printf("Line:%.60s", stptr);
    }
    else {
        sym = find_symbol(lastid);
        if (pass == 3) {
            if (sym) {
                if (sym->defined)
                    printf("Symbol already defined.\r\n");
            }
            else {
                sym = new_symbol(lastid);
            }
            if (sym) {
                sym->defined = 1;
                sym->value = ca;
                sym->segment = segment;
                sym->scope = 'P';
            }
        }
        else if (pass > 3) {
             if (sym->value != ca) {
                phasing_errors++;
                sym->phaserr = '*';
                 if (bGen) printf("%s=%06llx ca=%06llx\r\n", sym->name,  sym->value, code_address);
             }
             else
                 sym->phaserr = ' ';
            sym->value = ca;
        }
        strcpy(current_label, lastid);
    }
    ScanToEOL();
}

// ----------------------------------------------------------------------------
// extern somefn
// ----------------------------------------------------------------------------

void process_extern()
{
    SYM *sym;

    NextToken();
    if (token != tk_id)
        printf("Expecting an identifier.\r\n");
    else {
        sym = find_symbol(lastid);
        if (pass == 3) {
            if (sym) {
            
            }
            else {
                sym = new_symbol(lastid);
            }
            if (sym) {
                sym->defined = 0;
                sym->value = 0;
                sym->segment = segment;
                sym->scope = 'P';
                sym->isExtern = 1;
            }
        }
        else if (pass > 3) {
        }
    }
}

// ----------------------------------------------------------------------------
// .org $23E200
// .org is ignored for relocatable files.
// ----------------------------------------------------------------------------

void process_org()
{
    int64_t new_address;

    NextToken();
    new_address = expr();
    if (!rel_out) {
        if (segment==bssseg || segment==tlsseg) {
            bss_address = new_address;
            sections[segment].address = new_address;
        }
        else {
            if (first_org && segment==codeseg) {
               code_address = new_address;
               start_address = new_address;
               sections[0].address = new_address;
               first_org = 0;
            }
            else {
                while(sections[0].address < new_address)
                    emitByte(0x00);
            }
        }
    }
    ScanToEOL();
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void process_align()
{
    int64_t v;
    
    NextToken();
    v = expr();
    if (segment == codeseg || segment == rodataseg || segment == dataseg || segment==bssseg || segment==tlsseg) {
        while (sections[segment].address % v)
            emitByte(0x00);
    }
//    if (segment==bssseg) {
//        while (bss_address % v)
//            emitByte(0x00);
//    }
//    else {
//        while (code_address % v)
//            emitByte(0x00);
//    }
}

// ----------------------------------------------------------------------------
// code 0x8000 to 0xFFFF
// code 24 bits
// code
// ----------------------------------------------------------------------------

void process_code()
{
    int64_t st, nd;

    segment = codeseg;
    NextToken();
    if (token==tk_eol) {
        prevToken();
        return;
    }
    st = expr();
    if (token==tk_bits) {
        code_bits = st;
        return;
    }
    if (token==tk_to) {
        NextToken();
        nd = expr();
        code_bits = log(nd+1)/log(2);    // +1 to round up a little bit
    }
}

// ----------------------------------------------------------------------------
// data 0x8000 to 0xFFFF
// data 24 bits
// data
// ----------------------------------------------------------------------------

void process_data(int seg)
{
    int64_t st, nd;

    segment = seg;
    NextToken();
    if (token==tk_eol) {
        prevToken();
        return;
    }
    st = expr();
    if (token==tk_bits) {
        data_bits = st;
        return;
    }
    if (token==tk_to) {
        NextToken();
        nd = expr();
        data_bits = log(nd+1)/log(2);    // +1 to round up a little bit
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void process_db()
{
    int64_t val;

    SkipSpaces();
    //NextToken();
    while(token!=tk_eol) {
        SkipSpaces();
        if (*inptr=='\n') break;
        if (*inptr=='"') {
            inptr++;
            while (*inptr!='"') {
                if (*inptr=='\\') {
                    inptr++;
                    switch(*inptr) {
                    case '\\': emitByte('\\'); inptr++; break;
                    case 'r': emitByte(0x13); inptr++; break;
                    case 'n': emitByte(0x0A); inptr++; break;
                    case 'b': emitByte('\b'); inptr++; break;
                    case '"': emitByte('"'); inptr++; break;
                    default: inptr++; break;
                    }
                }
                else {
                    emitByte(*inptr);
                    inptr++;
                }
            }
            inptr++;
        }
        else if (*inptr=='\'') {
            inptr++;
            emitByte(*inptr);
            inptr++;
            if (*inptr!='\'') {
                printf("Missing ' in character constant.\r\n");
            }
        }
        else {
            NextToken();
            val = expr();
            emitByte(val & 255);
            prevToken();
        }
        SkipSpaces();
        if (*inptr!=',')
            break;
        inptr++;
    }
    ScanToEOL();
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void process_dc()
{
    int64_t val;

    SkipSpaces();
    while(token!=tk_eol) {
        SkipSpaces();
        if (*inptr=='"') {
            inptr++;
            while (*inptr!='"') {
                if (*inptr=='\\') {
                    inptr++;
                    switch(*inptr) {
                    case '\\': emitChar('\\'); inptr++; break;
                    case 'r': emitChar(0x13); inptr++; break;
                    case 'n': emitChar(0x0A); inptr++; break;
                    case 'b': emitChar('\b'); inptr++; break;
                    case '"': emitChar('"'); inptr++; break;
                    default: inptr++; break;
                    }
                }
                else {
                    emitChar(*inptr);
                    inptr++;
                }
            }
            inptr++;
        }
        else if (*inptr=='\'') {
            inptr++;
            emitChar(*inptr);
            inptr++;
            if (*inptr!='\'') {
                printf("Missing ' in character constant.\r\n");
            }
        }
        else {
             NextToken();
            val = expr();
            emitChar(val);
            prevToken();
        }
        SkipSpaces();
        if (*inptr!=',')
            break;
        inptr++;
    }
    ScanToEOL();
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void process_dh()
{
    int64_t val;

    SkipSpaces();
    while(token!=tk_eol) {
        SkipSpaces();
        if (*inptr=='"') {
            inptr++;
            while (*inptr!='"') {
                if (*inptr=='\\') {
                    inptr++;
                    switch(*inptr) {
                    case '\\': emitHalf('\\'); inptr++; break;
                    case 'r': emitHalf(0x13); inptr++; break;
                    case 'n': emitHalf(0x0A); inptr++; break;
                    case 'b': emitHalf('\b'); inptr++; break;
                    case '"': emitHalf('"'); inptr++; break;
                    default: inptr++; break;
                    }
                }
                else {
                    emitHalf(*inptr);
                    inptr++;
                }
            }
            inptr++;
        }
        else if (*inptr=='\'') {
            inptr++;
            emitHalf(*inptr);
            inptr++;
            if (*inptr!='\'') {
                printf("Missing ' in character constant.\r\n");
            }
        }
        else {
             NextToken();
            val = expr();
            emitHalf(val);
            prevToken();
        }
        SkipSpaces();
        if (*inptr!=',')
            break;
        inptr++;
    }
    ScanToEOL();
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void process_dw()
{
    int64_t val;

    SkipSpaces();
    while(token!=tk_eol) {
        SkipSpaces();
        if (*inptr=='"') {
            inptr++;
            while (*inptr!='"') {
                if (*inptr=='\\') {
                    inptr++;
                    switch(*inptr) {
                    case '\\': emitWord('\\'); inptr++; break;
                    case 'r': emitWord(0x13); inptr++; break;
                    case 'n': emitWord(0x0A); inptr++; break;
                    case 'b': emitWord('\b'); inptr++; break;
                    case '"': emitWord('"'); inptr++; break;
                    default: inptr++; break;
                    }
                }
                else {
                    emitWord(*inptr);
                    inptr++;
                }
            }
            inptr++;
        }
        else if (*inptr=='\'') {
            inptr++;
            emitWord(*inptr);
            inptr++;
            if (*inptr!='\'') {
                printf("Missing ' in character constant.\r\n");
            }
        }
        else {
             NextToken();
            val = expr();
    // A pointer to an object might be emitted as a data word.
    if (bGen && lastsym)
    if( lastsym->segment < 5)
    sections[segment+7].AddRel(sections[segment].index,((lastsym-syms+1) << 32) | 6 | (lastsym->isExtern ? 128 : 0));
            emitWord(val);
            prevToken();
        }
        SkipSpaces();
        if (*inptr!=',')
            break;
        inptr++;
    }
    ScanToEOL();
}

// ----------------------------------------------------------------------------
// fill.b 252,0x00
// ----------------------------------------------------------------------------

void process_fill()
{
    char sz = 'b';
    int64_t count;
    int64_t val;
    int64_t nn;

    if (*inptr=='.') {
        inptr++;
        if (strchr("bchwBCHW",*inptr)) {
            sz = tolower(*inptr);
            inptr++;
        }
        else
            printf("Illegal fill size.\r\n");
    }
    SkipSpaces();
    NextToken();
    count = expr();
    prevToken();
    need(',');
    NextToken();
    val = expr();
    prevToken();
    for (nn = 0; nn < count; nn++)
        switch(sz) {
        case 'b': emitByte(val); break;
        case 'c': emitChar(val); break;
        case 'h': emitHalf(val); break;
        case 'w': emitWord(val); break;
        }
}

// ----------------------------------------------------------------------------
// Bump up the address to the next aligned code address.
// ----------------------------------------------------------------------------

void bump_address()
{
     if (gCpu==888 || gCpu==889)
        Table888_bump_address();
}

// ----------------------------------------------------------------------------
// label:
// ----------------------------------------------------------------------------

void process_label()
{
    SYM *sym;
    static char nm[500];
    int64_t ca;
    int64_t val;
    int isEquate;
    
    isEquate = 0;
    // Bump up the address to align it with a valid code address if needed.
    bump_address();
    switch(segment) {
    case codeseg:
         ca = code_address;
         ca = sections[0].address;
         break;
    case rodataseg:
         ca = sections[1].address;
         break;
    case dataseg:
         ca = sections[2].address;
         break;
    case bssseg:
         ca = sections[3].address;
         break;
    case tlsseg:
         ca = sections[4].address;
         break;
    }
//    if (segment==bssseg)
//       ca = bss_address;
//    else
//        ca = code_address;
    if (lastid[0]=='.') {
        sprintf(nm, "%s%s", current_label, lastid);
    }
    else { 
        strcpy(current_label, lastid);
        strcpy(nm, lastid);
    }
    NextToken();
//    SkipSpaces();
    if (token==tk_equ || token==tk_eq) {
        NextToken();
        val = expr();
        isEquate = 1;
    }
    else prevToken();
//    if (token==tk_eol)
//       prevToken();
    //else if (token==':') inptr++;
    sym = find_symbol(nm);
    if (pass==3) {
        if (sym) {
            if (sym->defined) {
                if (sym->value != val) {
                    printf("Label %s already defined.\r\n", nm);
                    printf("Line %d: %.60s\r\n", lineno, stptr);
                }
            }
            sym->defined = 1;
            if (isEquate) {
                sym->value = val;
                sym->segment = constseg;
            }
            else {
                sym->value = ca;
                sym->segment = segment;
            }
        }
        else {
            sym = new_symbol(nm);    
            sym->defined = 1;
            if (isEquate) {
                sym->value = val;
                sym->segment = constseg;
            }
            else {
                sym->value = ca;
                sym->segment = segment;
            }
        }
    }
    else if (pass>3) {
         if (!sym) {
            printf("Internal error: SYM is NULL.\r\n");
            printf("Couldn't find <%s>\r\n", nm);
         }
         else {
             if (isEquate) {
                 sym->value = val;
             }
             else {
                 if (sym->value != ca) {
                     phasing_errors++;
                     sym->phaserr = '*';
                     if (bGen) printf("%s=%06llx ca=%06llx\r\n", sym->name,  sym->value, code_address);
                 }
                 else
                     sym->phaserr = ' ';
                 sym->value = ca;
             }
         }
    }
}

// ----------------------------------------------------------------------------
// Group and reorder the segments in the master file.
//     code          placed first
//     rodata        followed by
//     data
//     tls
// ----------------------------------------------------------------------------

void processSegments()
{
    char *pinptr;
    int segment;
    
    if (verbose)
       printf("Processing segments.\r\n");
    inptr = &masterFile[0];
    pinptr = inptr;
    codendx = 0;
    datandx = 0;
    rodatandx = 0;
    tlsndx = 0;
    bssndx = 0;
    memset(codebuf,0,sizeof(codebuf));
    memset(databuf,0,sizeof(databuf));
    memset(rodatabuf,0,sizeof(rodatabuf));
    memset(tlsbuf,0,sizeof(tlsbuf));
    memset(bssbuf,0,sizeof(bssbuf));
    
    while (*inptr) {
        SkipSpaces();
        if (*inptr=='.') inptr++;
        if ((strnicmp(inptr,"code",4)==0) && !isIdentChar(inptr[4])) {
            segment = codeseg;
        }
        else if ((strnicmp(inptr,"data",4)==0) && !isIdentChar(inptr[4])) {
            segment = dataseg;
        }
        else if ((strnicmp(inptr,"rodata",6)==0) && !isIdentChar(inptr[6])) {
            segment = rodataseg;
        }
        else if ((strnicmp(inptr,"tls",3)==0) && !isIdentChar(inptr[3])) {
            segment = tlsseg;
        }
        else if ((strnicmp(inptr,"bss",3)==0) && !isIdentChar(inptr[3])) {
            segment = bssseg;
        }
        ScanToEOL();
        inptr++;
        switch(segment) {
        case codeseg:   
             strncpy(&codebuf[codendx], pinptr, inptr-pinptr);
             codendx += inptr-pinptr;
             break;
        case dataseg:
             strncpy(&databuf[datandx], pinptr, inptr-pinptr);
             datandx += inptr-pinptr;
             break;
        case rodataseg:
             strncpy(&rodatabuf[rodatandx], pinptr, inptr-pinptr);
             rodatandx += inptr-pinptr;
             break;
        case tlsseg:
             strncpy(&tlsbuf[tlsndx], pinptr, inptr-pinptr);
             tlsndx += inptr-pinptr;
             break;
        case bssseg:
             strncpy(&bssbuf[bssndx], pinptr, inptr-pinptr);
             bssndx += inptr-pinptr;
             break;
        }
        pinptr = inptr;
    }
    memset(masterFile,0,sizeof(masterFile));
    strcpy(masterFile, codebuf);
    strcat(masterFile, rodatabuf);
    strcat(masterFile, databuf);
    strcat(masterFile, bssbuf);
    strcat(masterFile, tlsbuf);
    if (debug) {
        FILE *fp;
        fp = fopen("a64-segments.asm", "w");
        if (fp) {
                fwrite(masterFile, 1, strlen(masterFile), fp);
                fclose(fp);
        }
    }
}

// ----------------------------------------------------------------------------
// Look for .include directives and include the files.
// ----------------------------------------------------------------------------

void processLine(char *line)
{
    char *p;
    int quoteType;
    static char fnm[300];
    char *fname;
    int nn;

    p = line;
    while(isspace(*p)) p++;
    if (!*p) goto addToMaster;
    // see if the first thing on the line is an include directive
    if (*p=='.') p++;
    if (strnicmp(p, "include", 7)==0 && !isIdentChar(p[7]))
    {
        p += 7;
        // Capture the file name
        while(isspace(*p)) p++;
        if (*p=='"') { quoteType = '"'; p++; }
        else if (*p=='<') { quoteType = '>'; p++; }
        else quoteType = ' ';
        nn = 0;
        do {
           fnm[nn] = *p;
           p++; nn++;
           if (quoteType==' ' && isspace(*p)) break;
           else if (*p == quoteType) break;
           else if (*p=='\n') break;
        } while(nn < sizeof(fnm)/sizeof(char));
        fnm[nn] = '\0';
        fname = strdup(fnm);
        processFile(fname,1);
        free(fname);
        return;
    }
    // Not an include directive, then just copy the line to the master buffer.
addToMaster:
    strcpy(&masterFile[mfndx], line);
    mfndx += strlen(line);
}

// ----------------------------------------------------------------------------
// Build a aggregate of all the included files into a single master buffer.
// ----------------------------------------------------------------------------

void processFile(char *fname, int searchincl)
{
     FILE *fp;
     char *pathname;

     if (verbose)
        printf("Processing file:%s\r\n", fname);
     pathname = (char *)NULL;
     fp = fopen(fname, "r");
     if (!fp) {
         if (searchincl) {
             searchenv(fname, "INCLUDE", &pathname);
             if (strlen(pathname)) {
                 fp = fopen(pathname, "r");
                 if (fp) goto j1;
             }
         }
         printf("Can't open file <%s>\r\n", fname);
         goto j2;
     }
j1:
     while (!feof(fp)) {
         fgets(buf, sizeof(buf)/sizeof(char), fp);
         processLine(buf);
     }
     fclose(fp);
j2:
     if (pathname)
         free(pathname);
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

int checksum(int32_t *val)
{
    int nn;
    int cs;

    cs = 0;
    for (nn = 0; nn < 32; nn++)
        cs ^= (*val & (1 << nn))!=0;
    return cs;
}


int checksum64(int64_t *val)
{
    int nn;
    int cs;

    cs = 0;
    for (nn = 0; nn < 64; nn++)
        cs ^= (*val & (1LL << nn))!=0;
    return cs;
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void processMaster()
{
    if (gCpu==888)
       Table888_processMaster();
    else if (gCpu==889)
        Table888mmu_processMaster();
    else if (gCpu==64)
        FISA64_processMaster();
    else if (gCpu==5)
        Friscv_processMaster();
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

int64_t Round512(int64_t n)
{
    return (n + 511LL) & 0xFFFFFFFFFFFFFE00LL;
}

int64_t Round4096(int64_t n)
{
    return (n + 4095LL) & 0xFFFFFFFFFFFFF000LL;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void WriteELFFile(FILE *fp)
{
    int nn;
    Elf64Symbol elfsym;
    clsElf64File elf;
    SYM *sym;
    int64_t start;

    sections[0].hdr.sh_name = nmTable.AddName(".text");
    sections[0].hdr.sh_type = clsElf64Shdr::SHT_PROGBITS;
    sections[0].hdr.sh_flags = clsElf64Shdr::SHF_ALLOC | clsElf64Shdr::SHF_EXECINSTR;
    sections[0].hdr.sh_addr = rel_out ? 0 : sections[0].start;
    sections[0].hdr.sh_offset = 512;  // offset in file
    sections[0].hdr.sh_size = sections[0].index;
    sections[0].hdr.sh_link = 0;
    sections[0].hdr.sh_info = 0;
    sections[0].hdr.sh_addralign = 16;
    sections[0].hdr.sh_entsize = 0;

    sections[1].hdr.sh_name = nmTable.AddName(".rodata");
    sections[1].hdr.sh_type = clsElf64Shdr::SHT_PROGBITS;
    sections[1].hdr.sh_flags = clsElf64Shdr::SHF_ALLOC;
    sections[1].hdr.sh_addr = sections[0].hdr.sh_addr + sections[0].index;
    sections[1].hdr.sh_offset = sections[0].hdr.sh_offset + sections[0].index; // offset in file
    sections[1].hdr.sh_size = sections[1].index;
    sections[1].hdr.sh_link = 0;
    sections[1].hdr.sh_info = 0;
    sections[1].hdr.sh_addralign = 8;
    sections[1].hdr.sh_entsize = 0;

    sections[2].hdr.sh_name = nmTable.AddName(".data");
    sections[2].hdr.sh_type = clsElf64Shdr::SHT_PROGBITS;
    sections[2].hdr.sh_flags = clsElf64Shdr::SHF_ALLOC | clsElf64Shdr::SHF_WRITE;
    sections[2].hdr.sh_addr = sections[1].hdr.sh_addr + sections[1].index;
    sections[2].hdr.sh_offset = sections[1].hdr.sh_offset + sections[1].index; // offset in file
    sections[2].hdr.sh_size = sections[2].index;
    sections[2].hdr.sh_link = 0;
    sections[2].hdr.sh_info = 0;
    sections[2].hdr.sh_addralign = 8;
    sections[2].hdr.sh_entsize = 0;

    sections[3].hdr.sh_name = nmTable.AddName(".bss");
    sections[3].hdr.sh_type = clsElf64Shdr::SHT_PROGBITS;
    sections[3].hdr.sh_flags = clsElf64Shdr::SHF_ALLOC | clsElf64Shdr::SHF_WRITE;
    sections[3].hdr.sh_addr = sections[2].hdr.sh_addr + sections[2].index;
    sections[3].hdr.sh_offset = sections[2].hdr.sh_offset + sections[2].index; // offset in file
    sections[3].hdr.sh_size = 0;
    sections[3].hdr.sh_link = 0;
    sections[3].hdr.sh_info = 0;
    sections[3].hdr.sh_addralign = 8;
    sections[3].hdr.sh_entsize = 0;

    sections[4].hdr.sh_name = nmTable.AddName(".tls");
    sections[4].hdr.sh_type = clsElf64Shdr::SHT_PROGBITS;
    sections[4].hdr.sh_flags = clsElf64Shdr::SHF_ALLOC | clsElf64Shdr::SHF_WRITE;
    sections[4].hdr.sh_addr = sections[3].hdr.sh_addr + sections[3].index;;
    sections[4].hdr.sh_offset = sections[2].hdr.sh_offset + sections[2].index; // offset in file
    sections[4].hdr.sh_size = 0;
    sections[4].hdr.sh_link = 0;
    sections[4].hdr.sh_info = 0;
    sections[4].hdr.sh_addralign = 8;
    sections[4].hdr.sh_entsize = 0;

    sections[5].hdr.sh_name = nmTable.AddName(".strtab");
    // The following line must be before the name table is copied to the section.
    sections[6].hdr.sh_name = nmTable.AddName(".symtab");
    sections[7].hdr.sh_name = nmTable.AddName(".reltext");
    sections[8].hdr.sh_name = nmTable.AddName(".relrodata");
    sections[9].hdr.sh_name = nmTable.AddName(".reldata");
    sections[10].hdr.sh_name = nmTable.AddName(".relbss");
    sections[11].hdr.sh_name = nmTable.AddName(".reltls");
    sections[5].hdr.sh_type = clsElf64Shdr::SHT_STRTAB;
    sections[5].hdr.sh_flags = 0;
    sections[5].hdr.sh_addr = 0;
    sections[5].hdr.sh_offset = 512 + sections[0].index + sections[1].index + sections[2].index; // offset in file
    sections[5].hdr.sh_size = nmTable.length;
    sections[5].hdr.sh_link = 0;
    sections[5].hdr.sh_info = 0;
    sections[5].hdr.sh_addralign = 1;
    sections[5].hdr.sh_entsize = 0;
    memcpy(sections[5].bytes, nmTable.text, nmTable.length);

    sections[6].hdr.sh_type = clsElf64Shdr::SHT_SYMTAB;
    sections[6].hdr.sh_flags = 0;
    sections[6].hdr.sh_addr = 0;
    sections[6].hdr.sh_offset = Round512(512 + sections[0].index + sections[1].index + sections[2].index) + nmTable.length; // offset in file
    sections[6].hdr.sh_size = (numsym + 1) * 24;
    sections[6].hdr.sh_link = 5;
    sections[6].hdr.sh_info = 0;
    sections[6].hdr.sh_addralign = 1;
    sections[6].hdr.sh_entsize = 24;

    for(nn = 7; nn < 12; nn++) {
        sections[nn].hdr.sh_type = clsElf64Shdr::SHT_REL;
        sections[nn].hdr.sh_flags = 0;
        sections[nn].hdr.sh_addr = 0;
        sections[nn].hdr.sh_offset = sections[nn-1].hdr.sh_offset + sections[nn-1].hdr.sh_size; // offset in file
        sections[nn].hdr.sh_size = sections[nn].index;
        sections[nn].hdr.sh_link = 6;
        sections[nn].hdr.sh_info = 0;
        sections[nn].hdr.sh_addralign = 1;
        sections[nn].hdr.sh_entsize = 16;
    }

    nn = 1;
    // The first entry is an NULL symbol
    elfsym.st_name = 0;
    elfsym.st_info = 0;
    elfsym.st_other = 0;
    elfsym.st_shndx = 0;
    elfsym.st_value = 0;
    elfsym.st_size = 0;
    sections[6].Add(&elfsym);
    for (nn = 0; nn < numsym; nn++) {
        // Don't output the constants
//        if (syms[nn].segment < 5) {
            elfsym.st_name = syms[nn].name;
            elfsym.st_info = syms[nn].scope == 'P' ? STB_GLOBAL << 4 : 0;
            elfsym.st_other = 0;
            elfsym.st_shndx = syms[nn].segment;
            elfsym.st_value = syms[nn].value;
            elfsym.st_size = 8;
            sections[6].Add(&elfsym);
//        }
    }

    elf.hdr.e_ident[0] = 127;
    elf.hdr.e_ident[1] = 'E';
    elf.hdr.e_ident[2] = 'L';
    elf.hdr.e_ident[3] = 'F';
    elf.hdr.e_ident[4] = clsElf64Header::ELFCLASS64;   // 64 bit file format
    elf.hdr.e_ident[5] = clsElf64Header::ELFDATA2LSB;  // little endian
    elf.hdr.e_ident[6] = 1;        // header version always 1
    elf.hdr.e_ident[7] = 255;      // OS/ABI indentification, 255 = standalone
    elf.hdr.e_ident[8] = 255;      // ABI version
    elf.hdr.e_ident[9] = 0;
    elf.hdr.e_ident[10] = 0;
    elf.hdr.e_ident[11] = 0;
    elf.hdr.e_ident[12] = 0;
    elf.hdr.e_ident[13] = 0;
    elf.hdr.e_ident[14] = 0;
    elf.hdr.e_ident[15] = 0;
    elf.hdr.e_type = rel_out ? 1 : 2;
    elf.hdr.e_machine = 888;         // machine architecture
    elf.hdr.e_version = 1;
    sym = find_symbol("start");
    if (sym)
        start = sym->value;
    else
        start = 0xC00200;
    elf.hdr.e_entry = start;
    elf.hdr.e_phoff = 0;
    elf.hdr.e_shoff = sections[11].hdr.sh_offset + sections[11].index;
    elf.hdr.e_flags = 0;
    elf.hdr.e_ehsize = Elf64HdrSz;
    elf.hdr.e_phentsize = 0;
    elf.hdr.e_phnum = 0;
    elf.hdr.e_shentsize = Elf64ShdrSz;
    elf.hdr.e_shnum = 0;              // This will be incremented by AddSection()
    elf.hdr.e_shstrndx = 5;           // index into section table of string table header

    for (nn = 0; nn < 12; nn++)
        elf.AddSection(&sections[nn]);    
    elf.Write(fp);

}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    int nn,qq;
    static char fname[500];
    char *p;

    ofp = stdout;
    nn = processOptions(argc, argv);
    if (nn > argc-1) {
       displayHelp();
       return 0;
    }
    strcpy(fname, argv[nn]);
    mfndx = 0;
    start_address = 0;
    code_address = 0;
    bss_address = 0;
    for (qq = 0; qq < 12; qq++)
        sections[qq].Clear();
    nmTable.Clear();
    memset(masterFile,0,sizeof(masterFile));
    if (verbose) printf("Pass 1 - collect all input files.\r\n");
    processFile(fname,0);   // Pass 1, collect all include files
    if (debug) {
        FILE *fp;
        fp = fopen("a64-master.asm", "w");
        if (fp) {
                fwrite(masterFile, 1, strlen(masterFile), fp);
                fclose(fp);
        }
    }
    if (verbose) printf("Pass 2 - group and reorder segments\r\n");
    processSegments();     // Pass 2, group and order segments
    
    pass = 3;
    if (verbose) printf("Pass 3 - get all symbols, set initial values.\r\n");
    processMaster();
    pass = 4;
    phasing_errors = 0;
    if (verbose) printf("Pass 4 - assemble code.\r\n");
    processMaster();
    if (verbose) printf("Pass 4: phase errors: %d\r\n", phasing_errors);
    pass = 5;
    while (phasing_errors && pass < 10) {
        phasing_errors = 0;
        processMaster();
        if (verbose) printf("Pass %d: phase errors: %d\r\n", pass, phasing_errors);
        pass++;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // Output listing file.
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    ofp = (FILE *)NULL;
    if (listing) {
        if (verbose) printf("Generating listing file.\r\n");
        strcpy(fname, argv[nn]);
        p = strrchr(fname,'.');
        if (p) {
            *p = '\0';
        }
        strcat(fname, ".lst");
        ofp = fopen(fname,"w");
        if (!ofp)
           printf("Can't open output file <%s>\r\n", fname);
        bGen = 1;
    }
    processMaster();
    DumpSymbols();
    if (listing)
        fclose(ofp);
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // Output binary file.
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (binary_out) {
        if (verbose) printf("Generating binary file.\r\n");
        strcpy(fname, argv[nn]);
        p = strrchr(fname,'.');
        if (p) {
            *p = '\0';
        }
        strcat(fname, ".bin");
        ofp = fopen(fname,"wb");
        if (ofp) {
            fwrite((void*)sections[0].bytes,sections[0].index,1,ofp);
            fwrite((void*)sections[1].bytes,sections[1].index,1,ofp);
            fwrite((void*)sections[2].bytes,sections[2].index,1,ofp);
            //fwrite(binfile,binndx,1,ofp);
            fclose(ofp);    
        }
        else
            printf("Can't create .bin file.\r\n");
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // Output ELF file.
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (elf_out) {
        if (verbose) printf("Generating ELF file.\r\n");
        strcpy(fname, argv[nn]);
        p = strrchr(fname,'.');
        if (p) {
            *p = '\0';
        }
        if (rel_out)
            strcat(fname, ".rel");
        else
            strcat(fname, ".elf");
        ofp = fopen(fname,"wb");
        if (ofp) {
            WriteELFFile(ofp);
            fclose(ofp);    
        }
        else
            printf("Can't create .elf file.\r\n");
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // Output Verilog memory declaration
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (verilog_out) {
        if (verbose) printf("Generating Verilog file.\r\n");
        strcpy(fname, argv[nn]);
        p = strrchr(fname,'.');
        if (p) {
            *p = '\0';
        }
        strcat(fname, ".ver");
        vfp = fopen(fname, "w");
        if (vfp) {
            if (gCpu==64) {
                for (nn = 0; nn < binndx; nn+=8) {
                    fprintf(vfp, "\trommem[%d] = 65'h%01d%02X%02X%02X%02X%02X%02X%02X%02X;\n", 
                        (((start_address+nn)/8)%8192), checksum64((int64_t *)&binfile[nn]),
                        binfile[nn+7], binfile[nn+6], binfile[nn+5], binfile[nn+4], 
                        binfile[nn+3], binfile[nn+2], binfile[nn+1], binfile[nn]);
                }
            }
            else {
                for (nn = 0; nn < binndx; nn+=4) {
                    fprintf(vfp, "\trommem[%d] = 33'h%01d%02X%02X%02X%02X;\n", 
                        (((start_address+nn)/4)%8192), checksum((int32_t *)&binfile[nn]), binfile[nn+3], binfile[nn+2], binfile[nn+1], binfile[nn]);
                }
            }
            fclose(vfp);
        }
        else
            printf("Can't create .ver file.\r\n");
    }
    return 0;
}
