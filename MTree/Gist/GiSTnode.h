// -*- Mode: C++ -*-

//         GiSTnode.h
//
// Copyright (c) 1996, Regents of the University of California
// $Header: /usr/local/devel/GiST/libGiST/libGiST/GiSTnode.h,v 1.1.1.1 1996/08/06 23:47:21 jmh Exp $
#ifndef GISTNODE_H
#define GISTNODE_H

#include "GiSTentry.h"
#include "GiSTpredicate.h"
#include "GiSTpath.h"
#include "GiSTstore.h"

class GiST;

// Type of line table element
typedef unsigned short GiSTlte;

// Page header
struct GiSTheader {
	int level;
	int numEntries;
	GiSTpage parent;
	GiSTpage sibling;
};

// Entry pointer, with some handy operators 
class GiSTentryptr {
public:
	GiSTentryptr () { ptr = NULL; }
	operator GiSTentry* () const { return ptr; }  // for type conversion
	operator GiSTentry& () const { return *ptr; }  // for type conversion
	GiSTentry* operator-> () const { return ptr; }
	GiSTentryptr& operator= (GiSTentry* ptr) { this->ptr = ptr; return *this; }
	GiSTentryptr& operator= (GiSTentryptr& ptr) { this->ptr = ptr.ptr; return *this; }
	GiSTentry* Ptr () const { return ptr; }

private:
	GiSTentry *ptr;
};

#define GIST_PAGE_HEADER_SIZE sizeof(GiSTheader)

// The GiST node class.
class GiSTnode: public GiSTobject {
public:
	GiSTnode ();
	GiSTnode (const GiSTnode& node);

	GiSTobjid IsA () const { return GISTNODE_CLASS; }
#ifdef PRINTING_OBJECTS
	virtual void Print (ostream& os) const;
#endif
	// Operations on the entry array
	GiSTentryptr& operator[] (int index);
	const GiSTentryptr& operator[] (int index) const;

	void Expand (int index);
	int NumEntries () const { return numEntries; }
	void SetNumEntries (int num) { numEntries = num; }

	// Search methods
	virtual GiSTentry* SearchPtr (GiSTpage ptr) const;  // search the entry containing the 'ptr'
	virtual GiSTpage SearchMinPenalty (const GiSTentry& entry) const;  // returns where a new entry should be inserted
	virtual GiSTpage SearchNeighbors (GiSTpage ptr) const;  // through ptr identifying the entry
	virtual GiSTlist<GiSTentry*> Search (const GiSTpredicate &query) const;  // now it simply returns the entries in this node

	// Insertion and deletion
	virtual void Insert (const GiSTentry &entry);
	virtual void InsertBefore (const GiSTentry &entry, int index);
	virtual void DeleteEntry (int index);
	virtual void DeleteBulk (int vec[], int veclen);

	// Pack/unpack disk representation
	virtual void Pack (char *page) const;
	virtual void Unpack (const char *page);

	// two of the basic GiST methods
	// Returns the Union of all entries
	virtual GiSTentry* Union () const=0;
	// Splitting
	virtual GiSTnode *PickSplit ();

	// required support methods
	virtual GiSTentry* CreateEntry () const=0;
	virtual int FixedLength () const { return 0; }

	virtual GiSTcompressedEntry Entry (int entryPos) const;  // assembled from packedNode

	// coalescing
	virtual void Coalesce (const GiSTnode &source);

	// access to the private members
	GiSTpage Sibling () const { return sibling; }
	void SetSibling (GiSTpage p) { sibling = p; }
	int IsLeaf () const { return (level == 0); }
	int Level () const { return level; }
	void SetLevel (int l) { level = l; }

	virtual int IsOverFull (const GiSTstore &store) const;
	virtual int IsUnderFull (const GiSTstore &store) const;

	~GiSTnode ();

	virtual int Size () const;  // return value is different from pageSize

	GiST *Tree () const { return tree; }
	void SetTree (GiST *t) { tree = t; }

	GiSTpath& Path () { return path; }

private:
	void Reset ();
	GiSTpath path;
	GiSTentryptr *entries;
	char *packedNode;
	GiSTpage sibling;
	int level;
	int numEntries, maxEntries;
	GiST *tree;
};

#endif
