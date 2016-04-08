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

#include "MTcursor.h"
#include "MT.h"
#include "MTpredicate.h"

inline int comparedst (MTrecord *a, MTrecord *b) { double d = a->Bound() - b->Bound(); return d==0 ? 0 : (d>0 ? 1 : -1); }
inline int compareentry (MTentry *a, MTentry *b) { double d = a->MaxRadius() - b->MaxRadius(); return d==0 ? 0 : (d>0 ? 1 : -1); }

MTcursor::MTcursor (const MT& tree, const MTpred& pred): tree (tree), queue (comparedst), results (compareentry)
{
	GiSTpath path;
	path.MakeRoot ();
	queue.Insert (new MTrecord (path, 0, MAXDOUBLE));

	this->pred = (MTpred *) pred.Copy ();
}

MTcursor::~MTcursor ()
{
	if (pred) {
		delete pred;
	}
	while (!queue.IsEmpty()) {
			delete queue.RemoveFirst ();
	}
	while (!results.IsEmpty()) {
		delete results.RemoveFirst ();
	}
}

BOOL
MTcursor::IsReady () const
{
	if (queue.IsEmpty()) {
		return (!results.IsEmpty());
	} else if (results.IsEmpty()) {
		return FALSE;
	} else {
		return (queue.First()->Bound() >= results.First()->MaxRadius());
	}
}

MTentry *
MTcursor::Next ()
{
	while (!IsReady()) {
		if (queue.IsEmpty() && results.IsEmpty()) {
			return NULL;
		}
		FetchNode ();
	}
	if (!results.IsEmpty()) {
		return results.RemoveFirst ();
	} else {
		return NULL;
	}
}

void
MTcursor::FetchNode ()
{
	if (queue.IsEmpty()) {
		return;
	}

	MTrecord *record = queue.RemoveFirst ();
	MTnode *node = (MTnode *) tree.ReadNode (record->Path());  // retrieve next node to be examined
	delete record;

	// search the first children to be examined
	for (int i=0; i<node->NumEntries(); i++) {  // for each entry in the current node
		MTentry *entry = (MTentry *) (*node)[i].Ptr();
		double dist = pred->distance(entry->object());

		if (!entry->IsLeaf()) {  // insert the child node in the priority queue
			GiSTpath path = node->Path();
			path.MakeChild (entry->Ptr());
			queue.Insert (new MTrecord (path, MAX(dist - entry->MaxRadius(), 0), dist));
		} else {  // insert the entry in the result list
			MTentry *newEntry = (MTentry *) entry->Copy();
			newEntry->SetMinRadius(0);
			newEntry->SetMaxRadius(dist);  // we insert the actual distance from the query object as the key radius
			results.Insert (newEntry);
		}
	}

	delete node;  // delete examined node
}




