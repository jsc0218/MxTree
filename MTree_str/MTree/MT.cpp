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

#ifdef _WIN32	// these functions are defined under UNIX
void srandom (int seed) { srand(seed); }
int random() { return rand(); }
#endif

TruePredicate truePredicate;

int PickRandom (int from, int to)
{
	return (random() % (to - from) + from);
}

MTnode *
MT::ParentNode (MTnode *node)
{
	GiSTpath path = node->Path();
	path.MakeParent ();
	return (MTnode *) ReadNode (path);  // parentNode should be destroyed by the caller
}

void
MT::CollectStats ()
{
	GiSTpath path;
	path.MakeRoot ();
	GiSTnode *node = ReadNode (path);
	if (!node->IsLeaf()) {
		int maxLevel = node->Level();
		double *radii = new double[maxLevel];
		int *pages = new int[maxLevel];
		for (int i=0; i<maxLevel; i++) {
			pages[i] = 0;
			radii[i] = 0;
		}
		TruePredicate truePredicate;
		GiSTlist<GiSTentry*> list = node->Search(truePredicate);  // retrieve all the entries in this node
		
		double overlap = ((MTnode *)node)->Overlap();
		double totalOverlap = overlap;
		
		delete node;
		while (!list.IsEmpty()) {
			GiSTentry *entry = list.RemoveFront ();
			path.MakeChild (entry->Ptr());
			node = ReadNode (path);

			overlap = ((MTnode *)node)->Overlap();
			totalOverlap += overlap;

			pages[node->Level()]++;
			radii[node->Level()] += ((MTkey *) entry->Key())->MaxRadius();
			GiSTlist<GiSTentry*> newlist;
			if (!node->IsLeaf()) {
				newlist = node->Search(truePredicate);  // recurse to next level
			}
			while (!newlist.IsEmpty()) {
				list.Append (newlist.RemoveFront ());
			}
			path.MakeParent ();
			delete entry;
			delete node;
		}
		// output the results
		cout << "Level:\tPages:\tAverage_Radius:"<<endl;
		int totalPages = 1;  // for the root
		for (int i=maxLevel-1; i>=0; i--) {
			totalPages += pages[i];
			cout << i << ":\t" << pages[i] << "\t" << radii[i]/pages[i] << endl;
		}
		cout << "TotalPages:\t" << totalPages << endl;
		cout << "LeafPages:\t" << pages[0] << endl;
		cout << "TotalOverlap:\t" << (float)totalOverlap << endl;
		delete []radii;
		delete []pages;
	} else {
		delete node;
	}
}

BOOL
MT::CheckNode (GiSTpath path, MTentry *parentEntry)
{
	MTnode *node = (MTnode *) ReadNode (path);
	BOOL ret = TRUE;

	for (int i=0; i<node->NumEntries() && ret; i++) {
	    MTentry *nextEntry = (MTentry *) (*node)[i].Ptr();
	    if (parentEntry!=NULL && (nextEntry->Key()->distance+nextEntry->MaxRadius() > parentEntry->MaxRadius() || nextEntry->Key()->distance != nextEntry->object().distance(parentEntry->object()))) {
			cout << "Error with entry " << nextEntry << "in " << node;
			ret = FALSE;
		}
		if (!node->IsLeaf()) {
			path.MakeChild (nextEntry->Ptr());
			ret &= CheckNode (path, nextEntry);
			path.MakeParent ();
		}
	}

	delete node;
	return ret;
}

GiSTlist<MTentry *>
MT::RangeSearch (const MTquery& query)
{
	GiSTpath path;
	path.MakeRoot ();
	MTnode *root = (MTnode *) ReadNode (path);
	GiSTlist<MTentry *> list = root->RangeSearch(query);
	delete root;
	return list;
}

MTentry **
MT::TopSearch (const TopQuery& query)
{
	MTentry **results = new MTentry*[query.k];  // the results list (ordered for increasing distances)
	double *dists = new double[query.k];  // array containing the KNN distances
	for (int i=0; i<query.k; i++) {
		dists[i] = MaxDist ();  // initialization of the KNN-distances array
	}

	GiSTpath path;  // path for retrieving root node
	path.MakeRoot ();
	MTnode *node = (MTnode *) ReadNode (path);  // retrieve the root node
	MTorderedlist<MTrecord *> priorQue (comparedst);  // priority queue for active nodes
	SimpleQuery simQuery (query.Pred(), MaxDist(), TRUE);
	while (node != NULL) {  // examine current node
		double oldDist = simQuery.Grade ();
		for (int i=0; i<node->NumEntries(); i++) {  // for each entry in the current node
			simQuery.SetGrade (oldDist);
			MTentry *entry = (MTentry *) ((*node)[i].Ptr());
			if (simQuery.Consistent (*entry)) {
				if (entry->IsLeaf()) {
					if (dists[query.k-1] < MaxDist()) {
						delete results[query.k-1];  // delete last element of the results list
					}
					MTentry *newEntry = (MTentry *) entry->Copy ();
					newEntry->SetMinRadius(0);
					newEntry->SetMaxRadius(simQuery.Grade());  // insert the actual distance from the query object as the key radius
					// insert dist in the results list (sorting for incr. distance), which could be improved using binary search
					int j = 0;
					while (dists[j] <= simQuery.Grade()) {
						j++;
					}
					for (int i=query.k-1; i>j; i--) {  // shift up results array
						results[i] = results[i-1];
						dists[i] = dists[i-1];
					}
					results[j] = newEntry;
					dists[j] = simQuery.Grade ();
					simQuery.SetRadius (dists[query.k-1]);
				} else {  // insert the child node in the priority queue
					double bound = simQuery.Grade () - entry->MaxRadius();  // these are lower-bounds on the distances of the descendants of the entry from the query object
					bound = bound<0 ? 0 : bound;
					// insert the node in the stack (sorting for decr. level and incr. LB on distance), which could be improved using binary search
					GiSTpath path = node->Path();
					path.MakeChild (entry->Ptr());
					priorQue.Insert (new MTrecord (path, bound, simQuery.Grade ()));
				}
			}
		}
		delete node;  // delete examined node

		// the next entry to be chosen is that whose distance from the parent is most similar to the distance between the parent and the query object
		if (!priorQue.IsEmpty()) {  // retrieve next node to be examined
			MTrecord *record = priorQue.RemoveFirst ();
			if (record->Bound() >= dists[query.k-1]) {  // terminate the search
				while (!priorQue.IsEmpty()) {
					delete priorQue.RemoveFirst ();
				}
				node = NULL;
			} else {
				node = (MTnode *) ReadNode (record->Path());
				simQuery.SetGrade (record->Distance());  // distance between parent entry and pred
			}
			delete record;
		} else {
			node = NULL;  // terminate the search
		}
	}

	delete []dists;
	return results;
}
