// ============================================================================
//        __
//   \\__/ o\    (C) 2012-2014  Robert Finch, Stratford
//    \  __ /    All rights reserved.
//     \/_//     robfinch<remove>@finitron.ca
//       ||
//
// C64 - Raptor64 'C' derived language compiler
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

static uint8_t hashadd(char *nm)
{
	uint8_t hsh;

	for(hsh=0;*nm;nm++)
		hsh += *nm;
	return hsh;
}

SYM *search(char *na,TABLE *tbl)
{
	SYM *thead;
	if (tbl==&gsyms[0])
		thead = gsyms[hashadd(na)].head;
	else
		thead = tbl->head;
	while( thead != NULL) {
		if (thead->name != NULL)
			if(strcmp(thead->name,na) == 0)
				return thead;
        thead = thead->next;
    }
    return NULL;
}

// first look in the current compound statement for the symbol,
// Next look in progressively more outer compound statements
// Next look in the local symbol table for the function
// Finally look in the global symbol table.
//
SYM *gsearch(char *na)
{
	SYM *sp;
	Statement *st;

	// There might not be a current statement if global declarations are
	// being processed.
	if (currentStmt==NULL) {
		if ((sp = search(na,&lsyms))==NULL)
			sp = search(na,&gsyms[0]);
	}
	else {
		if((sp = search(na,&(currentStmt->ssyms))) == NULL) {
			st = currentStmt->outer;
			while (st) {
				sp = search(na,&st->ssyms);
				if (sp)
					return sp;
				st = st->outer;
			}
			if ((sp = search(na,&lsyms))==NULL)
				sp = search(na,&gsyms[0]);
		}
	}
    return sp;
}

void insert(SYM* sp, TABLE *table)
{
	if (table==&gsyms[0])
		table = &gsyms[hashadd(sp->name)];
	if( search(sp->name,table) == NULL) {
        if( table->head == NULL)
            table->head = table->tail = sp;
        else {
            table->tail->next = sp;
            table->tail = sp;
        }
        sp->next = NULL;
    }
    else
        error(ERR_DUPSYM);
}
