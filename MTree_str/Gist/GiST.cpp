// -*- Mode: C++ -*-

//         GiST.cpp
//
// Copyright (c) 1996, Regents of the University of California
// $Header: /usr/local/devel/GiST/libGiST/libGiST/GiST.cpp,v 1.1.1.1 1996/08/06 23:47:20 jmh Exp $

#include <stdio.h>
#include <string.h>

#include "GiST.h"
#include "GiSTpath.h" // for GiSTRootPage

GiST::GiST ()
{
	isOpen = 0;
	debug = 0;
	store = NULL;  // added by myself
}

void 
GiST::Create (const char *filename)
{
	if (IsOpen()) {
		return;
	}
	store = CreateStore();
	store->Create(filename);
	if (!store->IsOpen()) {  // create failed!
		return;
	}
	store->Allocate();

	GiSTnode *node = NewNode(this);
	node->Path().MakeRoot();
	WriteNode (node);
	delete node;
	isOpen = 1;
}

void 
GiST::Open (const char *filename)
{
	if (IsOpen()) {
		return;
	}
	store = CreateStore();
	store->Open(filename);
	if (!store->IsOpen()) {
		return;
	}
	isOpen = 1;
}

void 
GiST::Close ()
{
	if (IsOpen()) {
		store->Close();
		isOpen = 0;
    }
}

void 
GiST::Insert (const GiSTentry &entry)
{
	InsertHelper (entry, 0);
}

void 
GiST::InsertHelper (const GiSTentry &entry,
		   int level, // level of tree at which to insert
		   int *splitvec) // a vector to trigger Split instead of forced reinsert
{
	int overflow = 0;

	GiSTnode *leaf = ChooseSubtree (GiSTRootPage, entry, level);
	leaf->Insert(entry);
	if (leaf->IsOverFull(*store)) {
		if (ForcedReinsert() && !leaf->Path().IsRoot() && (!splitvec||!splitvec[level])) {
			int split[GIST_MAX_LEVELS];
			// R*-tree-style forced reinsert
			for (int i=0; i<GIST_MAX_LEVELS; i++) {
				split[i] = 0;
			}
			OverflowTreatment (leaf, entry, split);
			overflow = 1;
		} else {
			Split (&leaf, entry);
		}
		if (leaf->IsOverFull(*store)) {  // ??????????????????
			// we only should get here if we reinserted, and the node re-filled
			assert (overflow);
			leaf->DeleteEntry(entry.Position());
			Split (&leaf, entry);
		}
	} else {
		WriteNode (leaf);
	}
	if (!overflow) {
		AdjustKeys (leaf, NULL);
	}
	delete leaf;
}

void 
GiST::OverflowTreatment (GiSTnode *node, const GiSTentry& entry, int *splitvec)
{
	// remove the "top" p entries from the node
	GiSTlist<GiSTentry*> deleted = RemoveTop (node);
	WriteNode (node);
	AdjustKeys (node, NULL);
	// note that we've seen this level already
	splitvec[node->Level()] = 1;
	// for each of the deleted entries, call InsertHelper at this level
	while (!deleted.IsEmpty()) {
		GiSTentry *tmpentry = deleted.RemoveFront ();
		InsertHelper (*tmpentry, node->Level(), splitvec);
		delete tmpentry;
	}
}

GiSTlist<GiSTentry*>
GiST::RemoveTop (GiSTnode *node)
{
	GiSTlist<GiSTentry*> deleted;
	int count = node->NumEntries();
	// default: remove the first ones on the page
	int num_rem = (int)((count+1) * RemoveRatio() + 0.5);

	for (int i=num_rem-1; i>=0; i--) {
		deleted.Append ((GiSTentry *)(*node)[i].Ptr()->Copy());
		node->DeleteEntry(i);
	}
	return (deleted);
}

void 
GiST::AdjustKeys (GiSTnode *node, GiSTnode **parent)
{
	if (node->Path().IsRoot()) {
		return;
	}

	GiSTnode *P;
	// Read in node's parent
	if (parent == NULL) {
		GiSTpath parent_path = node->Path();
		parent_path.MakeParent ();
		P = ReadNode (parent_path);
		parent = &P;
	} else {
		P = *parent;
	}

	// Get the old entry pointing to node
	GiSTentry *entry = P->SearchPtr(node->Path().Page());

	assert (entry != NULL);

	// Get union of node
	GiSTentry *actual = node->Union();

	actual->SetPtr(node->Path().Page());
	if (!entry->IsEqual(*actual)) {
		int pos = entry->Position();
		P->DeleteEntry(pos);
		P->InsertBefore(*actual, pos);
		// A split may be necessary.
		// XXX: should we do Forced Reinsert here too?
		if (P->IsOverFull(*store)) {
			Split (parent, *actual);

			GiSTpage page = node->Path().Page();
			node->Path() = P->Path();
			node->Path().MakeChild(page);
		} else {
		    WriteNode (P);
			AdjustKeys (P, NULL);
		}
	}
	if (parent == &P) {
		delete P;
	}
	delete actual;
	delete entry;
}

void 
GiST::Sync ()
{
	store->Sync();
}

void 
GiST::Delete (const GiSTpredicate& pred)
{
	GiSTcursor *c = Search (pred);
	GiSTentry *e;

	do {
		if (c == NULL) {
			return;
		}
		e = c->Next();
		GiSTpath path = c->Path();
		delete c;
		if (e == NULL) {
			return;
		}
		// Read in the node that this belongs to
		GiSTnode *node = ReadNode (path);
		node->DeleteEntry(e->Position());
		WriteNode (node);

		int condensed = CondenseTree (node);
		delete node;
		if (condensed) {
			ShortenTree ();
			// because the tree changed, we need to search all over again!
			// XXX - this is inefficient!  users may want to avoid condensing.
			c = Search (pred);
		}
	} while (e != NULL);
}

void
GiST::ShortenTree ()
{
	GiSTpath path;
	// Shorten the tree if necessary (This should only be done if root actually changed!)
	path.MakeRoot ();
	GiSTnode *root = ReadNode(path);

	if (!root->IsLeaf() && root->NumEntries()==1) {
		path.MakeChild ((*root)[0]->Ptr());
		GiSTnode *child = ReadNode (path);

		store->Deallocate(path.Page());
		child->SetSibling(0);
		child->Path().MakeRoot();
		WriteNode (child);
		delete child;
	}
	delete root;
}

// handle underfull leaf nodes
int
GiST::CondenseTree (GiSTnode *leaf)
{
	GiSTlist<GiSTentry*> Q;
	int deleted = 0;

	// Must be condensing a leaf
	assert (leaf->IsLeaf());
	while (!leaf->Path().IsRoot()) {
		GiSTpath parent_path = leaf->Path();
		parent_path.MakeParent ();
		GiSTnode *P = ReadNode (parent_path);
		GiSTentry *En = P->SearchPtr(leaf->Path().Page());

		assert (En != NULL);
		// Handle under-full node
		if (leaf->IsUnderFull(*store)) {
		    if (!IsOrdered()) {
				TruePredicate truePredicate;
				GiSTlist<GiSTentry*> list = leaf->Search(truePredicate);

				while (!list.IsEmpty()) {
					GiSTentry *e = list.RemoveFront();
					Q.Append (e);
				}
				P->DeleteEntry(En->Position());
				WriteNode (P);
				deleted = 1;
				AdjustKeys (P, NULL);
			} else {
				// Try to borrow entries, else coalesce with a neighbor
				GiSTpage neighbor_page = P->SearchNeighbors(leaf->Path().Page());  // right and then left
				if (neighbor_page != 0) {
					GiSTpath neighbor_path = leaf->Path();
					neighbor_path.MakeSibling (neighbor_page);

					GiSTnode *neighbor;
					if (leaf->Sibling() == neighbor_page) {  // RIGHT sibling...
						neighbor = ReadNode (neighbor_path);
					} else {  // LEFT sibling
						neighbor = leaf;
						leaf = ReadNode (neighbor_path);
					}

					leaf->Coalesce(*neighbor);
					// If not overfull, coalesce, kill right node
					if (!leaf->IsOverFull(*store)) {
						leaf->SetSibling(neighbor->Sibling());
						WriteNode (leaf);

						// Delete the neighbor from parent
						GiSTentry *e = P->SearchPtr(neighbor->Path().Page());
						P->DeleteEntry(e->Position());
						WriteNode (P);
						delete e;
						store->Deallocate(neighbor->Path().Page());
						deleted = 1;
					} else {  // If overfull, split (same as borrowing)
						GiSTnode *node2 = leaf->PickSplit();
						node2->Path() = neighbor->Path();
						node2->SetSibling(neighbor->Sibling());
						WriteNode (leaf);
						WriteNode (node2);
						AdjustKeys (node2, &P);
						delete node2;
						deleted = 1;
					}
					delete neighbor;
				}
			}
		}
		// Adjust covering predicate
		if (!deleted) {
			AdjustKeys (leaf, &P);
		}
		parent_path = leaf->Path();
		parent_path.MakeParent ();
		delete leaf;
		// Propagate deletes
		if (!deleted) {
			break;
		}
		leaf = P;
	}
	// Re-insert orphaned entries
	while (!Q.IsEmpty()) {
		GiSTentry *e = Q.RemoveFront ();
		InsertHelper (*e, e->Level());
		delete e;
	}
	return (deleted);
}

GiSTnode* 
GiST::ChooseSubtree (GiSTpage page, const GiSTentry &entry, int level)
{
	GiSTnode *node;
	GiSTpath path;

	for (;;) {
		path.MakeChild (page);
		node = ReadNode (path);
		if (level==node->Level() || node->IsLeaf()) {
			break;
		}
		page = node->SearchMinPenalty(entry);
		delete node;
	}
	return node;
}

void 
GiST::Split (GiSTnode **node, const GiSTentry& entry)
{
	int went_left = 0, new_root = 0;

	if ((*node)->Path().IsRoot()) {
		new_root = 1;
		(*node)->Path().MakeChild(store->Allocate());
	}

	GiSTnode *node2 = (*node)->PickSplit();
	node2->Path().MakeSibling(store->Allocate());

	GiSTentry *e = (*node)->SearchPtr(entry.Ptr());
	if (e != NULL) {
		went_left = 1;
		delete e;
	}
	node2->SetSibling((*node)->Sibling());
	(*node)->SetSibling(node2->Path().Page());
	WriteNode (*node);
	WriteNode (node2);

	GiSTentry *e1 = (*node)->Union();
	GiSTentry *e2 = node2->Union();

	e1->SetPtr((*node)->Path().Page());
	e2->SetPtr(node2->Path().Page());
	// Create new root if root is being split
	if (new_root) {
		GiSTnode *root = NewNode (this);
		root->SetLevel((*node)->Level() + 1);
		root->InsertBefore(*e1, 0);
		root->InsertBefore(*e2, 1);
		root->Path().MakeRoot();
		WriteNode (root);
		delete root;
	} else {
		// Insert entry for N' in parent
		GiSTpath parent_path = (*node)->Path();
		parent_path.MakeParent ();
		GiSTnode *parent = ReadNode (parent_path);
		// Find the entry for N in parent
		GiSTentry *e = parent->SearchPtr((*node)->Path().Page());
		assert (e != NULL);
		// Insert the new entry right after it
		int pos = e->Position();
		parent->DeleteEntry(pos);
		parent->InsertBefore(*e1, pos);
		parent->InsertBefore(*e2, pos+1);
		delete e;
		if (!parent->IsOverFull(*store)) {
			WriteNode (parent);
		} else {
			Split (&parent, went_left? *e1: *e2);
			GiSTpage page = (*node)->Path().Page();
			(*node)->Path() = parent->Path();  // parent's path may changed
			(*node)->Path().MakeChild (page);
			page = node2->Path().Page();
			node2->Path() = (*node)->Path();
			node2->Path().MakeSibling (page);
		}
		delete parent;
	}
	if (!went_left) {
		delete *node;
		*node = node2;  // return it
	} else {
		delete node2;
	}
	delete e1;
	delete e2;
}

GiSTnode* 
GiST::ReadNode (const GiSTpath& path) const
{
	char *buf = new char[store->PageSize()];
	GiSTnode *node = NewNode((GiST *)this);

	store->Read(path.Page(), buf);  // do the deed
	node->Unpack(buf);

#ifdef PRINTING_OBJECTS
	if (debug) {
		cout << "READ PAGE " << path.Page() << ":\n";
		node->Print(cout);
	}
#endif
	node->Path() = path;
	delete []buf;
	return node;
}

void 
GiST::WriteNode (GiSTnode *node)
{
	char *buf = new char[store->PageSize()];
    
	// make Purify happy
	memset (buf, 0, store->PageSize());
#ifdef PRINTING_OBJECTS
	if (debug) {
		cout << "WRITE PAGE " << node->Path().Page() << ":\n";
		node->Print(cout);
	}
#endif
	node->Pack(buf);
	store->Write(node->Path().Page(), buf);  // do the deed
	delete []buf;
}

#ifdef PRINTING_OBJECTS

void 
GiST::DumpNode (ostream& os, GiSTpath path) const
{
	GiSTnode *node = ReadNode(path);

	node->Print(os);
	if (!node->IsLeaf()) {
		TruePredicate truePredicate;
		GiSTlist<GiSTentry*> list = node->Search(truePredicate);

		while (!list.IsEmpty()) {
			GiSTentry *e = list.RemoveFront();
			path.MakeChild(e->Ptr());
			DumpNode (os, path);
			path.MakeParent();
			delete e;
		}
	}
	delete node;
}

void 
GiST::Print (ostream& os) const
{
	GiSTpath path;

	path.MakeRoot();
	DumpNode (os, path);
}

#endif

GiSTcursor* 
GiST::Search (const GiSTpredicate &query) const
{
	return new GiSTcursor(*this, query);
}

GiST::~GiST ()
{
	Close ();
	if (store) {
		delete store;
	}
}
