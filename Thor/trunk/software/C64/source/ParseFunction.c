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
#include        <stdio.h>
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

extern int funcdecl;
extern char *names[20];
extern int nparms;

void ParseFunctionBody(SYM *sp);
void funcbottom();

/*      function compilation routines           */

/*
 *      funcbody starts with the current symbol being either
 *      the first parameter id or the begin for the local
 *      block. If begin is the current symbol then funcbody
 *      assumes that the function has no parameters.
 */
void ParseFunction(SYM *sp)
{
	int poffset, i;
	int oldglobal;
    SYM *sp1, *sp2, *makeint();

		oldglobal = global_flag;
        global_flag = 0;
        poffset = 32;            /* size of return block */
        nparms = 0;
		iflevel = 0;
        if(lastst == id || 1) {              /* declare parameters */
                //while(lastst == id) {
                //        names[nparms++] = litlate(lastid);
                //        NextToken();
                //        if( lastst == comma)
                //                NextToken();
                //        else
                //                break;
                //        }
                //needpunc(closepa);
//                dodecl(sc_member);      /* declare parameters */
				sp->parms = NULL;
				ParseParameterDeclarations(1);
                for(i = 0;i < nparms;++i) {
                        if( (sp1 = search(names[i],&lsyms)) == NULL)
                                sp1 = makeint(names[i]);
						//if( sp1->tp->size < 8 )
						//{
						//	sp1->value.i = poffset;// + (8 - sp1->tp->size);
						//	poffset += 8;
						//}
						//else
						//{
						//	sp1->value.i = poffset;
						//	poffset += sp1->tp->size;
						//}
						sp1->value.i = poffset;
						poffset += 8;
                        sp1->storage_class = sc_auto;
						sp1->nextparm = NULL;
						// record parameter list
						if (sp->parms == NULL) {
							sp->parms = sp1;
						}
						else {
							sp1->nextparm = sp->parms;
							sp->parms = sp1;
						}
					}
                }
		if (lastst == closepa)
			NextToken();
		if (lastst == semicolon) {	// Function prototype
			sp->IsPrototype = 1;
			sp->IsNocall = isNocall;
			sp->IsPascal = isPascal;
			sp->IsInterrupt = isInterrupt;
			sp->NumParms = nparms;
			isPascal = FALSE;
			isOscall = FALSE;
			isInterrupt = FALSE;
			isNocall = FALSE;
		    ReleaseLocalMemory();        /* release local symbols (parameters)*/
		}
		else if(lastst != begin) {
			NextToken();
			ParseParameterDeclarations(2);
			sp->IsNocall = isNocall;
			sp->IsPascal = isPascal;
			sp->IsInterrupt = isInterrupt;
			sp->NumParms = nparms;
			ParseFunctionBody(sp);
			funcbottom();
		}
//                error(ERR_BLOCK);
        else {
			sp->IsNocall = isNocall;
			sp->IsPascal = isPascal;
			sp->IsInterrupt = isInterrupt;
			sp->NumParms = nparms;
			ParseFunctionBody(sp);
			funcbottom();
        }
		global_flag = oldglobal;
}

SYM     *makeint(char *name)
{       SYM     *sp;
        TYP     *tp;
        sp = allocSYM();
        tp = allocTYP();
        tp->type = bt_long;
        tp->size = 8;
        tp->btp = 0;
		tp->lst.head = 0;
        tp->sname = 0;
        sp->name = name;
        sp->storage_class = sc_auto;
        sp->tp = tp;
		sp->IsPrototype = FALSE;
        insert(sp,&lsyms);
        return sp;
}

void check_table(SYM *head)
{   
	while( head != 0 ) {
		if( head->storage_class == sc_ulabel )
				fprintf(list,"*** UNDEFINED LABEL - %s\n",head->name);
		head = head->next;
	}
}

void funcbottom()
{ 
	nl();
    check_table(&lsyms);
    lc_auto = 0;
    fprintf(list,"\n\n*** local symbol table ***\n\n");
    ListTable(&lsyms,0);
    fprintf(list,"\n\n\n");
    ReleaseLocalMemory();        /* release local symbols */
	isPascal = FALSE;
	isOscall = FALSE;
	isInterrupt = FALSE;
	isNocall = FALSE;
}

void ParseFunctionBody(SYM *sp)
{    
	char lbl[200];

	lbl[0] = 0;
	needpunc(begin);
    ParseAutoDeclarations();
	cseg();
	if (sp->storage_class == sc_static)
	{
		//strcpy(lbl,GetNamespace());
		//strcat(lbl,"_");
		strcpy(lbl,sp->name);
		gen_strlab(lbl);
	}
	//	put_label((unsigned int) sp->value.i);
	else {
		if (sp->storage_class == sc_global)
			strcpy(lbl, "public code ");
		strcat(lbl,sp->name);
		gen_strlab(lbl);
	}
	currentFn = sp;
	currentFn->IsLeaf = TRUE;
	currentFn->DoesThrow = FALSE;
	currentFn->UsesPredicate = FALSE;
	regmask = 0;
	bregmask = 0;
    GenerateFunction(sp, ParseCompoundStatement());
//	if (optimize)
		flush_peep();
	if (sp->storage_class == sc_global) {
		fprintf(output,"endpublic\r\n\r\n");
	}

}
