// ============================================================================
//        __
//   \\__/ o\    (C) 2012,2013  Robert Finch, Stratford
//    \  __ /    All rights reserved.
//     \/_//     robfinch<remove>@finitron.ca
//       ||
//
// C64 - 'C' derived language compiler
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
/*
 *	68000 C compiler
 *
 *	Copyright 1984, 1985, 1986 Matthew Brandt.
 *  all commercial rights reserved.
 *
 *	This compiler is intended as an instructive tool for personal use. Any
 *	use for profit without the written consent of the author is prohibited.
 *
 *	This compiler may be distributed freely for non-commercial use as long
 *	as this notice stays intact. Please forward any enhancements or questions
 *	to:
 *
 *		Matthew Brandt
 *		Box 920337
 *		Norcross, Ga 30092
 */
#include        <stdio.h>
#include <string.h>
#include        "c.h"
#include        "expr.h"
#include        "gen.h"
#include        "cglbdec.h"

void put_mask(int mask);
void align(int n);

/*      variable initialization         */

enum e_gt { nogen, bytegen, chargen, halfgen, wordgen, longgen };
//enum e_sg { noseg, codeseg, dataseg, bssseg, idataseg };

int	       gentype = nogen;
int	       curseg = noseg;
int        outcol = 0;

struct oplst {
        char    *s;
        int     ov;
        }       opl[] =
{       {"move",op_move}, {"addu",op_add}, {"addu", op_addu}, {"mov", op_mov}, {"mtspr", op_mtspr}, {"mfspr", op_mfspr},
		{"ldi",op_ldi},
		{"add",op_addi}, {"subu",op_sub}, {"subu", op_subu},
                {"subi",op_subi}, {"and",op_and},
		{"sext8",op_sext8}, {"sext16", op_sext16}, {"sext32", op_sext32},
		{"subui",op_subui}, {"shru", op_shru}, {"divsi", op_divsi}, {"not", op_not},
		{"addui",op_addui},
		{"shr", op_shr}, {"dw", op_dw}, {"shl", op_shl}, {"shr", op_shr}, {"shru", op_shru},
		{"shlu", op_shlu}, {"shlui", op_shlui},
		{"shli", op_shli}, {"shri", op_shri}, {"shrui", op_shrui},
		{"bfext", op_bfext}, {"bfextu", op_bfextu}, {"bfins", op_bfins},
		{"sw", op_sw}, {"lw", op_lw}, {"lh", op_lh}, {"lc", op_lc}, {"lb", op_lb},
		{"lbu", op_lbu}, {"lcu", op_lcu}, {"lhu", op_lhu}, {"sti", op_sti},

		{"ldis", op_ldis}, {"lws", op_lws}, {"sws", op_sws},
		{"lm", op_lm}, {"sm",op_sm}, {"sb",op_sb}, {"sc",op_sc}, {"sh",op_sh},
		{"call", op_call}, {"ret", op_ret}, {"loop", op_loop}, {"beqi", op_beqi},
		{"jal", op_jal}, {"jsr", op_jsr}, {"rts", op_rts},
		{"rti", op_rti},
		{"lwr", op_lwr}, {"swc", op_swc}, {"cache",op_cache},
		{"or",op_or}, {"ori",op_ori}, {"iret", op_iret}, {"andi", op_andi},
		{"xor",op_xor}, {"xori", op_xori}, {"mul",op_mul}, {"muli", op_muli}, {"mului", op_mului}, 
		{"fdmul", op_fdmul}, {"fddiv", op_fddiv}, {"fdadd", op_fdadd}, {"fdsub", op_fdsub},
		{"fsmul", op_fsmul}, {"fsdiv", op_fsdiv}, {"fsadd", op_fsadd}, {"fssub", op_fssub},
		{"fs2d", op_fs2d}, {"fi2d", op_i2d},
		{"divs",op_divs}, {"swap",op_swap}, {"mod", op_mod}, {"modu", op_modu},
		{"eq",op_eq}, {"bnei", op_bnei}, {"sei", op_sei},
		{"ltu", op_ltu}, {"leu",op_leu}, {"gtu",op_gtu}, {"geu", op_geu},
                {"bhi",op_bhi}, {"bhs",op_bhs}, {"blo",op_blo},
                {"bls",op_bls}, {"mulu",op_mulu}, {"divu",op_divu},
                {"ne",op_ne}, {"lt",op_lt}, {"le",op_le},
		{"gt",op_gt}, {"ge",op_ge}, {"neg",op_neg}, {"fdneg", op_fdneg}, {"nr", op_nr},
		{"not",op_not}, {"cmp",op_cmp}, {"ext",op_ext}, 
		{"jmp",op_jmp},
                {"lea",op_lea}, {"asr",op_asr}, 
                {"clr",op_clr}, {"link",op_link}, {"unlk",op_unlk},
                {"bra",op_bra}, {"pea",op_pea},
				{"cmp",op_cmpi}, {"tst",op_tst},
		{"stop", op_stop},
		{"bmi", op_bmi}, {"outb", op_outb}, {"inb", op_inb}, {"inbu", op_inbu},
				{"dc",op_dc},
		{"",op_empty}, {"",op_asm},
                {0,0} };

static char *pad(char *op)
{
	static char buf[20];
	int n;

	n = strlen(op);
	strncpy(buf,op,20);
	if (n < 5) {
		strcat(buf, "     ");
		buf[5] = '\0';
	}
	return buf;
}

char *opstr(int op)
{
	int     i;
    i = 0;
    while( opl[i].s )
    {
		if( opl[i].ov == op )
		{
			return (opl[i].s);
		}
		++i;
    }
	return NULL;
}

void putop(int op)
{    
	int     i;
    i = 0;
    while( opl[i].s )
    {
		if( opl[i].ov == op )
		{
			fprintf(output,"%s",pad(opl[i].s));
			return;
		}
		++i;
    }
    printf("DIAG - illegal opcode.\n");
}

static void PutConstant(ENODE *offset)
{
	switch( offset->nodetype )
	{
	case en_autofcon:
			fprintf(output,"%I64d",offset->i);
			break;
	case en_fcon:
			fprintf(output,"0x%I64x",offset->f);
			break;
	case en_autocon:
	case en_icon:
			fprintf(output,"%I64d",offset->i);
			break;
	case en_labcon:
			fprintf(output,"%s_%I64d",GetNamespace(),offset->i);
			break;
	case en_clabcon:
			fprintf(output,"%s_%I64d",GetNamespace(),offset->i);
			break;
	case en_nacon:
	case en_cnacon:
			fprintf(output,"%s",offset->sp);
			break;
	case en_add:
			PutConstant(offset->p[0]);
			fprintf(output,"+");
			PutConstant(offset->p[1]);
			break;
	case en_sub:
			PutConstant(offset->p[0]);
			fprintf(output,"-");
			PutConstant(offset->p[1]);
			break;
	case en_uminus:
			fprintf(output,"-");
			PutConstant(offset->p[0]);
			break;
	default:
			printf("DIAG - illegal constant node.\n");
			break;
	}
}

char *RegMoniker(int regno)
{
	static char buf[4][20];
	static int n;

	n = (n + 1) & 3;
	switch(regno) {
	case 254:	sprintf(&buf[n], "bp"); break;	// was 27
	case 28:	sprintf(&buf[n], "xlr"); break;
	case 29:	sprintf(&buf[n], "pc"); break;
	case 255:	sprintf(&buf[n], "sp"); break;	// was 30
	default:	sprintf(&buf[n], "r%d", regno); break;
	}
	return &buf[n];
}

char *BrRegMoniker(int regno)
{
	static char buf[4][20];
	static int n;

	n = (n + 1) & 3;
	switch(regno) {
	case  1:	sprintf(&buf[n], "lr"); break;
	case 11:	sprintf(&buf[n], "xlr"); break;
	case 15:	sprintf(&buf[n], "pc"); break;
	default:	sprintf(&buf[n], "br%d", regno); break;
	}
	return &buf[n];
}

char *PredRegMoniker(int regno)
{
	static char buf[4][20];
	static int n;

	n = (n + 1) & 3;
	sprintf(&buf[n], "p%d", regno);
	return &buf[n];
}

char *PredOpStr(int op)
{
	switch(op)
	{
	case 2: return "eq";
	case 3: return "ne";
	case 4: return "le";
	case 5: return "gt";
	case 6: return "ge";
	case 7: return "lt";
	case 8: return "leu";
	case 9: return "gtu";
	case 10: return "geu";
	case 11: return "ltu";
	}
	return "<bad op>";
}

int PredOp(int op)
{
	switch(op) {
//		case op_f:	return 0;
//		case op_t:	return 1;
		case op_eq:	return 2;
		case op_ne:	return 3;
		case op_le: return 4;
		case op_gt: return 5;
		case op_ge: return 6;
		case op_lt: return 7;
		case op_leu:	return 8;
		case op_gtu:	return 9;
		case op_geu:	return 10;
		case op_ltu:	return 11;
		default: return 1;
	}
}

int InvPredOp(int op)
{
	switch(op) {
//		case op_f:	return 0;
//		case op_t:	return 1;
		case 2:	return 3;
		case 3:	return 2;
		case 4: return 5;
		case 5: return 4;
		case 6: return 7;
		case 7: return 6;
		case 8:	return 9;
		case 9:	return 8;
		case 10:	return 11;
		case 11:	return 10;
		default: return 1;
	}
}

void PutAddressMode(AMODE *ap)
{
	switch( ap->mode )
    {
    case am_immed:
		fprintf(output,"#");
    case am_direct:
            PutConstant(ap->offset);
            break;
    case am_breg:
			fprintf(output, "%s", BrRegMoniker(ap->preg));
            break;
    case am_reg:
			fprintf(output, "%s", RegMoniker(ap->preg));
            break;
    case am_predreg:
			fprintf(output, "%s", PredRegMoniker(ap->preg));
            break;
    case am_ind:
			//if (ap->offset != NULL) {
			//	if (ap->offset->i != 0)
			//		fprintf(output, "%I64d[r%d]", ap->offset->i, ap->preg);
			//	else
			//		fprintf(output,"[r%d]",ap->preg);
			//}
			//else
			if (ap->offset != NULL)
				if (ap->offset->i)
					fprintf(output, "%I64d", ap->offset->i);
				fprintf(output,"[%s]",RegMoniker(ap->preg));
			break;
	case am_brind:
			fprintf(output,"[%s]",BrRegMoniker(ap->preg));
			break;
    case am_ainc:
            fprintf(output,"******[r%d]",ap->preg);
			fprintf(output,"addui\ta%d,a%d,#",ap->preg,ap->preg);
            break;
    case am_adec:
			fprintf(output,"subui\ta%d,a%d,#",ap->preg,ap->preg);
            fprintf(output,"******[a%d]",ap->preg);
            break;
    case am_indx:
			if (ap->offset->i != 0)
				PutConstant(ap->offset);
            fprintf(output,"[%s]",RegMoniker(ap->preg));
            break;
    case am_indx2:
			if (ap->offset->i != 0)
				PutConstant(ap->offset);
			if (ap->scale==1)
	            fprintf(output,"[%s+%s]",RegMoniker(ap->sreg),RegMoniker(ap->preg));
			else
		        fprintf(output,"[%s+%s*%d]",RegMoniker(ap->sreg),RegMoniker(ap->preg),ap->scale);
            break;
    case am_indx3:
			if (ap->offset->i != 0)
	            PutConstant(ap->offset);
            fprintf(output,"[%s+%s]",RegMoniker(ap->sreg),RegMoniker(ap->preg));
            break;
    case am_mask:
            put_mask(ap->offset);
            break;
    default:
            printf("DIAG - illegal address mode.\n");
            break;
    }
}

/*
 *      output a generic instruction.
 */
//void put_code(int op, int len,AMODE *aps,AMODE *apd,AMODE *ap3,AMODE *ap4)
void put_code(struct ocode *p)
{
	int op = p->opcode;
	AMODE *aps,*apd,*ap3,*ap4;
	int pop = p->predop;
	int predreg = p->pregreg;
	int len = p->length;
	aps = p->oper1;
	apd = p->oper2;
	ap3 = p->oper3;
	ap4 = p->oper4;

	if( op == op_dc )
		{
		switch( len )
			{
			case 1: fprintf(output,"\tdb"); break;
			case 2: fprintf(output,"\tdc"); break;
			case 4: fprintf(output,"\tdh"); break;
			case 8: fprintf(output,"\tdw"); break;
			}
		}
	else
		{
			fprintf(output, "\t");
			if (pop!=1)
				fprintf(output, "p%d.%s\t",predreg,PredOpStr(pop));
			else
				fprintf(output, "%6.6s\t", "");
			putop(op);
		}
        if( aps != 0 )
        {
                fprintf(output,"\t");
				PutAddressMode(aps);
                if( apd != 0 )
                {
                        fprintf(output,",");
                       	PutAddressMode(apd);
						if (ap3 != NULL) {
							fprintf(output,",");
							PutAddressMode(ap3);
							if (ap4 != NULL) {
								fprintf(output,",");
								PutAddressMode(ap4);
							}
						}
                }
        }
        fprintf(output,"\n");
}

/*
 *      generate a register mask for restore and save.
 */
void put_mask(int mask)
{
	int nn;
	int first = 1;

	for (nn = 0; nn < 32; nn++) {
		if (mask & (1<<nn)) {
			if (!first)
				fprintf(output,"/");
			fprintf(output,"r%d",nn);
			first = 0;
		}
	}
//	fprintf(output,"#0x%04x",mask);

}

/*
 *      generate a register name from a tempref number.
 */
void putreg(int r)
{
	fprintf(output, "r%d", r);
}

/*
 *      generate a named label.
 */
void gen_strlab(char *s)
{       fprintf(output,"%s:\n",s);
}

/*
 *      output a compiler generated label.
 */
void put_label(int lab, char *nm, char *ns, char d)
{
	if (nm==NULL)
		fprintf(output,"%s_%d:\n",ns,lab);
	else if (strlen(nm)==0)
		fprintf(output,"%s_%d:\n",ns,lab);
	else
		fprintf(output,"%s_%d:	; %s\n",ns,lab,nm);
}

void GenerateByte(int val)
{
	if( gentype == bytegen && outcol < 60) {
        fprintf(output,",%d",val & 0x00ff);
        outcol += 4;
    }
    else {
        nl();
        fprintf(output,"\tdb\t%d",val & 0x00ff);
        gentype = bytegen;
        outcol = 19;
    }
}

void GenerateChar(int val)
{
	if( gentype == chargen && outcol < 60) {
        fprintf(output,",%d",val & 0xffff);
        outcol += 6;
    }
    else {
        nl();
        fprintf(output,"\tdc\t%d",val & 0xffff);
        gentype = chargen;
        outcol = 21;
    }
}

void genhalf(int val)
{
	if( gentype == halfgen && outcol < 60) {
        fprintf(output,",%d",val & 0xffffffff);
        outcol += 10;
    }
    else {
        nl();
        fprintf(output,"\tdh\t%d",val & 0xffffffff);
        gentype = halfgen;
        outcol = 25;
    }
}

void GenerateWord(__int64 val)
{
	if( gentype == wordgen && outcol < 58) {
        fprintf(output,",%I64d",val);
        outcol += 18;
    }
    else {
        nl();
        fprintf(output,"\tdh\t%I64d",val);
        gentype = wordgen;
        outcol = 33;
    }
}

void GenerateLong(__int64 val)
{ 
	if( gentype == longgen && outcol < 56) {
                fprintf(output,",%I64d",val);
                outcol += 10;
                }
        else    {
                nl();
                fprintf(output,"\tdw\t%I64d",val);
                gentype = longgen;
                outcol = 25;
                }
}

void GenerateReference(SYM *sp,int offset)
{
	char    sign;
    if( offset < 0) {
        sign = '-';
        offset = -offset;
    }
    else
        sign = '+';
    if( gentype == longgen && outcol < 55 - strlen(sp->name)) {
        if( sp->storage_class == sc_static)
                fprintf(output,",%s_%I64d%c%d",GetNamespace(),sp->value.i,sign,offset);
        else if( sp->storage_class == sc_thread)
                fprintf(output,",%s_%I64d%c%d",GetNamespace(),sp->value.i,sign,offset);
        else
                fprintf(output,",%s%c%d",sp->name,sign,offset);
        outcol += (11 + strlen(sp->name));
    }
    else {
        nl();
        if(sp->storage_class == sc_static)
            fprintf(output,"\tdw\t%s_%I64d%c%d",GetNamespace(),sp->value.i,sign,offset);
        else if(sp->storage_class == sc_thread)
            fprintf(output,"\tdw\t%s_%I64d%c%d",GetNamespace(),sp->value.i,sign,offset);
        else
            fprintf(output,"\tdw\t%s%c%d",sp->name,sign,offset);
        outcol = 26 + strlen(sp->name);
        gentype = longgen;
    }
}

void genstorage(int nbytes)
{       nl();
        fprintf(output,"\tdcb.b\t%d,0xff\n",nbytes);
}

void GenerateLabelReference(int n)
{       if( gentype == longgen && outcol < 58) {
                fprintf(output,",%s_%d",GetNamespace(),n);
                outcol += 6;
                }
        else    {
                nl();
                fprintf(output,"\tlong\t%s_%d",GetNamespace(),n);
                outcol = 22;
                gentype = longgen;
                }
}

/*
 *      make s a string literal and return it's label number.
 */
int stringlit(char *s)
{      
	struct slit *lp;

    ++global_flag;          /* always allocate from global space. */
    lp = (struct slit *)xalloc(sizeof(struct slit));
    lp->label = nextlabel++;
    lp->str = litlate(s);
	lp->nmspace = litlate(GetNamespace());
    lp->next = strtab;
    strtab = lp;
    --global_flag;
    return lp->label;
}

/*
 *      dump the string literal pool.
 */
void dumplits()
{
	char            *cp;

    cseg();
    nl();
	align(8);
    nl();
	while( strtab != NULL) {
	    nl();
        put_label(strtab->label,strtab->str,strtab->nmspace,'D');
        cp = strtab->str;
        while(*cp)
            GenerateChar(*cp++);
        GenerateChar(0);
        strtab = strtab->next;
    }
    nl();
}

void nl()
{       if(outcol > 0) {
                fprintf(output,"\n");
                outcol = 0;
                gentype = nogen;
                }
}

void align(int n)
{
	fprintf(output,"\talign\t%d\n",n);
}

void cseg()
{
	if( curseg != codeseg) {
		nl();
		fprintf(output,"\tcode\n");
		fprintf(output,"\talign\t16\n");
		curseg = codeseg;
    }
}

void dseg()
{    
	if( curseg != dataseg) {
		nl();
		fprintf(output,"\tdata\n");
		fprintf(output,"\talign\t8\n");
		curseg = dataseg;
    }
}

void tseg()
{    
	if( curseg != tlsseg) {
		nl();
		fprintf(output,"\ttls\n");
		fprintf(output,"\talign\t8\n");
		curseg = tlsseg;
    }
}

void seg(int sg)
{    
	if( curseg != sg) {
		nl();
		switch(sg) {
		case bssseg:
			fprintf(output,"\tbss\n");
			fprintf(output,"\talign\t8\n");
			break;
		case dataseg:
			fprintf(output,"\tdata\n");
			fprintf(output,"\talign\t8\n");
			break;
		case tlsseg:
			fprintf(output,"\ttls\n");
			fprintf(output,"\talign\t8\n");
			break;
		case idataseg:
			fprintf(output,"\tidata\n");
			fprintf(output,"\talign\t8\n");
			break;
		case codeseg:
			fprintf(output,"\tcode\n");
			fprintf(output,"\talign\t16\n");
			break;
		}
		curseg = sg;
    }
}
