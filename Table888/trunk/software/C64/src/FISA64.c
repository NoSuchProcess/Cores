// ============================================================================
// (C) 2012-2015 Robert Finch
// All Rights Reserved.
// robfinch<remove>@finitron.ca
//
// C64 - FISA64 'C' derived language compiler
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
#include <stdio.h>
#include <string.h>
#include "c.h"
#include "expr.h"
#include "Statement.h"
#include "gen.h"
#include "cglbdec.h"

extern int     breaklab;
extern int     contlab;
extern int     retlab;
extern int		throwlab;

extern int lastsph;
extern char *semaphores[20];

extern TYP              stdfunc;

void GenerateFISA64Return(SYM *sym, Statement *stmt);
int TempInvalidate();
void TempRevalidate(int);
void ReleaseTempRegister(AMODE *ap);
AMODE *GetTempRegister();
void FISA64_GenLdi(AMODE *, AMODE *);
extern AMODE *copy_addr(AMODE *ap);
extern void GenLoad(AMODE *ap1, AMODE *ap3, int ssize);

/*
 *      returns the desirability of optimization for a subexpression.
 */
static int FISA64OptimizationDesireability(CSE *csp)
{
	if( csp->voidf || (csp->exp->nodetype == en_icon &&
                       csp->exp->i < 16383 && csp->exp->i >= -16384))
        return 0;
    if (csp->exp->nodetype==en_cnacon)
        return 0;
	if (csp->exp->isVolatile)
		return 0;
    if( IsLValue(csp->exp) )
	    return 2 * csp->uses;
    return csp->uses;
}


// ----------------------------------------------------------------------------
// AllocateRegisterVars will allocate registers for the expressions that have
// a high enough desirability.
// ----------------------------------------------------------------------------

int AllocateFISA64RegisterVars()
{
	CSE *csp;
    ENODE *exptr;
    int reg, mask, rmask;
    AMODE *ap, *ap2, *ap3;
    AMODE *a1,*a2,*a3,*a4;
	int64_t nn;
	int cnt;
	int size;

	reg = 11;
    mask = 0;
	rmask = 0;
    while( bsort(&olist) );         /* sort the expression list */
    csp = olist;
    while( csp != NULL ) {
        if( FISA64OptimizationDesireability(csp) < 3 )	// was < 3
            csp->reg = -1;
//        else if( csp->duses > csp->uses / 4 && reg < 18 )
		else {
			{
				if( csp->duses > csp->uses / 4 && reg < 18 )
//				if( reg < 18 )	// was / 4
					csp->reg = reg++;
				else
					csp->reg = -1;
			}
		}
        if( csp->reg != -1 )
		{
			rmask = rmask | (1 << (31 - csp->reg));
			mask = mask | (1 << csp->reg);
		}
        csp = csp->next;
    }
	if( mask != 0 ) {
		cnt = 0;
		//GenerateTriadic(op_subui,0,makereg(regSP),makereg(regSP),make_immed(bitsset(rmask)*8));
		for (nn = 0; nn < 32; nn++) {
			if (rmask & (0x80000000 >> nn)) {
				//GenerateDiadic(op_sw,0,makereg(nn&31),make_indexed(cnt,regSP));
				GenerateMonadic(op_push,0,makereg(nn&31));
				cnt+=8;
			}
		}
	}
    save_mask = mask;
    csp = olist;
    while( csp != NULL ) {
            if( csp->reg != -1 )
                    {               /* see if preload needed */
                    exptr = csp->exp;
                    if( !IsLValue(exptr) || (exptr->p[0]->i > 0) )
                            {
                            initstack();
                            ap = GenerateExpression(exptr,F_ALL,8);
							ap2 = makereg(csp->reg);
							if (ap->mode==am_immed) {
                                FISA64_GenLdi(ap2,ap);
                           }
							else if (ap->mode==am_reg)
								GenerateDiadic(op_mov,0,ap2,ap);
							else {
								size = GetNaturalSize(exptr);
								ap->isUnsigned = exptr->isUnsigned;
								GenLoad(ap2,ap,size);
							}
                            ReleaseTempRegister(ap);
                            }
                    }
            csp = csp->next;
            }
	return popcnt(mask);
}


AMODE *GenExprFISA64(ENODE *node)
{
	AMODE *ap1,*ap2,*ap3;
	int lab0, lab1;
	int op;
	int size;

    lab0 = nextlabel++;
    lab1 = nextlabel++;
	switch(node->nodetype) {
	case en_eq:		op = op_seq;	break;
	case en_ne:		op = op_sne;	break;
	case en_lt:		op = op_slt;	break;
	case en_ult:	op = op_sltu;	break;
	case en_le:		op = op_sle;	break;
	case en_ule:	op = op_sleu;	break;
	case en_gt:		op = op_sgt;	break;
	case en_ugt:	op = op_sgtu;	break;
	case en_ge:		op = op_sge;	break;
	case en_uge:	op = op_sgeu;	break;
	}
	switch(node->nodetype) {
	case en_eq:
	case en_ne:
	case en_lt:
	case en_ult:
	case en_gt:
	case en_ugt:
	case en_le:
	case en_ule:
	case en_ge:
	case en_uge:
		size = GetNaturalSize(node);
		ap1 = GenerateExpression(node->p[0],F_REG, size);
		ap2 = GenerateExpression(node->p[1],F_REG|F_IMMED,size);
		GenerateTriadic(op,0,ap1,ap1,ap2);
		ReleaseTempRegister(ap2);
		return ap1;
	}
    GenerateFalseJump(node,lab0,0);
    ap1 = GetTempRegister();
    GenerateDiadic(op_lc0i,0,ap1,make_immed(1));
    GenerateMonadic(op_bra,0,make_label(lab1));
    GenerateLabel(lab0);
    GenerateDiadic(op_lc0i,0,ap1,make_immed(0));
    GenerateLabel(lab1);
    return ap1;
}

void GenerateFISA64Cmp(ENODE *node, int op, int label, int predreg)
{
	int size;
	AMODE *ap1, *ap2, *ap3;

	size = GetNaturalSize(node);
	ap1 = GenerateExpression(node->p[0],F_REG, size);
	ap2 = GenerateExpression(node->p[1],F_REG|F_IMMED,size);
	// Optimize CMP to zero and branch into plain branch, this works only for
	// signed relational compares.
	if (ap2->mode == am_immed && ap2->offset->i==0 && (op==op_eq || op==op_ne || op==op_lt || op==op_le || op==op_gt || op==op_ge)) {
    	switch(op)
    	{
    	case op_eq:	op = op_beq; break;
    	case op_ne:	op = op_bne; break;
    	case op_lt: op = op_blt; break;
    	case op_le: op = op_ble; break;
    	case op_gt: op = op_bgt; break;
    	case op_ge: op = op_bge; break;
    	}
		ReleaseTempRegister(ap2);
		ReleaseTempRegister(ap1);
		GenerateDiadic(op,0,ap1,make_clabel(label));
		return;
	}
	if (op==op_ltu || op==op_leu || op==op_gtu || op==op_geu)
 	    GenerateTriadic(op_cmpu,0,ap1,ap1,ap2);
	else
 	    GenerateTriadic(op_cmp,0,ap1,ap1,ap2);
	switch(op)
	{
	case op_eq:	op = op_beq; break;
	case op_ne:	op = op_bne; break;
	case op_lt: op = op_blt; break;
	case op_le: op = op_ble; break;
	case op_gt: op = op_bgt; break;
	case op_ge: op = op_bge; break;
	case op_ltu: op = op_blt; break;
	case op_leu: op = op_ble; break;
	case op_gtu: op = op_bgt; break;
	case op_geu: op = op_bge; break;
	}
	GenerateDiadic(op,0,ap1,make_clabel(label));
	ReleaseTempRegister(ap2);
	ReleaseTempRegister(ap1);
}


// Generate a function body.
//
void GenerateFISA64Function(SYM *sym, Statement *stmt)
{
	char buf[20];
	char *bl;
	AMODE *ap;

	throwlab = retlab = contlab = breaklab = -1;
	lastsph = 0;
	memset(semaphores,0,sizeof(semaphores));
	throwlab = nextlabel++;
	while( lc_auto & 7 )	/* round frame size to word */
		++lc_auto;
	if (sym->IsInterrupt) {
	}
	if (!sym->IsNocall) {
		// For a leaf routine don't bother to store the link register or exception link register.
		if (sym->IsLeaf) {
    		GenerateTriadic(op_subui,0,makereg(regSP),makereg(regSP),make_immed(16));
			GenerateMonadic(op_push,0,makereg(regBP));
        }
		else {
			GenerateMonadic(op_push, 0, makereg(regLR));
			GenerateMonadic(op_push, 0, makereg(regXLR));
			GenerateMonadic(op_push, 0, makereg(regBP));
			ap = make_label(throwlab);
			ap->mode = am_immed;
			FISA64_GenLdi(makereg(regXLR),ap);
		}
		GenerateDiadic(op_mov,0,makereg(regBP),makereg(regSP));
		if (lc_auto)
			GenerateTriadic(op_subui,0,makereg(regSP),makereg(regSP),make_immed(lc_auto));
	}
	if (optimize)
		opt1(stmt);
    GenerateStatement(stmt);
    GenerateFISA64Return(sym,0);
	// Generate code for the hidden default catch
	GenerateLabel(throwlab);
	if (sym->IsLeaf){
		if (sym->DoesThrow) {
			GenerateDiadic(op_mov,0,makereg(regLR),makereg(regXLR));
			GenerateDiadic(op_bra,0,make_label(retlab),NULL);				// goto regular return cleanup code
		}
	}
	else {
		GenerateDiadic(op_lw,0,makereg(regLR),make_indexed(8,regBP));		// load throw return address from stack into LR
		GenerateDiadic(op_sw,0,makereg(regLR),make_indexed(16,regBP));		// and store it back (so it can be loaded with the lm)
		GenerateDiadic(op_bra,0,make_label(retlab),NULL);				// goto regular return cleanup code
	}
}


// Generate a return statement.
//
void GenerateFISA64Return(SYM *sym, Statement *stmt)
{
	AMODE *ap;
	int nn;
	int lab1;
	int cnt;
	int toAdd;

    // Generate the return expression and force the result into r1.
    if( stmt != NULL && stmt->exp != NULL )
	{
		initstack();
		ap = GenerateExpression(stmt->exp,F_REG|F_IMMED,8);
		if (ap->mode == am_immed)
		    FISA64_GenLdi(makereg(1),ap);
		else if (ap->mode == am_reg)
			GenerateDiadic(op_mov, 0, makereg(1),ap);
		else
		    GenLoad(makereg(1),ap,8);
		ReleaseTempRegister(ap);
	}

	// Generate the return code only once. Branch to the return code for all returns.
	if( retlab == -1 )
    {
		retlab = nextlabel++;
		GenerateLabel(retlab);
		// Unlock any semaphores that may have been set
		for (nn = lastsph - 1; nn >= 0; nn--)
			GenerateDiadic(op_sb,0,makereg(0),make_string(semaphores[nn]));
		if (sym->IsNocall)	// nothing to do for nocall convention
			return;
		// Restore registers used as register variables.
		if( save_mask != 0 ) {
			cnt = (bitsset(save_mask)-1)*8;
			for (nn = 31; nn >=1 ; nn--) {
				if (save_mask & (1 << nn)) {
					GenerateMonadic(op_pop,0,makereg(nn));
					cnt -= 8;
				}
			}
		}
		// Unlink the stack
		// For a leaf routine the link register and exception link register doesn't need to be saved/restored.
		GenerateDiadic(op_mov,0,makereg(regSP),makereg(regBP));
		if (sym->IsLeaf) {
			GenerateMonadic(op_pop,0,makereg(regBP));
			toAdd = 16;
        }
		else {
			GenerateMonadic(op_pop,0,makereg(regBP));
			GenerateMonadic(op_pop,0,makereg(regXLR));
			GenerateMonadic(op_pop,0,makereg(regLR));
			toAdd = 0;
		}
		// Generate the return instruction. For the Pascal calling convention pop the parameters
		// from the stack.
		if (sym->IsInterrupt) {
			GenerateMonadic(op_rti,0,NULL);
			return;
		}
		if (sym->IsPascal)
            GenerateMonadic(op_rts,0,make_immed(toAdd+sym->NumParms * 8));
		else
            GenerateMonadic(op_rts,0,make_immed(toAdd));
    }
	// Just branch to the already generated stack cleanup code.
	else {
		GenerateMonadic(op_bra,0,make_label(retlab));
	}
}

// push the operand expression onto the stack.
//
static void GeneratePushParameter(ENODE *ep)
{    
	AMODE *ap;
	ap = GenerateExpression(ep,F_REG,8);
	GenerateMonadic(op_push,0,ap);
	ReleaseTempRegister(ap);
}

// push entire parameter list onto stack
//
static int GeneratePushParameterList(ENODE *plist)
{
	int i;

    for(i = 0; plist != NULL; i++ )
    {
		GeneratePushParameter(plist->p[0]);
		plist = plist->p[1];
    }
    return i;
}

AMODE *GenerateFISA64FunctionCall(ENODE *node, int flags)
{ 
	AMODE *ap, *ap2, *result;
	AMODE *a1,*a2,*a3,*a4;
	SYM *sym;
    int             i;
	int msk;
	int sp = 0;

	sp = TempInvalidate();
	sym = NULL;
    i = GeneratePushParameterList(node->p[1]);
	// Call the function
	if( node->p[0]->nodetype == en_nacon || node->p[0]->nodetype == en_cnacon ) {
 /*
        ap = GetTempRegister();
        ap2 = make_offset(node->p[0]);
        a1 = copy_addr(ap2);
        a2 = copy_addr(ap2);
        a3 = copy_addr(ap2);
        a4 = copy_addr(ap2);
        a1->rshift = 0;
        a2->rshift = 16;
        a3->rshift = 32;
        a4->rshift = 48;
        a1->mode = am_immed;
        a2->mode = am_immed;
        a3->mode = am_immed;
        a4->mode = am_immed;
        GenerateDiadic(op_lc0i,0,ap,a1);
        GenerateDiadic(op_lc1i,0,ap,a2);
        GenerateDiadic(op_lc2i,0,ap,a3);
        GenerateDiadic(op_lc3i,0,ap,a4);
        //GenerateDiadic(op_jal,0,makereg(regLR),make_offset(node->p[0]));
        GenerateDiadic(op_jal,0,makereg(regLR),make_indirect(ap->preg));
*/
		sym = gsearch(node->p[0]->sp);
//		ReleaseTempRegister(ap);
        GenerateMonadic(op_bsr,0,make_offset(node->p[0]));
	}
    else
    {
		ap = GenerateExpression(node->p[0],F_REG,8);
		ap->mode = am_ind;
		ap->offset = 0;
		GenerateDiadic(op_jal,0,makereg(regLR),ap);
		ReleaseTempRegister(ap);
    }
	// Pop parameters off the stack
	if (i!=0) {
		if (sym) {
			if (!sym->IsPascal)
				GenerateTriadic(op_addui,0,makereg(regSP),makereg(regSP),make_immed(i * 8));
		}
		else
			GenerateTriadic(op_addui,0,makereg(regSP),makereg(regSP),make_immed(i * 8));
	}
	TempRevalidate(sp);
    result = GetTempRegister();
	if (flags & F_NOVALUE)
		;
	else {
		if( result->preg != 1 || (flags & F_REG) == 0 ) {
			if (sym) {
				if (sym->tp->btp->type==bt_void)
					;
				else
					GenerateDiadic(op_mov,0,result,makereg(1));
			}
			else
				GenerateDiadic(op_mov,0,result,makereg(1));
		}
	}
    return result;
}

void FISA64_GenLdi(AMODE *ap1, AMODE *ap2)
{
    AMODE *a1,*a2,*a3,*a4;

	GenerateDiadic(op_ldi,0,ap1,ap2);
    return;

    a1 = copy_addr(ap2);
    a2 = copy_addr(ap2);
    a3 = copy_addr(ap2);
    a4 = copy_addr(ap2);
    a1->rshift = 0;
    a2->rshift = 16;
    a3->rshift = 32;
    a4->rshift = 48;
    if (ap2->offset->nodetype != en_icon) { // must be a name constant
		GenerateDiadic(op_lc0i,0,ap1,a1);
		if (address_bits > 15)
		GenerateDiadic(op_lc1i,0,ap1,a2);
		if (address_bits > 31)
		GenerateDiadic(op_lc2i,0,ap1,a3);
		if (address_bits > 47)
		GenerateDiadic(op_lc3i,0,ap1,a4);
    }
    else {
		GenerateDiadic(op_lc0i,0,ap1,a1);
		if (ap2->offset->i > 0x7FFFLL || ap2->offset->i <= -0x7FFFLL)
		GenerateDiadic(op_lc1i,0,ap1,a2);
        if (ap2->offset->i > 0x7FFFFFFFLL || ap2->offset->i <= -0x7FFFFFFFLL)
		GenerateDiadic(op_lc2i,0,ap1,a3);
	    if (ap2->offset->i > 0x7FFFFFFFFFFFLL || ap2->offset->i <= -0x7FFFFFFFFFFFLL)
		GenerateDiadic(op_lc3i,0,ap1,a4);
    }
}
