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

#ifndef MTNODE_H
#define MTNODE_H

#include "MTentry.h"

class MTquery;

class MTnode : public GiSTnode {
public:
	// constructors, destructors, etc.
	MTnode (): GiSTnode (), obj (NULL) { }
	MTnode (const MTnode& node): GiSTnode (node), obj (node.obj) { }
	GiSTobjid IsA () const { return MTNODE_CLASS; }
	GiSTobject *Copy () const { return new MTnode (*this); }
	GiSTobject *NCopy ();  // copy the node invalidating its entries (useful in Split)  

#ifdef PRINTING_OBJECTS
	void Print (ostream& os) const;
#endif

	// search methods
	GiSTpage SearchMinPenalty (const GiSTentry& newEntry) const;  // overload of virtual method
	GiSTlist<MTentry *> RangeSearch (const MTquery& query);  // range search

	// insertion
	void InsertBefore (const GiSTentry& newEntry, int index);  // overload of virtual method in order to insert the entry in the right place with the order ability

	// two of the basic GiST methods 
	GiSTnode *PickSplit ();  // whole process of splitting, including promote, split, delete and sort
	GiSTentry* Union () const;  // 

	// support functions for methods above
	MTnode *PromotePart ();  // object promotion  
	MTnode *PromoteVote ();  // object confirmed promotion
	int *PickCandidates ();  // pick several entries in this node
	void Split (MTnode *node, int *lArray, int *rArray, int *lDel, int *rDel);  // perform the split between the parents

	int IsUnderFull (const GiSTstore &store) const;  // used (in split and) in deletion

	// required support methods
	GiSTentry *CreateEntry () const { return new MTentry; }
	int FixedLength () const { return EntrySize (); }

	void InvalidateEntry (BOOL bNew);  // invalidate the entry in parent and grandparent   
	void InvalidateEntries ();  // invalidate the distance from its parent
	void SortEntries ();  // order the entries with respect to the distance from their parent
	void mMRadius (MTentry *unionEntry) const;  // compute the min & MAX radii of this node
	MTentry *ParentEntry () const;  // retrieve the entry representing this node
	double Distance (int i) const;  // distance between i th entry in this node and its parent entry

	double Overlap() const;  // calculate the overlap in this node

private:
	Object *obj;  // object copied from the node's entry, useful for Union()
};

#endif
