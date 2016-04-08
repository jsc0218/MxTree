/*********************************************************************
*                                                                    *
* Copyright (c) 1997,1998, 1999                                      *
* Multimedia DB Group and DEIS - CSITE-CNR,                          *
* University of Bologna, Bologna, ITALY.                             *
*                                                                    *
* All Rights Reserved.                                               *
*                                                                    *
* Permission to use, copy, and distribute this software and its      *
* documentation for NON-COMMERCIAL purposes and without fee is       *
* hereby granted provided  that this copyright notice appears in     *
* all copies.                                                        *
*                                                                    *
* THE AUTHORS MAKE NO REPRESENTATIONS OR WARRANTIES ABOUT THE        *
* SUITABILITY OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING  *
* BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY,      *
* FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT. THE AUTHOR  *
* SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A      *
* RESULT OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS    *
* DERIVATIVES.                                                       *
*                                                                    *
*********************************************************************/

#include "MT.h"
#include "MTpredicate.h"

int 
SimpleQuery::Consistent (const GiSTentry& entry)
{
	assert (entry.IsA() == MTENTRY_CLASS);
	if (grade==0 || fabs(grade - ((MTentry &)entry).Key()->distance) <= radius + ((MTentry &)entry).MaxRadius()) {  // prune for reference point
		grade = pred->distance(((MTentry &)entry).object());
		return grade <= radius+((MTentry &)entry).MaxRadius();
	} else {
		return FALSE;
	}
}

int
SimpleQuery::NonConsistent (const GiSTentry& entry)
{
	return FALSE;
}
