// ============================================================================
//        __
//   \\__/ o\    (C) 2012-2014  Robert Finch, Stratford
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
#include <stdio.h>
#include <stdlib.h>
#include        "c.h"
#include        "expr.h"
#include "Statement.h"
#include        "gen.h"
#include        "cglbdec.h"

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

/*
 *      this module contains all of the code generation routines
 *      for evaluating expressions and conditions.
 */

int hook_predreg=15;

AMODE *GenerateExpression();            /* forward ParseSpecifieraration */
AMODE *GenExprRaptor64(ENODE *node);
void GenerateRaptor64Cmp(ENODE *node, int op, int label, int predreg);
void GenerateTable888Cmp(ENODE *node, int op, int label, int predreg);
void GenerateThorCmp(ENODE *node, int op, int label, int predreg);
extern void FISA64_GenLdi(AMODE*,AMODE *);
void GenLoad(AMODE *ap1, AMODE *ap3, int ssize);

extern int throwlab;
static int nest_level = 0;

static void Enter(char *p)
{
/*
     int nn;
     
     for (nn = 0; nn < nest_level; nn++)
         printf(" ");
     printf("%s: %d ", p, lineno);
     nest_level++;
*/
}
static void Leave(char *p, int n)
{
/*
     int nn;
     
     nest_level--;
     for (nn = 0; nn < nest_level; nn++)
         printf(" ");
     printf("%s (%d) ", p, n);
*/
}


/*
 *      construct a reference node for an internal label number.
 */
AMODE *make_label(int64_t lab)
{
	ENODE *lnode;
    AMODE *ap;
    lnode = allocEnode();
    lnode->nodetype = en_labcon;
    lnode->i = lab;
    ap = allocAmode();
    ap->mode = am_direct;
    ap->offset = lnode;
	ap->isUnsigned = TRUE;
    return ap;
}

AMODE *make_clabel(int64_t lab)
{
	ENODE *lnode;
    AMODE *ap;
    lnode = allocEnode();
    lnode->nodetype = en_clabcon;
    lnode->i = lab;
	if (lab==-1)
		printf("-1\r\n");
    ap = allocAmode();
    ap->mode = am_direct;
    ap->offset = lnode;
	ap->isUnsigned = TRUE;
    return ap;
}

AMODE *make_string(char *s)
{
	ENODE *lnode;
    AMODE *ap;

    lnode = allocEnode();
    lnode->nodetype = en_nacon;
    lnode->sp = s;
    ap = allocAmode();
    ap->mode = am_direct;
    ap->offset = lnode;
    return ap;
}

/*
 *      make a node to reference an immediate value i.
 */
AMODE *make_immed(int64_t i)
{
	AMODE *ap;
    ENODE *ep;
    ep = allocEnode();
    ep->nodetype = en_icon;
    ep->i = i;
    ap = allocAmode();
    ap->mode = am_immed;
    ap->offset = ep;
    return ap;
}

AMODE *make_indirect(int i)
{
	AMODE *ap;
    ENODE *ep;
    ep = allocEnode();
    ep->nodetype = en_uw_ref;
    ep->i = 0;
    ap = allocAmode();
	ap->mode = am_ind;
	ap->preg = i;
    ap->offset = 0;//ep;	//=0;
    return ap;
}

AMODE *make_indexed(int64_t o, int i)
{
	AMODE *ap;
    ENODE *ep;
    ep = allocEnode();
    ep->nodetype = en_icon;
    ep->i = o;
    ap = allocAmode();
	ap->mode = am_indx;
	ap->preg = i;
    ap->offset = ep;
    return ap;
}

/*
 *      make a direct reference to a node.
 */
AMODE *make_offset(ENODE *node)
{
	AMODE *ap;
    ap = allocAmode();
    ap->mode = am_direct;
    ap->offset = node;
    return ap;
}
        
AMODE *make_indx(ENODE *node, int reg)
{
	AMODE *ap;
    ap = allocAmode();
    ap->mode = am_indx;
    ap->offset = node;
    ap->preg = reg;
    return ap;
}
// ----------------------------------------------------------------------------
//      MakeLegalAmode will coerce the addressing mode in ap1 into a
//      mode that is satisfactory for the flag word.
// ----------------------------------------------------------------------------
void MakeLegalAmode(AMODE *ap,int flags, int size)
{
	AMODE *ap2, *ap3;
	int64_t i;

//     Enter("MkLegalAmode");
	if (ap==(AMODE*)NULL) return;
    if( ((flags & F_VOL) == 0) || ap->tempflag )
    {
        switch( ap->mode ) {
            case am_immed:
					i = ((ENODE *)(ap->offset))->i;
                    if ((flags & F_IMMED18) && isRaptor64) {
						if (i < 0x1ffff && i > -0x1ffff)
							return;
					}
					else if (flags & F_IMM8) {
						if (i < 256 && i >= 0)
							return;
					}
					else if (flags & F_IMM0) {
						if (i==0)
							return;
					}
                    else if( flags & F_IMMED )
                        return;         /* mode ok */
                    break;
            case am_reg:
                    if( flags & F_REG )
                        return;
                    break;
            case am_fpreg:
                    if( flags & F_FPREG )
                        return;
                    break;
			case am_breg:
					if (flags & F_BREG)
						return;
					break;
            case am_ind:
			case am_indx:
            case am_indx2: 
			case am_direct:
			case am_indx3:
                    if( flags & F_MEM )
                        return;
                    break;
            }
        }

        if( flags & F_REG )
        {
            ReleaseTempRegister(ap);      /* maybe we can use it... */
            ap2 = GetTempRegister();
			if (ap->mode == am_ind || ap->mode==am_indx)
                GenLoad(ap2,ap,size);
			else if (ap->mode==am_immed) {
			    if (isFISA64)
			        FISA64_GenLdi(ap2,ap);
                else
				    GenerateDiadic(op_ldi,0,ap2,ap);
            }
			else {
				if (ap->mode==am_reg)
					GenerateDiadic(op_mov,0,ap2,ap);
				else
                    GenLoad(ap2,ap,size);
			}
            ap->mode = am_reg;
            ap->preg = ap2->preg;
            ap->deep = ap2->deep;
            ap->tempflag = 1;
            return;
        }
        if( flags & F_FPREG )
        {
            ReleaseTempReg(ap);      /* maybe we can use it... */
            ap2 = GetTempFPRegister();
			if (ap->mode == am_ind || ap->mode==am_indx)
                GenLoad(ap2,ap,size);
			else if (ap->mode==am_immed) {
			    if (isFISA64)
			        FISA64_GenLdi(ap2,ap);
                else
				    GenerateDiadic(op_ldi,0,ap2,ap);
            }
			else {
				if (ap->mode==am_reg)
					GenerateDiadic(op_mov,0,ap2,ap);
				else
                    GenLoad(ap2,ap,size);
			}
            ap->mode = am_fpreg;
            ap->preg = ap2->preg;
            ap->deep = ap2->deep;
            ap->tempflag = 1;
            return;
        }
		if (flags & F_BREG) {
            ReleaseTempRegister(ap);
            ap2 = GetTempBrRegister();
			if (ap->mode == am_ind || ap->mode==am_indx) {
				ap3 = GetTempRegister();
				GenerateDiadic(op_lw,0,ap3,ap);
				GenerateDiadic(op_mtspr,0,ap2,ap3);
				ReleaseTempRegister(ap3);
			}
			else if (ap->mode==am_immed) {
				GenerateDiadic(op_ldis,0,ap2,ap);
			}
			else {
				if (ap->mode==am_reg)
					GenerateDiadic(op_mtspr,0,ap2,ap);
				else {
					ap3 = GetTempRegister();
					GenerateDiadic(op_lw,0,ap3,ap);
					GenerateDiadic(op_mtspr,0,ap2,ap3);
					ReleaseTempRegister(ap3);
				}
			}
            ap->mode = am_breg;
            ap->preg = ap2->preg;
            ap->deep = ap2->deep;
            ap->tempflag = 1;
            return;
		}
		// Here we wanted the mode to be non-register (memory/immed)
		// Should fix the following to place the result in memory and
		// not a register.
        if( size == 1 )
		{
			ReleaseTempRegister(ap);
			ap2 = GetTempRegister();
			GenerateDiadic(op_mov,0,ap2,ap);
			if (ap->isUnsigned)
				GenerateTriadic(op_andi,0,ap2,ap2,make_immed(255));
			else {
				if (isTable888|isFISA64)
					GenerateDiadic(op_sxb,0,ap2,ap2);
				else
					GenerateDiadic(op_sext8,0,ap2,ap2);
			}
			ap->mode = ap2->mode;
			ap->preg = ap2->preg;
			ap->deep = ap2->deep;
			size = 2;
        }
        ap2 = GetTempRegister();
		switch(ap->mode) {
		case am_ind:
		case am_indx:
            GenLoad(ap2,ap,size);
			break;
		case am_immed:
            if (isFISA64)
                FISA64_GenLdi(ap2,ap);
			else if (isTable888)
				GenerateDiadic(op_ldi,0,ap2,ap);
			else
				GenerateTriadic(op_ori,0,ap2,makereg(0),ap);
		case am_reg:
			if (isTable888|isFISA64)
				GenerateDiadic(op_mov,0,ap2,ap);
			else
				GenerateTriadic(op_or,0,ap2,ap,makereg(0));
		default:
            GenLoad(ap2,ap,size);
		}
    ap->mode = am_reg;
    ap->preg = ap2->preg;
    ap->deep = ap2->deep;
    ap->tempflag = 1;
//     Leave("MkLegalAmode",0);
}

void GenLoad(AMODE *ap3, AMODE *ap1, int ssize)
{
    if (ap3->isFloat) {
        GenerateDiadic(op_lfd,0,ap3,ap1);
    }
	else if (ap3->isUnsigned) {
		switch(ssize) {
		case 1:	GenerateDiadic(op_lbu,0,ap3,ap1); break;
		case 2:	GenerateDiadic(op_lcu,0,ap3,ap1); break;
		case 4:	GenerateDiadic(op_lhu,0,ap3,ap1); break;
		case 8:	GenerateDiadic(op_lw,0,ap3,ap1); break;
		}
	}
	else {
		switch(ssize) {
		case 1:	GenerateDiadic(op_lb,0,ap3,ap1); break;
		case 2:	GenerateDiadic(op_lc,0,ap3,ap1); break;
		case 4:	GenerateDiadic(op_lh,0,ap3,ap1); break;
		case 8:	GenerateDiadic(op_lw,0,ap3,ap1); break;
		case 12:	GenerateDiadic(op_lft,0,ap3,ap1); break;
		}
	}
}

void GenStore(AMODE *ap1, AMODE *ap3, int size)
{
    if (ap1->isFloat) {
        GenerateDiadic(op_sfd,0,ap1,ap3);
    } 
    else {
    	switch(size) {
    	case 1: GenerateDiadic(op_sb,0,ap1,ap3); break;
    	case 2: GenerateDiadic(op_sc,0,ap1,ap3); break;
    	case 4: GenerateDiadic(op_sh,0,ap1,ap3); break;
    	case 8: GenerateDiadic(op_sw,0,ap1,ap3); break;
    	}
    }
}

/*
 *      if isize is not equal to osize then the operand ap will be
 *      loaded into a register (if not already) and if osize is
 *      greater than isize it will be extended to match.
 */
void GenerateSignExtend(AMODE *ap, int isize, int osize, int flags)
{    
	struct amode *ap2;

	if( isize == osize )
        return;
	if (ap->isUnsigned)
		return;
    if(ap->mode != am_reg)
        MakeLegalAmode(ap,flags & F_REG,isize);
	if (ap->isFloat) {
		switch(isize) {
		case 4:	GenerateDiadic(op_fs2d,0,ap,ap); break;
		}
	}
	else {
		if (isTable888|isFISA64) {
			switch( isize )
			{
			case 1:	GenerateDiadic(op_sxb,0,ap,ap); break;
			case 2:	GenerateDiadic(op_sxc,0,ap,ap); break;
			case 4:	GenerateDiadic(op_sxh,0,ap,ap); break;
			}
		}
		else {
			switch( isize )
			{
			case 1:	GenerateDiadic(op_sext8,0,ap,ap); break;
			case 2:	GenerateDiadic(op_sext16,0,ap,ap); break;
			case 4:	GenerateDiadic(op_sext32,0,ap,ap); break;
			}
		}
	}
}

void GenerateZeroExtend(AMODE *ap, int isize, int osize)
{    
	AMODE *ap2;

    if(ap->mode != am_reg)
        MakeLegalAmode(ap,F_REG,isize);
	switch( osize )
	{
	case 1:	GenerateTriadic(op_andi,0,ap,ap,make_immed(0xFF)); break;
	case 2:	GenerateTriadic(op_andi,0,ap,ap,make_immed(0xFFFF)); break;
	case 4:	GenerateTriadic(op_andi,0,ap,ap,make_immed(0xFFFFFFFF)); break;
	}
}

/*
 *      return true if the node passed can be generated as a short
 *      offset.
 */
int isshort(ENODE *node)
{
	return node->nodetype == en_icon &&
        (node->i >= -32768 && node->i <= 32767);
}

/*
 *      return true if the node passed can be evaluated as a byte
 *      offset.
 */
int isbyte(ENODE *node)
{
	return node->nodetype == en_icon &&
       (-128 <= node->i && node->i <= 127);
}

int ischar(ENODE *node)
{
	return node->nodetype == en_icon &&
        (node->i >= -32768 && node->i <= 32767);
}

// ----------------------------------------------------------------------------
//      generate code to evaluate an index node (^+) and return
//      the addressing mode of the result. This routine takes no
//      flags since it always returns either am_ind or am_indx.
//
// No reason to ReleaseTempReg() because the registers used are transported
// forward.
// ----------------------------------------------------------------------------
AMODE *GenerateIndex(ENODE *node)
{       
	AMODE *ap1, *ap2, *ap3;
	
    if( (node->p[0]->nodetype == en_tempref || node->p[0]->nodetype==en_regvar) && (node->p[1]->nodetype == en_tempref || node->p[1]->nodetype==en_regvar))
    {       /* both nodes are registers */
        ap1 = GenerateExpression(node->p[0],F_REG,8);
        ap2 = GenerateExpression(node->p[1],F_REG,8);
        ap1->mode = am_indx2;
        ap1->sreg = ap2->preg;
		ap1->deep2 = ap2->deep2;
		ap1->offset = makeinode(en_icon,0);
		ap1->scale = node->scale;
        return ap1;
    }
    ap1 = GenerateExpression(node->p[0],F_REG | F_IMMED,8);
    if( ap1->mode == am_immed )
    {
		ap2 = GenerateExpression(node->p[1],F_REG,8);
		ap2->mode = am_indx;
		ap2->offset = ap1->offset;
		ap2->isUnsigned = ap1->isUnsigned;
		return ap2;
    }
    ap2 = GenerateExpression(node->p[1],F_ALL,8);   /* get right op */
    if( ap2->mode == am_immed && ap1->mode == am_reg ) /* make am_indx */
    {
        ap2->mode = am_indx;
        ap2->preg = ap1->preg;
        ap2->deep = ap1->deep;
        return ap2;
    }
	if (ap2->mode == am_ind && ap1->mode == am_reg) {
        ap2->mode = am_indx2;
        ap2->sreg = ap1->preg;
		ap2->deep2 = ap1->deep;
        return ap2;
	}
	if (ap2->mode == am_direct && ap1->mode==am_reg) {
        ap2->mode = am_indx;
        ap2->preg = ap1->preg;
        ap2->deep = ap1->deep;
        return ap2;
    }
	// ap1->mode must be F_REG
	MakeLegalAmode(ap2,F_REG,8);
    ap1->mode = am_indx2;            /* make indexed */
	ap1->sreg = ap2->preg;
	ap1->deep2 = ap2->deep;
	ap1->offset = makeinode(en_icon,0);
	ap1->scale = node->scale;
    return ap1;                     /* return indexed */
}

long GetReferenceSize(ENODE *node)
{
    switch( node->nodetype )        /* get load size */
    {
    case en_b_ref:
    case en_ub_ref:
    case en_bfieldref:
    case en_ubfieldref:
            return 1;
	case en_c_ref:
	case en_uc_ref:
	case en_cfieldref:
	case en_ucfieldref:
			return 2;
	case en_h_ref:
	case en_uh_ref:
	case en_hfieldref:
	case en_uhfieldref:
	case en_flt_ref:
			return 4;
    case en_w_ref:
	case en_uw_ref:
    case en_wfieldref:
	case en_uwfieldref:
	case en_tempref:
	case en_regvar:
	case en_dbl_ref:
            return 8;
    case en_triple_ref:
            return 12;
	//case en_struct_ref:
	//		return node->esize;
    }
	return 8;
}

//
//  Return the addressing mode of a dereferenced node.
//
AMODE *GenerateDereference(ENODE *node,int flags,int size, int su)
{    
	struct amode    *ap1, *ap2;
    int             siz1;

    Enter("Genderef");
	siz1 = GetReferenceSize(node);
    if( node->p[0]->nodetype == en_add )
    {
//        ap2 = GetTempRegister();
        ap1 = GenerateIndex(node->p[0]);
//        GenerateTriadic(op_add,0,ap2,makereg(ap1->preg),makereg(regGP));
		ap1->isUnsigned = !su;//node->isUnsigned;
		// *** may have to fix for stackseg
		ap1->segment = dataseg;
//		ap2->mode = ap1->mode;
//		ap2->segment = dataseg;
//		ap2->offset = ap1->offset;
//		ReleaseTempRegister(ap1);
		if (!node->isUnsigned)
			GenerateSignExtend(ap1,siz1,size,flags);
		else
		    MakeLegalAmode(ap1,flags,siz1);
        MakeLegalAmode(ap1,flags,size);
    Leave("Genderef",6);
        return ap1;
    }
    else if( node->p[0]->nodetype == en_autocon )
    {
        ap1 = allocAmode();
        ap1->mode = am_indx;
        ap1->preg = regBP;
		ap1->segment = stackseg;
        ap1->offset = makeinode(en_icon,node->p[0]->i);
		ap1->isUnsigned = !su;
		if (!node->isUnsigned)
	        GenerateSignExtend(ap1,siz1,size,flags);
		else
		    MakeLegalAmode(ap1,flags,siz1);
        MakeLegalAmode(ap1,flags,size);
    Leave("Genderef",5);
        return ap1;
    }
    else if( node->p[0]->nodetype == en_autofcon )
    {
        ap1 = allocAmode();
        ap1->mode = am_indx;
        ap1->preg = regBP;
        ap1->offset = makeinode(en_icon,node->p[0]->i);
		ap1->segment = stackseg;
		if (!node->isUnsigned)
	        GenerateSignExtend(ap1,siz1,size,flags);
		else
		    MakeLegalAmode(ap1,flags,siz1);
        MakeLegalAmode(ap1,flags,size);
    Leave("Genderef",4);
        return ap1;
    }
    else if(( node->p[0]->nodetype == en_labcon || node->p[0]->nodetype==en_nacon ) && use_gp)
    {
        ap1 = allocAmode();
        ap1->mode = am_indx;
        ap1->preg = regGP;
		ap1->segment = dataseg;
        ap1->offset = node->p[0];//makeinode(en_icon,node->p[0]->i);
		ap1->isUnsigned = !su;
		if (!node->isUnsigned)
	        GenerateSignExtend(ap1,siz1,size,flags);
		else
		    MakeLegalAmode(ap1,flags,siz1);
        MakeLegalAmode(ap1,flags,size);
    Leave("Genderef",3);
        return ap1;
    }
    ap1 = GenerateExpression(node->p[0],F_REG | F_FPREG | F_IMMED,8); /* generate address */
    if( ap1->mode == am_reg )
    {
//        ap1->mode = am_ind;
          if (use_gp) {
              ap1->mode = am_indx;
              ap1->sreg = regGP;
          }
          else
             ap1->mode = am_ind;
		ap1->offset = 0;	// ****
		ap1->isUnsigned = !su;
		if (!node->isUnsigned)
	        GenerateSignExtend(ap1,siz1,size,flags);
		else
		    MakeLegalAmode(ap1,flags,siz1);
        MakeLegalAmode(ap1,flags,size);
    Leave("Genderef",2);
        return ap1;
    }
    if( ap1->mode == am_fpreg )
    {
//        ap1->mode = am_ind;
          if (use_gp) {
              ap1->mode = am_indx;
              ap1->sreg = regGP;
          }
          else
             ap1->mode = am_ind;
		ap1->offset = 0;	// ****
		ap1->isUnsigned = !su;
		if (!node->isUnsigned)
	        GenerateSignExtend(ap1,siz1,size,flags);
		else
		    MakeLegalAmode(ap1,flags,siz1);
        MakeLegalAmode(ap1,flags,size);
    Leave("Genderef",1);
        return ap1;
    }
	// See segments notes
	//if (node->p[0]->nodetype == en_labcon &&
	//	node->p[0]->etype == bt_pointer && node->p[0]->constflag)
	//	ap1->segment = codeseg;
	//else
	//	ap1->segment = dataseg;
    if (use_gp) {
        ap1->mode = am_indx;
        ap1->preg = regGP;
    	ap1->segment = dataseg;
    }
    else {
        ap1->mode = am_direct;
	    ap1->isUnsigned = !su;
    }
//    ap1->offset = makeinode(en_icon,node->p[0]->i);
    ap1->isUnsigned = !su;
	if (!node->isUnsigned)
	    GenerateSignExtend(ap1,siz1,size,flags);
	else
		MakeLegalAmode(ap1,flags,siz1);
    MakeLegalAmode(ap1,flags,size);
    Leave("Genderef",0);
    return ap1;
}

//
// Generate code to evaluate a unary minus or complement.
//
AMODE *GenerateUnary(ENODE *node,int flags, int size, int op)
{
	AMODE *ap, *ap2;

    if (node->etype==bt_double) {
        ap2 = GetTempFPRegister();
        ap = GenerateExpression(node->p[0],F_FPREG,size);
        if (op==op_neg)
           op=op_fdneg;
    }
    else {
        ap2 = GetTempRegister();
        ap = GenerateExpression(node->p[0],F_REG,size);
    }
    GenerateDiadic(op,0,ap2,ap);
    ReleaseTempReg(ap);
    MakeLegalAmode(ap2,flags,size);
    return ap2;
}

// Generate code for a binary expression

AMODE *GenerateBinary(ENODE *node,int flags, int size, int op)
{
	AMODE *ap1, *ap2, *ap3;
	int op2;
	if (op==op_ftadd || op==op_ftsub || op==op_ftmul || op==op_ftdiv ||
        op==op_fdadd || op==op_fdsub || op==op_fdmul || op==op_fddiv ||
		op==op_fsadd || op==op_fssub || op==op_fsmul || op==op_fsdiv)
	{
    	ap3 = GetTempFPRegister();
	    ap1 = GenerateExpression(node->p[0],F_FPREG,size);
		ap2 = GenerateExpression(node->p[1],F_FPREG,size);
	}
	else {
      	ap3 = GetTempRegister();
        ap1 = GenerateExpression(node->p[0],F_REG,size);
		ap2 = GenerateExpression(node->p[1],F_REG|F_IMMED,size);
	}
	if (is816) {
	   Generate816Binary(op,ap1,ap1,ap2);
    }
    else {
       if (ap2->mode==am_immed) {
           switch(op) {
           op_add:    op = op_addi; break;
           op_addu:   op = op_addui; break;
           op_sub:    op = op_subi; break;
           op_subui:  op = op_subui; break;
           op_mul:    op = op_muli; break;
           op_mulu:   op = op_mului; break;
           op_div:    op = op_divi; break;
           op_divu:   op = op_divui; break;
           op_and:    op = op_andi; break;
           op_or:     op = op_ori; break;
           op_xor:    op = op_xori; break;
           op_eor:    op = op_eori; break;
           default:   ;
           }
       }
       GenerateTriadic(op,0,ap3,ap1,ap2);
    }
    ReleaseTempReg(ap2);
    ReleaseTempReg(ap1);
    MakeLegalAmode(ap3,flags,size);
    return ap3;
}

/*
 *      generate code to evaluate a mod operator or a divide
 *      operator.
 */
AMODE *GenerateModDiv(ENODE *node,int flags,int size, int op)
{
	AMODE *ap1, *ap2, *ap3;

    if( node->p[0]->nodetype == en_icon )
         swap_nodes(node);
    if (op==op_fddiv) {
        ap3 = GetTempFPRegister();
        ap1 = GenerateExpression(node->p[0],F_FPREG,8);
        ap2 = GenerateExpression(node->p[1],F_FPREG,8);
    }
    else {
        ap3 = GetTempRegister();
        ap1 = GenerateExpression(node->p[0],F_REG,8);
        ap2 = GenerateExpression(node->p[1],F_REG | F_IMMED,8);
    }
    if (ap2->mode==am_immed) {
       switch(op) {
       op_div:    op = op_divi; break;
       op_divu:   op = op_divui; break;
       op_mod:    op = op_modi; break;
       op_modu:   op = op_modui; break;
       default:   ;
       }
    }
    GenerateTriadic(op,0,ap3,ap1,ap2);
//    GenerateDiadic(op_ext,0,ap3,0);
    MakeLegalAmode(ap3,flags,8);
    ReleaseTempReg(ap2);
    ReleaseTempReg(ap1);
    return ap3;
}

/*
 *      exchange the two operands in a node.
 */
void swap_nodes(ENODE *node)
{
	ENODE *temp;
    temp = node->p[0];
    node->p[0] = node->p[1];
    node->p[1] = temp;
}

/*
 *      generate code to evaluate a multiply node. 
 */
AMODE *GenerateMultiply(ENODE *node, int flags, int size, int op)
{       
	AMODE *ap1, *ap2, *ap3;
    Enter("Genmul");
    if( node->p[0]->nodetype == en_icon )
        swap_nodes(node);
    if (op==op_fdmul) {
        ap3 = GetTempFPRegister();
        ap1 = GenerateExpression(node->p[0],F_FPREG,8);
        ap2 = GenerateExpression(node->p[1],F_FPREG,8);
    }
    else {
        ap3 = GetTempRegister();
        ap1 = GenerateExpression(node->p[0],F_REG,8);
        ap2 = GenerateExpression(node->p[1],F_REG | F_IMMED,8);
    }
    if (ap2->mode==am_immed) {
       switch(op) {
       op_mul:    op = op_muli; break;
       op_mulu:   op = op_mului; break;
       default:   ;
       }
    }
	GenerateTriadic(op,0,ap3,ap1,ap2);
    ReleaseTempReg(ap2);
    ReleaseTempReg(ap1);
    MakeLegalAmode(ap3,flags,8);
    Leave("Genmul",0);
    return ap3;
}

/*
 *      generate code to evaluate a condition operator node (?:)
 */
AMODE *gen_hook(ENODE *node,int flags, int size)
{
	AMODE *ap1, *ap2;
    int false_label, end_label;

	hook_predreg--;
    false_label = nextlabel++;
    end_label = nextlabel++;
    flags = (flags & F_REG) | F_VOL;
    GenerateFalseJump(node->p[0],false_label,hook_predreg);
    node = node->p[1];
    ap1 = GenerateExpression(node->p[0],flags,size);
    GenerateDiadic(op_bra,0,make_clabel(end_label),0);
    GenerateLabel(false_label);
    ap2 = GenerateExpression(node->p[1],flags,size);
    if( !equal_address(ap1,ap2) )
    {
		GenerateDiadic(op_mov,0,ap1,ap2);
    }
    ReleaseTempReg(ap2);
    GenerateLabel(end_label);
	hook_predreg++;
    return ap1;
}

void GenMemop(int op, AMODE *ap1, AMODE *ap2, int ssize)
{
	AMODE *ap3;

    if (isFISA64) {
        if (ap2->mode==am_immed && ssize==8) {
            if ((op==op_addi||op==op_add) && ap2->offset->i < 32 && ap2->offset->i >= -32) {
                GenerateDiadic(op_inc,0,ap1,ap2);
                return;
            }
            if ((op==op_addui||op==op_addu) && ap2->offset->i < 32) {
                GenerateDiadic(op_inc,0,ap1,ap2);
                return;
            }
            if ((op==op_subi||op==op_sub) && ap2->offset->i < 31 && ap2->offset->i >= -31) {
                GenerateDiadic(op_dec,0,ap1,ap2);
                return;
            }
            if ((op==op_subui||op==op_subu) && ap2->offset->i < 31) {
                GenerateDiadic(op_dec,0,ap1,ap2);
                return;
            }
        }
    }
    else if (isTable888) {
        if (ap2->mode==am_immed && ssize==8) {
            if ((op==op_addi||op==op_add) && ap2->offset->i < 0x7f && ap2->offset->i > -128) {
                GenerateDiadic(op_inc,0,ap1,ap2);
                return;
            }
            if ((op==op_addui||op==op_addu) && ap2->offset->i < 0x7f) {
                GenerateDiadic(op_inc,0,ap1,ap2);
                return;
            }
        }
    }
    if (ap1->isFloat)
     	ap3 = GetTempFPRegister();
    else
     	ap3 = GetTempRegister();
    GenLoad(ap3,ap1,ssize);
	GenerateTriadic(op,0,ap3,ap3,ap2);
	GenStore(ap3,ap1,ssize);
	ReleaseTempReg(ap3);
}

AMODE *GenerateAssignAdd(ENODE *node,int flags, int size, int op)
{
	AMODE *ap1, *ap2, *ap3;
    int             ssize, mask0, mask1;
    ssize = GetNaturalSize(node->p[0]);
    if( ssize > size )
            size = ssize;
    if (node->etype==bt_double) {
        ap1 = GenerateExpression(node->p[0],F_FPREG|F_MEM,ssize);
        ap2 = GenerateExpression(node->p[1],F_FPREG,size);
        if (op==op_addu || op==op_add)
           op = op_fdadd;
        else if (op==op_subu || op==op_sub)
           op = op_fdsub;
    }
    else {
        ap1 = GenerateExpression(node->p[0],F_ALL,ssize);
        ap2 = GenerateExpression(node->p[1],F_REG | F_IMMED,size);
    }
	if (ap2->mode ==am_immed)
		switch(op) {
		case op_addu:	op = op_addui; break;
		case op_add:	op = op_addi;  break;
		case op_subu:	op = op_subui; break;
		case op_sub:	op = op_subi;  break;
		}
	if (ap1->mode==am_reg) {
	    GenerateTriadic(op,0,ap1,ap1,ap2);
	}
	else if (ap1->mode==am_fpreg) {
	    GenerateTriadic(op,0,ap1,ap1,ap2);
	}
	else {
		GenMemop(op, ap1, ap2, ssize);
	}
    ReleaseTempReg(ap2);
	if (!ap1->isUnsigned)
		GenerateSignExtend(ap1,ssize,size,flags);
    MakeLegalAmode(ap1,flags,size);
    return ap1;
}

AMODE *GenerateAssignLogic(ENODE *node,int flags, int size, int op)
{
	AMODE *ap1, *ap2, *ap3;
    int             ssize, mask0, mask1;
    ssize = GetNaturalSize(node->p[0]);
    if( ssize > size )
            size = ssize;
    ap1 = GenerateExpression(node->p[0],F_ALL,ssize);
    ap2 = GenerateExpression(node->p[1],F_REG | F_IMMED,size);
	if (ap2->mode==am_immed)
		switch(op) {
		case op_and:	op = op_andi; break;
		case op_or:		op = op_ori; break;
		case op_xor:	op = op_xori; break;
		case op_eor:	op = op_eori; break;
		}
	if (ap1->mode==am_reg) {
	    GenerateTriadic(op,0,ap1,ap1,ap2);
	}
	else {
		GenMemop(op, ap1, ap2, ssize);
	}
    ReleaseTempRegister(ap2);
	if (!ap1->isUnsigned)
		GenerateSignExtend(ap1,ssize,size,flags);
    MakeLegalAmode(ap1,flags,size);
    return ap1;
}

//
//      generate a *= node.
//
AMODE *GenerateAssignMultiply(ENODE *node,int flags, int size, int op)
{
	AMODE *ap1, *ap2, *ap3;
    int             ssize, mask0, mask1;
    ssize = GetNaturalSize(node->p[0]);
    if( ssize > size )
            size = ssize;
    if (node->etype==bt_double) {
        ap1 = GenerateExpression(node->p[0],F_FPREG | F_MEM,ssize);
        ap2 = GenerateExpression(node->p[1],F_FPREG,size);
        op = op_fdmul;
    }
    else {
        ap1 = GenerateExpression(node->p[0],F_ALL & ~F_IMMED,ssize);
        ap2 = GenerateExpression(node->p[1],F_REG | F_IMMED,size);
    }
	if (ap2->mode==am_immed)
		switch(op) {
		case op_mulu:	op = op_mului; break;
		case op_mul:	op = op_muli; break;
		}
	if (ap1->mode==am_reg || ap1->mode==am_fpreg) {
	    GenerateTriadic(op,0,ap1,ap1,ap2);
	}
	else {
		GenMemop(op, ap1, ap2, ssize);
	}
    ReleaseTempReg(ap2);
    GenerateSignExtend(ap1,ssize,size,flags);
    MakeLegalAmode(ap1,flags,size);
    return ap1;
}

/*
 *      generate /= and %= nodes.
 */
AMODE *GenerateAssignModiv(ENODE *node,int flags,int size,int op)
{
	AMODE *ap1, *ap2, *ap3;
    int             siz1;
    int isFP;
 
    siz1 = GetNaturalSize(node->p[0]);
    isFP = node->etype==bt_double;
    if (isFP) {
        ap1 = GetTempFPRegister();
        ap2 = GenerateExpression(node->p[0],F_FPREG|F_MEM,siz1);
        if (op==op_divs || op==op_divu)
           op = op_fddiv;
//        else if (op==op_mod || op==op_modu)
//           op = op_fdmod;
    }
    else {
        ap1 = GetTempRegister();
        ap2 = GenerateExpression(node->p[0],F_ALL & ~F_IMMED,siz1);
    }
	if (ap2->mode==am_reg && ap2->preg != ap1->preg)
		GenerateDiadic(op_mov,0,ap1,ap2);
	else if (ap2->mode==am_fpreg && ap2->preg != ap1->preg)
		GenerateDiadic(op_fdmov,0,ap1,ap2);
	else
        GenLoad(ap1,ap2,siz1);
    //GenerateSignExtend(ap1,siz1,8,flags);
    if (isFP)
        ap3 = GenerateExpression(node->p[1],F_FPREG,8);
    else
        ap3 = GenerateExpression(node->p[1],F_REG|F_IMMED,8);
    GenerateTriadic(op,0,ap1,ap1,ap3);
    ReleaseTempReg(ap3);
    //GenerateDiadic(op_ext,0,ap1,0);
	if (ap2->mode==am_reg)
		GenerateDiadic(op_mov,0,ap2,ap1);
	else if (ap2->mode==am_fpreg)
		GenerateDiadic(op_fdmov,0,ap2,ap1);
	else
	    GenStore(ap1,ap2,siz1);
    ReleaseTempReg(ap2);
    MakeLegalAmode(ap1,flags,size);
    return ap1;
}

// ----------------------------------------------------------------------------
//      generate code for an assignment node. if the size of the
//      assignment destination is larger than the size passed then
//      everything below this node will be evaluated with the
//      assignment size.
// ----------------------------------------------------------------------------
AMODE *GenerateAssign(ENODE *node, int flags, int size)
{
	AMODE    *ap1, *ap2 ,*ap3, *ap4;
	AMODE *a1,*a2,*a3,*a4;
        int             ssize;
		ENODE *ep;

    Enter("GenAssign");

    if (node->p[0]->nodetype == en_uwfieldref ||
		node->p[0]->nodetype == en_wfieldref ||
		node->p[0]->nodetype == en_uhfieldref ||
		node->p[0]->nodetype == en_hfieldref ||
		node->p[0]->nodetype == en_ucfieldref ||
		node->p[0]->nodetype == en_cfieldref ||
		node->p[0]->nodetype == en_ubfieldref ||
		node->p[0]->nodetype == en_bfieldref) {

      Leave("GenAssign",0);
		return GenerateBitfieldAssign(node, flags, size);
    }

	ssize = GetReferenceSize(node->p[0]);
//	if( ssize > size )
//			size = ssize;
	if (size > 8) {
		ap1 = GenerateExpression(node->p[0],F_MEM,ssize);
		ap2 = GenerateExpression(node->p[1],F_MEM,size);
	}
	else {
		ap1 = GenerateExpression(node->p[0],F_REG|F_FPREG|F_MEM,ssize);
		if (ap1->mode==am_fpreg || ap1->isFloat) {
    		ap2 = GenerateExpression(node->p[1],F_FPREG|F_MEM,size);
        }
		else
    		ap2 = GenerateExpression(node->p[1],F_ALL,size);
		if (node->p[0]->isUnsigned && !node->p[1]->isUnsigned)
		    GenerateZeroExtend(ap2,size,ssize);
	}
    if (ap1->mode == am_fpreg) {
		if (ap2->mode==am_reg)
			GenerateDiadic(op_mov,0,ap1,ap2);
		else if (ap2->mode==am_fpreg)
			GenerateDiadic(op_mov,0,ap1,ap2);
		else if (ap2->mode==am_immed) {
            MakeLegalAmode(ap2,F_FPREG,8);
			GenerateDiadic(op_mov,0,ap1,ap2);
        }
        else
			GenerateDiadic(op_lfd,0,ap1,ap2);
    }
	else if (ap1->mode == am_reg) {
		if (ap2->mode==am_reg)
			GenerateDiadic(op_mov,0,ap1,ap2);
		else if (ap2->mode==am_fpreg)
			GenerateDiadic(op_mov,0,ap1,ap2);
		else if (ap2->mode==am_immed) {
            if (isFISA64)
                FISA64_GenLdi(ap1,ap2);
            else
      			GenerateDiadic(op_ldi,0,ap1,ap2);
      }
		else {
			if (ap1->isUnsigned) {
				switch(size) {
				case 1:	GenerateDiadic(op_lbu,0,ap1,ap2); break;
				case 2:	GenerateDiadic(op_lcu,0,ap1,ap2); break;
				case 4: GenerateDiadic(op_lhu,0,ap1,ap2); break;
				case 8:	GenerateDiadic(op_lw,0,ap1,ap2); break;
				}
			}
			else {
				switch(size) {
				case 1:	GenerateDiadic(op_lb,0,ap1,ap2); break;
				case 2:	GenerateDiadic(op_lc,0,ap1,ap2); break;
				case 4: GenerateDiadic(op_lh,0,ap1,ap2); break;
				case 8:	GenerateDiadic(op_lw,0,ap1,ap2); break;
				}
				if (ssize > size) {
					switch(size) {
					case 1:	GenerateDiadic(op_sxb,0,ap1,ap1); break;
					case 2:	GenerateDiadic(op_sxc,0,ap1,ap1); break;
					case 4: GenerateDiadic(op_sxh,0,ap1,ap1); break;
					}
				}
			}
		}
	}
	// ap1 is memory
	else {
		if (ap2->mode == am_reg) {
		    GenStore(ap2,ap1,ssize);
        }
		else if (ap2->mode == am_fpreg) {
		    GenStore(ap2,ap1,ssize);
        }
		else if (ap2->mode == am_immed) {
            if (ap2->offset->i == 0) {
                GenStore(makereg(0),ap1,ssize);
            }
            else {
    			ap3 = GetTempRegister();
    			if (isFISA64)
    			    FISA64_GenLdi(ap3,ap2);
    			else
                    GenerateDiadic(op_ldi,0,ap3,ap2);
                GenStore(ap3,ap1,ssize);
		    	ReleaseTempReg(ap3);
            }
		}
		else {
			ap3 = GetTempRegister();
			// Generate a memory to memory move (struct assignments)
			if (ssize > 8) {
				ap3 = GetTempRegister();
				GenerateDiadic(op_ldi,0,ap3,make_immed(size));
				GenerateTriadic(op_push,0,ap3,ap2,ap1);
				GenerateDiadic(op_jal,0,makereg(LR),make_string("memcpy_"));
				GenerateTriadic(op_addui,0,makereg(SP),makereg(SP),make_immed(24));
				ReleaseTempRegister(ap3);
			}
			else {
				if (ap1->isUnsigned) {
					switch(size) {
					case 1:	GenerateDiadic(op_lbu,0,ap3,ap2); break;
					case 2:	GenerateDiadic(op_lcu,0,ap3,ap2); break;
					case 4: GenerateDiadic(op_lhu,0,ap3,ap2); break;
					case 8:	GenerateDiadic(op_lw,0,ap3,ap2); break;
					}
				}
				else {
					switch(size) {
					case 1:	GenerateDiadic(op_lb,0,ap3,ap2); break;
					case 2:	GenerateDiadic(op_lc,0,ap3,ap2); break;
					case 4: GenerateDiadic(op_lh,0,ap3,ap2); break;
					case 8:	GenerateDiadic(op_lw,0,ap3,ap2); break;
					}
					if (ssize > size) {
						switch(size) {
						case 1:	GenerateDiadic(op_sxb,0,ap3,ap3); break;
						case 2:	GenerateDiadic(op_sxc,0,ap3,ap3); break;
						case 4: GenerateDiadic(op_sxh,0,ap3,ap3); break;
						}
					}
				}
				GenStore(ap3,ap1,ssize);
				ReleaseTempRegister(ap3);
			}
		}
	}
/*
	if (ap1->mode == am_reg) {
		if (ap2->mode==am_immed)	// must be zero
			GenerateDiadic(op_mov,0,ap1,makereg(0));
		else
			GenerateDiadic(op_mov,0,ap1,ap2);
	}
	else {
		if (ap2->mode==am_immed)
		switch(size) {
		case 1:	GenerateDiadic(op_sb,0,makereg(0),ap1); break;
		case 2:	GenerateDiadic(op_sc,0,makereg(0),ap1); break;
		case 4: GenerateDiadic(op_sh,0,makereg(0),ap1); break;
		case 8:	GenerateDiadic(op_sw,0,makereg(0),ap1); break;
		}
		else
		switch(size) {
		case 1:	GenerateDiadic(op_sb,0,ap2,ap1); break;
		case 2:	GenerateDiadic(op_sc,0,ap2,ap1); break;
		case 4: GenerateDiadic(op_sh,0,ap2,ap1); break;
		case 8:	GenerateDiadic(op_sw,0,ap2,ap1); break;
		// Do structure assignment
		default: {
			ap3 = GetTempRegister();
			GenerateDiadic(op_ldi,0,ap3,make_immed(size));
			GenerateTriadic(op_push,0,ap3,ap2,ap1);
			GenerateDiadic(op_jal,0,makereg(LR),make_string("memcpy"));
			GenerateTriadic(op_addui,0,makereg(SP),makereg(SP),make_immed(24));
			ReleaseTempRegister(ap3);
		}
		}
	}
*/
	ReleaseTempReg(ap2);
    MakeLegalAmode(ap1,flags,size);
    Leave("GenAssign",1);
	return ap1;
}

/*
 *      generate an auto increment or decrement node. op should be
 *      either op_add (for increment) or op_sub (for decrement).
 */
AMODE *GenerateAutoIncrement(ENODE *node,int flags,int size,int op)
{
	AMODE *ap1, *ap2;
    int siz1;

    siz1 = GetNaturalSize(node->p[0]);
    if( flags & F_NOVALUE )         /* dont need result */
            {
            ap1 = GenerateExpression(node->p[0],F_ALL,siz1);
			switch(op) {
			case op_addu:	op = op_addui; break;
			case op_add:	op = op_addi;  break;
			case op_subu:	op = op_subui; break;
			case op_sub:	op = op_subi;  break;
			}
			if (ap1->mode != am_reg) {
                GenMemop(op, ap1, make_immed(node->i), size)
                ;
/*
				ap2 = GetTempRegister();
				if (ap1->isUnsigned) {
					switch(size) {
					case 1:	GenerateDiadic(op_lbu,0,ap2,ap1); break;
					case 2:	GenerateDiadic(op_lcu,0,ap2,ap1); break;
					case 4:	GenerateDiadic(op_lhu,0,ap2,ap1); break;
					case 8:	GenerateDiadic(op_lw,0,ap2,ap1); break;
					}
				}
				else {
					switch(size) {
					case 1:	GenerateDiadic(op_lb,0,ap2,ap1); break;
					case 2:	GenerateDiadic(op_lc,0,ap2,ap1); break;
					case 4:	GenerateDiadic(op_lh,0,ap2,ap1); break;
					case 8:	GenerateDiadic(op_lw,0,ap2,ap1); break;
					}
				}
	            GenerateTriadic(op,0,ap2,ap2,make_immed(node->i));
				switch(size) {
				case 1:	GenerateDiadic(op_sb,0,ap2,ap1); break;
				case 2:	GenerateDiadic(op_sc,0,ap2,ap1); break;
				case 4:	GenerateDiadic(op_sh,0,ap2,ap1); break;
				case 8:	GenerateDiadic(op_sw,0,ap2,ap1); break;
				}
				ReleaseTempRegister(ap2);
*/
			}
			else
				GenerateTriadic(op,0,ap1,ap1,make_immed(node->i));
            //ReleaseTempRegister(ap1);
            return ap1;
            }
    ap2 = GenerateExpression(node->p[0],F_ALL,siz1);
	if (ap2->mode == am_reg) {
	    GenerateTriadic(op,0,ap2,ap2,make_immed(node->i));
		return ap2;
	}
	else {
//	    ap1 = GetTempRegister();
		switch(op) {
		case op_addu:	op = op_addui; break;
		case op_add:	op = op_addi;  break;
		case op_subu:	op = op_subui; break;
		case op_sub:	op = op_subi;  break;
		}
        GenMemop(op, ap2, make_immed(node->i), siz1);
        return ap2;
        GenLoad(ap1,ap2,siz1);
		GenerateTriadic(op,0,ap1,ap1,make_immed(node->i));
		GenStore(ap1,ap2,siz1);
//		ReleaseTempRegister(ap1);
	}
    //ReleaseTempRegister(ap2);
    //GenerateSignExtend(ap1,siz1,size,flags);
    return ap2;
}

/*
 *      general expression evaluation. returns the addressing mode
 *      of the result.
 */
AMODE *GenerateExpression(ENODE *node, int flags, int size)
{   
	AMODE *ap1, *ap2, *ap3;
    int lab0, lab1;
    int natsize;
	static char buf[4][20];
	static int ndx;
	static int numDiags = 0;

//    Enter("GenExperssion"); 
    if( node == (ENODE *)NULL )
    {
		numDiags++;
        printf("DIAG - null node in GenerateExpression.\n");
		if (numDiags > 100)
			exit(0);
         Leave("GenExperssion",1); 
        return (AMODE *)NULL;
    }
	//size = node->esize;
    switch( node->nodetype )
    {
	case en_fcon:
            ap1 = allocAmode();
            ap1->mode = am_immed;
            ap1->offset = node;
			ap1->isFloat = TRUE;
            MakeLegalAmode(ap1,flags,size);
         Leave("GenExperssion",2); 
            return ap1;
    case en_icon:
            ap1 = allocAmode();
            ap1->mode = am_immed;
            ap1->offset = node;
            MakeLegalAmode(ap1,flags,size);
         Leave("GenExperssion",3); 
            return ap1;
    case en_labcon:
            if (use_gp) {
                ap1 = GetTempRegister();
                ap2 = allocAmode();
                ap2->mode = am_indx;
                ap2->preg = regGP;      // global pointer
                ap2->offset = node;     // use as constant node
                GenerateDiadic(op_lea,0,ap1,ap2);
                MakeLegalAmode(ap1,flags,size);
         Leave("GenExperssion",4); 
                return ap1;             // return reg
            }
            ap1 = allocAmode();
			/* this code not really necessary, see segments notes
			if (node->etype==bt_pointer && node->constflag) {
				ap1->segment = codeseg;
			}
			else {
				ap1->segment = dataseg;
			}
			*/
            ap1->mode = am_immed;
            ap1->offset = node;
			ap1->isUnsigned = node->isUnsigned;
            MakeLegalAmode(ap1,flags,size);
         Leave("GenExperssion",5); 
            return ap1;

    case en_nacon:
            if (use_gp) {
                ap1 = GetTempRegister();
                ap2 = allocAmode();
                ap2->mode = am_indx;
                ap2->preg = regGP;      // global pointer
                ap2->offset = node;     // use as constant node
                GenerateDiadic(op_lea,0,ap1,ap2);
                MakeLegalAmode(ap1,flags,size);
         Leave("GenExperssion",6); 
                return ap1;             // return reg
            }
            // fallthru
	case en_cnacon:
	case en_clabcon:
            ap1 = allocAmode();
            ap1->mode = am_immed;
            ap1->offset = node;
			ap1->isUnsigned = node->isUnsigned;
            MakeLegalAmode(ap1,flags,size);
         Leave("GenExperssion",7); 
            return ap1;
    case en_autocon:
            ap1 = GetTempRegister();
            ap2 = allocAmode();
            ap2->mode = am_indx;
            ap2->preg = regBP;          /* frame pointer */
            ap2->offset = node;     /* use as constant node */
            GenerateDiadic(op_lea,0,ap1,ap2);
            MakeLegalAmode(ap1,flags,size);
            return ap1;             /* return reg */
    case en_autofcon:
            ap1 = GetTempRegister();
            ap2 = allocAmode();
            ap2->mode = am_indx;
            ap2->preg = regBP;          /* frame pointer */
            ap2->offset = node;     /* use as constant node */
            ap2->isFloat = TRUE;
            GenerateDiadic(op_lea,0,ap1,ap2);
            MakeLegalAmode(ap1,flags,size);
            return ap1;             /* return reg */
    case en_ub_ref:
	case en_uc_ref:
	case en_uh_ref:
	case en_uw_ref:
	case en_struct_ref:
			ap1 = GenerateDereference(node,flags,size,0);
			ap1->isUnsigned = TRUE;
            return ap1;
    case en_b_ref:
	case en_c_ref:
	case en_h_ref:
    case en_w_ref:
			ap1 = GenerateDereference(node,flags,size,1);
            return ap1;
	case en_flt_ref:
	case en_dbl_ref:
    case en_triple_ref:
			ap1 = GenerateDereference(node,flags,size,1);
			ap1->isFloat = TRUE;
            return ap1;
	case en_ubfieldref:
	case en_ucfieldref:
	case en_uhfieldref:
	case en_uwfieldref:
			ap1 = GenerateBitfieldDereference(node,flags,size);
			ap1->isUnsigned = TRUE;
			return ap1;
	case en_wfieldref:
	case en_bfieldref:
	case en_cfieldref:
	case en_hfieldref:
			ap1 = GenerateBitfieldDereference(node,flags,size);
			return ap1;
	case en_bregvar:
            ap1 = allocAmode();
            ap1->mode = am_breg;
            ap1->preg = node->i;
            ap1->tempflag = 0;      /* not a temporary */
            MakeLegalAmode(ap1,flags,size);
            return ap1;
	case en_regvar:
    case en_tempref:
            ap1 = allocAmode();
            ap1->mode = am_reg;
            ap1->preg = node->i;
            ap1->tempflag = 0;      /* not a temporary */
            MakeLegalAmode(ap1,flags,size);
            return ap1;
    case en_tempfpref:
            ap1 = allocAmode();
            ap1->mode = am_fpreg;
            ap1->preg = node->i;
            ap1->tempflag = 0;      /* not a temporary */
            MakeLegalAmode(ap1,flags,size);
            return ap1;
	case en_fpregvar:
//    case en_fptempref:
            ap1 = allocAmode();
            ap1->mode = am_fpreg;
            ap1->preg = node->i;
            ap1->tempflag = 0;      /* not a temporary */
            MakeLegalAmode(ap1,flags,size);
            return ap1;
    case en_uminus: return GenerateUnary(node,flags,size,op_neg);
    case en_compl:  return GenerateUnary(node,flags,size,op_com);
    case en_add:    return GenerateBinary(node,flags,size,op_addu);
    case en_sub:    return GenerateBinary(node,flags,size,op_subu);
    case en_i2d:
         ap1 = GetTempFPRegister();	
         ap2=GenerateExpression(node->p[0],F_REG,8);
         GenerateDiadic(op_mv2flt,0,ap1,ap2);
         ReleaseTempReg(ap2);
         return ap1;
    case en_d2i:
         ap1 = GetTempRegister();	
         ap2 = GenerateExpression(node->p[0],F_FPREG,8);
         GenerateDiadic(op_mv2fix,0,ap1,ap2);
         ReleaseTempReg(ap2);
         return ap1;
    case en_i2t:	return GenerateUnary(node,flags,size,op_i2t);

	case en_fdadd:    return GenerateBinary(node,flags,size,op_fdadd);
    case en_fdsub:    return GenerateBinary(node,flags,size,op_fdsub);
    case en_fsadd:    return GenerateBinary(node,flags,size,op_fsadd);
    case en_fssub:    return GenerateBinary(node,flags,size,op_fssub);
    case en_fdmul:    return GenerateMultiply(node,flags,size,op_fdmul);
    case en_fsmul:    return GenerateMultiply(node,flags,size,op_fsmul);
    case en_fddiv:    return GenerateMultiply(node,flags,size,op_fddiv);
    case en_fsdiv:    return GenerateMultiply(node,flags,size,op_fsdiv);
	case en_ftadd:    return GenerateBinary(node,flags,size,op_ftadd);
    case en_ftsub:    return GenerateBinary(node,flags,size,op_ftsub);
    case en_ftmul:    return GenerateMultiply(node,flags,size,op_ftmul);
    case en_ftdiv:    return GenerateMultiply(node,flags,size,op_ftdiv);

	case en_and:    return GenerateBinary(node,flags,size,op_and);
    case en_or:     return GenerateBinary(node,flags,size,op_or);
	case en_xor:	return GenerateBinary(node,flags,size,op_xor);
    case en_mul:    return GenerateMultiply(node,flags,size,op_mul);
    case en_mulu:   return GenerateMultiply(node,flags,size,op_mulu);
    case en_div:    return GenerateModDiv(node,flags,size,op_divs);
    case en_udiv:   return GenerateModDiv(node,flags,size,op_divu);
    case en_mod:    return GenerateModDiv(node,flags,size,op_mod);
    case en_umod:   return GenerateModDiv(node,flags,size,op_modu);
    case en_shl:    return GenerateShift(node,flags,size,isFISA64?op_asl:op_shl);
    case en_shlu:   return GenerateShift(node,flags,size,isFISA64?op_asl:op_shlu);
    case en_asr:	return GenerateShift(node,flags,size,isFISA64?op_asr:op_asr);
    case en_shr:	return GenerateShift(node,flags,size,isFISA64?op_asr:op_asr);
    case en_shru:   return GenerateShift(node,flags,size,isFISA64?op_lsr:op_shru);
    case en_asadd:  return GenerateAssignAdd(node,flags,size,op_addu);
    case en_assub:  return GenerateAssignAdd(node,flags,size,op_subu);
    case en_asand:  return GenerateAssignLogic(node,flags,size,op_and);
    case en_asor:   return GenerateAssignLogic(node,flags,size,op_or);
	case en_asxor:  return GenerateAssignLogic(node,flags,size,op_xor);
    case en_aslsh:
            return GenerateAssignShift(node,flags,size,isFISA64?op_asl:op_shl);
    case en_asrsh:
            return GenerateAssignShift(node,flags,size,op_asr);
    case en_asrshu:
            return GenerateAssignShift(node,flags,size,isFISA64?op_lsr:op_shru);
    case en_asmul: return GenerateAssignMultiply(node,flags,size,op_mul);
    case en_asmulu: return GenerateAssignMultiply(node,flags,size,op_mulu);
    case en_asdiv: return GenerateAssignModiv(node,flags,size,op_divs);
    case en_asdivu: return GenerateAssignModiv(node,flags,size,op_divu);
    case en_asmod: return GenerateAssignModiv(node,flags,size,op_mod);
    case en_asmodu: return GenerateAssignModiv(node,flags,size,op_modu);
    case en_assign:
            return GenerateAssign(node,flags,size);
    case en_ainc: return GenerateAutoIncrement(node,flags,size,op_addu);
    case en_adec: return GenerateAutoIncrement(node,flags,size,op_subu);
    case en_land:
        if (isFISA64)
            return GenExprFISA64(node);
        else if (is816)
            return GenExpr816(node);
		else if (isRaptor64)
			return GenExprRaptor64(node);
		size = GetNaturalSize(node);
		ap3 = GetTempRegister();
		ap1 = GenerateExpression(node->p[0],F_REG, size);
		ap2 = GenerateExpression(node->p[1],F_REG|F_IMMED,size);
		GenerateTriadic(op_and,0,ap3,ap1,ap2);
		ReleaseTempRegister(ap2);
		ReleaseTempRegister(ap1);
		if (isTable888) {
            lab0 = nextlabel++;
			GenerateDiadic(op_brz,0,ap3,make_clabel(lab0));
			GenerateDiadic(op_ldi,0,ap3,make_immed(1));
			GenerateLabel(lab0);
			return ap3;
		}
		else {
			GenerateDiadic(op_tst,0,makepred(15),ap3);
			GeneratePredicatedDiadic(PredOp(op_ne),15,op_ldi,0,ap3,make_immed(1));
			GeneratePredicatedDiadic(PredOp(op_eq),15,op_ldi,0,ap3,make_immed(0));
			return ap3;
		}
	case en_lor:
        if (isFISA64)
            return GenExprFISA64(node);
        else if (is816)
            return GenExpr816(node);
		else if (isRaptor64)
			return GenExprRaptor64(node);
		size = GetNaturalSize(node);
		ap3 = GetTempRegister();
		ap1 = GenerateExpression(node->p[0],F_REG, size);
		ap2 = GenerateExpression(node->p[1],F_REG|F_IMMED,size);
		GenerateTriadic(op_or,0,ap3,ap1,ap2);
		ReleaseTempRegister(ap2);
		ReleaseTempRegister(ap1);
		if (isTable888) {
            lab0 = nextlabel++;
			GenerateDiadic(op_brz,0,ap3,make_clabel(lab0));
			GenerateDiadic(op_ldi,0,ap3,make_immed(1));
			GenerateLabel(lab0);
		}
		else {
			GenerateDiadic(op_tst,0,makepred(15),ap3);
			GeneratePredicatedDiadic(PredOp(op_ne),15,op_ldi,0,ap3,make_immed(1));
			GeneratePredicatedDiadic(PredOp(op_eq),15,op_ldi,0,ap3,make_immed(0));
		}
        return ap3;
	case en_not:
        if (isFISA64)
            return GenExprFISA64(node);
        else if (is816)
            return GenExpr816(node);
		else if (isRaptor64)
			return GenExprRaptor64(node);
		size = GetNaturalSize(node);
		ap1 = GenerateExpression(node->p[0],F_REG,size);
		if (isTable888) {
			GenerateDiadic(op_not,0,ap1,ap1);
		}
		else {
			GenerateDiadic(op_tst,0,makepred(15),ap1);
			GeneratePredicatedDiadic(PredOp(op_ne),15,op_ldi,0,ap1,make_immed(0));
			GeneratePredicatedDiadic(PredOp(op_eq),15,op_ldi,0,ap1,make_immed(1));
		}
        return ap1;
    case en_chk:
        return GenExprFISA64(node);
         
    case en_eq:     case en_ne:
    case en_lt:     case en_le:
    case en_gt:     case en_ge:
    case en_ult:    case en_ule:
    case en_ugt:    case en_uge:
                case en_feq:    case en_fne:
                case en_flt:    case en_fle:
                case en_fgt:    case en_fge:
        if (isFISA64)
            return GenExprFISA64(node);
        else if (is816)
            return GenExpr816(node);
		else if (isRaptor64)
			return GenExprRaptor64(node);
		else if (isTable888)
			return GenTable888Set(node);
		//{
  //          lab0 = nextlabel++;
  //          lab1 = nextlabel++;
  //          GenerateFalseJump(node,lab0,0);
  //          ap1 = GetTempRegister();
  //          GenerateDiadic(op_ldi,0,ap1,make_immed(1));
  //          GenerateTriadic(op_bra,0,make_clabel(lab1),NULL,NULL);
  //          GenerateLabel(lab0);
  //          GenerateDiadic(op_ldi,0,ap1,make_immed(0));
  //          GenerateLabel(lab1);
  //          return ap1;
		//}
		else {
			size = GetNaturalSize(node);
			ap1 = GenerateExpression(node->p[0],F_REG, size);
			ap2 = GenerateExpression(node->p[1],F_REG|F_IMMED,size);
			GenerateTriadic(op_cmp,0,makepred(15),ap1,ap2);
			ReleaseTempRegister(ap2);
			ReleaseTempRegister(ap1);
			ap3 = GetTempRegister();
			GeneratePredicatedDiadic(PredOp(op_ne),15,op_ldi,0,ap3,make_immed(1));
			GeneratePredicatedDiadic(PredOp(op_eq),15,op_ldi,0,ap3,make_immed(0));
		}
        return ap3;
    case en_cond:
            return gen_hook(node,flags,size);
    case en_void:
            natsize = GetNaturalSize(node->p[0]);
            ReleaseTempRegister(GenerateExpression(node->p[0],F_ALL | F_NOVALUE,natsize));
            return GenerateExpression(node->p[1],flags,size);
    case en_fcall:
            if (isFISA64)
				return GenerateFISA64FunctionCall(node,flags);
            else if (is816)
				return Generate816FunctionCall(node,flags);
			else if (isRaptor64)
				return GenerateRaptor64FunctionCall(node,flags);
			else if (isTable888)
				return GenerateTable888FunctionCall(node,flags);
			else
				return GenerateFunctionCall(node,flags);
	case en_cubw:
	case en_cubu:
	case en_cbu:
			ap1 = GenerateExpression(node->p[0],F_REG,size);
			GenerateTriadic(op_andi,0,ap1,ap1,make_immed(0xff));
			return ap1;
	case en_cucw:
	case en_cucu:
	case en_ccu:
			ap1 = GenerateExpression(node->p[0],F_REG,size);
			GenerateTriadic(op_andi,0,ap1,ap1,make_immed(0xffff));
			return ap1;
	case en_cuhw:
	case en_cuhu:
	case en_chu:
			ap1 = GenerateExpression(node->p[0],F_REG,size);
			GenerateTriadic(op_andi,0,ap1,ap1,make_immed(0xffffffffL));
			return ap1;
	case en_cbw:
			ap1 = GenerateExpression(node->p[0],F_REG,size);
			GenerateDiadic(op_sxb,0,ap1,ap1);
			return ap1;
	case en_ccw:
			ap1 = GenerateExpression(node->p[0],F_REG,size);
			GenerateDiadic(op_sxc,0,ap1,ap1);
			return ap1;
	case en_chw:
			ap1 = GenerateExpression(node->p[0],F_REG,size);
			GenerateDiadic(op_sxh,0,ap1,ap1);
			return ap1;
    default:
            printf("DIAG - uncoded node (%d) in GenerateExpression.\n", node->nodetype);
            return 0;
    }
}

/*
 *      return the natural evaluation size of a node.
 */
int GetNaturalSize(ENODE *node)
{ 
	int     siz0, siz1;
    if( node == NULL )
            return 0;
    switch( node->nodetype )
        {
		case en_uwfieldref:
		case en_wfieldref:
			return 8;
		case en_bfieldref:
		case en_ubfieldref:
			return 1;
		case en_cfieldref:
		case en_ucfieldref:
			return 2;
		case en_hfieldref:
		case en_uhfieldref:
			return 4;
        case en_icon:
                if( -128 <= node->i && node->i <= 127 )
                        return 1;
                if( -32768 <= node->i && node->i <= 32767 )
                        return 2;
				if (-2147483648LL <= node->i && node->i <= 2147483647LL)
					return 4;
				return 8;
		case en_fcon:
				return 8;
	    case en_tcon: return 12;
		case en_fcall:  case en_labcon: case en_clabcon:
		case en_cnacon: case en_nacon:  case en_autocon: case en_autofcon:
		case en_tempref:
		case en_regvar:
        case en_fpregvar:
		case en_cbw: case en_cubw:
		case en_ccw: case en_cucw:
		case en_chw: case en_cuhw:
		case en_cbu: case en_ccu: case en_chu:
		case en_cubu: case en_cucu: case en_cuhu:
                return 8;
		case en_b_ref:
		case en_ub_ref:
                return 1;
        case en_cbc:
		case en_c_ref:	return 2;
		case en_uc_ref:	return 2;
		case en_cbh:	return 4;
		case en_cch:	return 4;
		case en_h_ref:	return 4;
		case en_uh_ref:	return 4;
		case en_flt_ref: return 4;
		case en_w_ref:  case en_uw_ref:
		case en_dbl_ref:
                return 8;
		case en_triple_ref:
                return 12;
		case en_struct_ref:
				return node->esize;
        case en_not:    case en_compl:
        case en_uminus: case en_assign:
        case en_ainc:   case en_adec:
                return GetNaturalSize(node->p[0]);
		case en_fdadd:	case en_fdsub:
		case en_fdmul:	case en_fddiv:
		case en_fsadd:	case en_fssub:
		case en_fsmul:	case en_fsdiv:
        case en_add:    case en_sub:
		case en_mul:    case en_mulu:
		case en_div:	case en_udiv:
		case en_mod:    case en_umod:
		case en_and:    case en_or:     case en_xor:
		case en_shl:    case en_shlu:
		case en_shr:	case en_shru:
		case en_asr:	case en_asrshu:
                case en_feq:    case en_fne:
                case en_flt:    case en_fle:
                case en_fgt:    case en_fge:
        case en_eq:     case en_ne:
        case en_lt:     case en_le:
        case en_gt:     case en_ge:
		case en_ult:	case en_ule:
		case en_ugt:	case en_uge:
        case en_land:   case en_lor:
        case en_asadd:  case en_assub:
		case en_asmul:  case en_asmulu:
		case en_asdiv:	case en_asdivu:
        case en_asmod:  case en_asand:
		case en_asor:   case en_asxor:	case en_aslsh:
        case en_asrsh:
                siz0 = GetNaturalSize(node->p[0]);
                siz1 = GetNaturalSize(node->p[1]);
                if( siz1 > siz0 )
                    return siz1;
                else
                    return siz0;
        case en_void:   case en_cond:
                return GetNaturalSize(node->p[1]);
        case en_chk:
             return 8;
        default:
                printf("DIAG - natural size error %d.\n", node->nodetype);
                break;
        }
    return 0;
}


static void GenerateCmp(ENODE *node, int op, int label, int predreg)
{
     Enter("GenCmp");
    if (isFISA64)
		GenerateFISA64Cmp(node, op, label, 0);
    else if (is816)
		Generate816Cmp(node, op, label, 0);
	else if (isRaptor64)
		GenerateRaptor64Cmp(node, op, label, 0);
	else if (isTable888)
		GenerateTable888Cmp(node, op, label, 0);
	else
		GenerateThorCmp(node, op, label, predreg);
     Leave("GenCmp",0);
}

/*
 *      generate a jump to label if the node passed evaluates to
 *      a true condition.
 */
void GenerateTrueJump(ENODE *node, int label, int predreg)
{       AMODE  *ap1,*ap2;
        int             siz1;
        int             lab0;
		int size;
        if( node == 0 )
                return;
        switch( node->nodetype )
                {
                case en_eq:	GenerateCmp(node, op_eq, label, predreg); break;
                case en_ne: GenerateCmp(node, op_ne, label, predreg); break;
                case en_lt: GenerateCmp(node, op_lt, label, predreg); break;
                case en_le:	GenerateCmp(node, op_le, label, predreg); break;
                case en_gt: GenerateCmp(node, op_gt, label, predreg); break;
                case en_ge: GenerateCmp(node, op_ge, label, predreg); break;
                case en_ult: GenerateCmp(node, op_ltu, label, predreg); break;
                case en_ule: GenerateCmp(node, op_leu, label, predreg); break;
                case en_ugt: GenerateCmp(node, op_gtu, label, predreg); break;
                case en_uge: GenerateCmp(node, op_geu, label, predreg); break;
                case en_land:
                        lab0 = nextlabel++;
                        GenerateFalseJump(node->p[0],lab0, predreg);
                        GenerateTrueJump(node->p[1],label, predreg);
                        GenerateLabel(lab0);
                        break;
                case en_lor:
                        GenerateTrueJump(node->p[0],label, predreg);
                        GenerateTrueJump(node->p[1],label, predreg);
                        break;
                case en_not:
                        GenerateFalseJump(node->p[0],label, predreg);
                        break;
                default:
                        siz1 = GetNaturalSize(node);
                        ap1 = GenerateExpression(node,F_REG,siz1);
//                        GenerateDiadic(op_tst,siz1,ap1,0);
                        ReleaseTempRegister(ap1);
                        if (isFISA64)
							GenerateDiadic(op_bne,0,ap1,make_clabel(label));
                        else if (is816) {
                            GenerateMonadic(op_lda,0,ap1);
                            GenerateMonadic(op_ora,0,makereg(ap1->preg+2));
                            GenerateMonadic(op_bne,0,make_clabel(label));
                        }
						else if (isTable888)
							GenerateDiadic(op_brnz,0,ap1,make_clabel(label));
						else if (isRaptor64)
							GenerateTriadic(op_bne,0,ap1,makereg(0),make_label(label));
						else {
							GenerateDiadic(op_tst,0,makepred(predreg),ap1);
							GeneratePredicatedMonadic(predreg,PredOp(op_ne),op_bra,0,make_clabel(label));
						}
                        break;
                }
}

/*
 *      generate code to execute a jump to label if the expression
 *      passed is false.
 */
void GenerateFalseJump(ENODE *node,int label, int predreg)
{
	AMODE *ap,*ap1,*ap2;
		int size;
        int             siz1;
        int             lab0;
        if( node == (ENODE *)NULL )
                return;
        switch( node->nodetype )
                {
                case en_eq:	GenerateCmp(node, op_ne, label, predreg); break;
                case en_ne: GenerateCmp(node, op_eq, label, predreg); break;
                case en_lt: GenerateCmp(node, op_ge, label, predreg); break;
                case en_le: GenerateCmp(node, op_gt, label, predreg); break;
                case en_gt: GenerateCmp(node, op_le, label, predreg); break;
                case en_ge: GenerateCmp(node, op_lt, label, predreg); break;
                case en_ult: GenerateCmp(node, op_geu, label, predreg); break;
                case en_ule: GenerateCmp(node, op_gtu, label, predreg); break;
                case en_ugt: GenerateCmp(node, op_leu, label, predreg); break;
                case en_uge: GenerateCmp(node, op_ltu, label, predreg); break;
                case en_feq:	GenerateCmp(node, op_fne, label, predreg); break;
                case en_fne: GenerateCmp(node, op_feq, label, predreg); break;
                case en_flt: GenerateCmp(node, op_fge, label, predreg); break;
                case en_fle: GenerateCmp(node, op_fgt, label, predreg); break;
                case en_fgt: GenerateCmp(node, op_fle, label, predreg); break;
                case en_fge: GenerateCmp(node, op_flt, label, predreg); break;
                case en_land:
                        GenerateFalseJump(node->p[0],label, predreg);
                        GenerateFalseJump(node->p[1],label, predreg);
                        break;
                case en_lor:
                        lab0 = nextlabel++;
                        GenerateTrueJump(node->p[0],lab0, predreg);
                        GenerateFalseJump(node->p[1],label, predreg);
                        GenerateLabel(lab0);
                        break;
                case en_not:
                        GenerateTrueJump(node->p[0],label, predreg);
                        break;
                default:
                        siz1 = GetNaturalSize(node);
                        ap = GenerateExpression(node,F_REG,siz1);
//                        GenerateDiadic(op_tst,siz1,ap,0);
                        ReleaseTempRegister(ap);
                        if (isFISA64)
							GenerateDiadic(op_beq,0,ap,make_clabel(label));
                        else if (is816) {
                            GenerateMonadic(op_lda,0,ap);
                            GenerateMonadic(op_ora,0,makereg(ap->preg+2));
                            GenerateMonadic(op_beq,0,make_clabel(label));
                        }
						else if (isTable888)
							GenerateDiadic(op_brz,0,ap,make_clabel(label));
						else if (isRaptor64)
							GenerateTriadic(op_beq,0,ap,makereg(0),make_label(label));
						else {
							GenerateDiadic(op_tst,0,makepred(predreg),ap);
							GeneratePredicatedMonadic(predreg,PredOp(op_eq),op_bra,0,make_clabel(label));
						}
                        break;
                }
}
