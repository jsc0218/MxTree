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
	virtual GiSTobject *NCopy ();  // copy the node invalidating its entries (useful in Split)  

#ifdef PRINTING_OBJECTS
	void Print (ostream& os) const;
#endif

	// search methods
	GiSTpage SearchMinPenalty (const GiSTentry& newEntry) const;  // overload of virtual method
	GiSTlist<MTentry *> RangeSearch (const MTquery& query);  // range search

	// insertion
	void InsertBefore (const GiSTentry& newEntry, int index);  // overload of virtual method in order to insert the entry in the right place with the order ability

	// two of the basic GiST methods 
	GiSTnode *PickSplit ();  // first step of split, including promote, split, delete and sort
	GiSTentry* Union () const;  // second step of split

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

//private:
protected:
	Object *obj;  // object copied from the node's entry, useful for Union()
};

#endif
