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

#ifdef UNIX
#include <unistd.h>
#endif
#include "MT.h"

extern double MIN_UTIL;

// find the node having the minimum number of children
// this is used in the redistributing phase of the BulkLoad algorithm
int FindMin (int *array, int size)
{
	int min = 0;
	for (int i=1; i<size; i++) {
		if (array[i] < array[min]) {
			min = i;
		}
	}
	return min;
}

// return root level+1 (the height of the tree)
// this is used in the "splitting" phase of the BulkLoad algorithm
int
MT::TreeHeight () const
{
	GiSTpath path;
	path.MakeRoot ();
	GiSTnode *root = ReadNode (path);
	int i = root->Level();
	delete root;
	return (i+1);
}

// load this M-tree with n data using the BulkLoad algorithm [CP98]
// data is an array of n entries
// padFactor is the maximum node utilization (use 1)
// name is the name of the tree
void
MT::BulkLoad (MTentry **data, int n, double padFactor, const char *name)
{
	int size = 0;
	if (EntrySize()) {
		size = n * (sizeof(GiSTpage) + EntrySize());  // (only valid if we've fixed size entries)
	} else {
		for (int i=0; i<n; i++) {
			size += sizeof(GiSTlte) + sizeof(GiSTpage) + data[i]->CompressedLength();
		}
	}
	int totSize = size + GIST_PAGE_HEADER_SIZE + sizeof(GiSTlte);

	if (totSize > Store()->PageSize()) {  // we need to split the entries into several sub-trees
		int numEntries = (int)(Store()->PageSize()*padFactor*n) / totSize;
		int s = (int) MAX (MIN (numEntries, ceil(((float)n)/numEntries)), numEntries*MIN_UTIL);  // initial number of samples
		int nSamples, *samples = new int[s], *sizes = NULL, *ns = NULL, iter = 0, MAXITER = s * s;
		GiSTlist<double *> *distm = (GiSTlist<double *> *) calloc (s, sizeof(GiSTlist<double *>));  // relative distances between samples
		int MINSIZE = (int) (Store()->PageSize()*MIN_UTIL), addEntrySize = EntrySize() ? sizeof(GiSTpage) : sizeof(GiSTlte)+sizeof(GiSTpage);
		GiSTlist<int> *lists = NULL;  // set for each sample set
		GiSTlist<double> *dists = NULL;  // set for distance between each sample and its members
		BOOL *bSampled = new BOOL[n];  // is this entry in the samples set?

		// sampling phase
		do {
			iter++;
			if (iter > 1) {  // this is a new sampling phase
				while (!lists[0].IsEmpty()) {
					lists[0].RemoveFront ();
					dists[0].RemoveFront ();
				}
				delete []lists;
				delete []dists;
				delete []sizes;
				delete []ns;
				while (!distm[0].IsEmpty()) {
					delete []distm[0].RemoveFront();  // empty the distance list
				}
				for (int i=1; i<s; i++) {
					distm[i].front = distm[i].rear = NULL;
				}
			}
			if (iter >= MAXITER) {
				cout << "Too many loops in BulkLoad!"<<endl<<"Please select a lower minimum node utilization or a bigger node size."<<endl;
				exit(1);
			}

			for (int i=0; i<n; i++) {
				bSampled[i] = FALSE;
			}
			nSamples = 0;
			// pick s samples to create parents
			while (nSamples < s) {
				int i;
				do {
					i = PickRandom (0, n);
				} while (bSampled[i]);
				bSampled[i] = TRUE;
				samples[nSamples++] = i;
			}

			lists = new GiSTlist<int>[s];
			dists = new GiSTlist<double>[s];
			sizes = new int[s];
			ns = new int[s];
			for (int i=0; i<s; i++) {
				sizes[i] = GIST_PAGE_HEADER_SIZE + sizeof(GiSTlte);
				ns[i] = 1;
				distm[i].Prepend (new double[s]);
			}

			// compute the relative distances between samples
			for (int i=0; i<s; i++) {
				for (int j=0; j<i; j++) {
					distm[j].front->entry[i] = distm[i].front->entry[j] = data[samples[j]]->object().distance(data[samples[i]]->object());
				}
				distm[i].front->entry[i] = 0;
			}

			// assign each entry to its nearest parent
			for (int i=0; i<n; i++) {
				if (bSampled[i]) {
					int j = 0;
					for (; samples[j]!=i; j++);  // find this entry in the samples set and return position in it
					lists[j].Prepend (i);  // insert the entry in the right sample
					dists[j].Prepend (0);  // distance between sample and data[i]
					sizes[j] += addEntrySize + data[i]->CompressedLength();
				} else {  // here we optimize the distance computations (like we do in the insert algorithm)
					double *dist = new double[s];  // distance between this non-sample and samples
					dist[0] = data[samples[0]]->object().distance(data[i]->object());
					int minIndex = 0;
					for (int j=1; j<s; j++) {  // seek the nearest sample
						dist[j] = -MaxDist();
						if (fabs (data[samples[j]]->Key()->distance - data[i]->Key()->distance) >= dist[minIndex]) {  // pruning
							continue;
						}
						BOOL flag = TRUE;
						for (int k=0; k<j && flag; k++) {  // pruning (other samples)
							if (dist[k] < 0) {
								continue;
							} else {
								flag = fabs (dist[k] - distm[j].front->entry[k]) < dist[minIndex];
							}
						}
						if (!flag) {
							continue;
						}
						dist[j] = data[samples[j]]->object().distance(data[i]->object());  // have to compute this distance
						if (dist[j] < dist[minIndex]) {
							minIndex = j;
						}
					}
					lists[minIndex].Append (i);  // insert the entry in the right sample
					dists[minIndex].Append (dist[minIndex]);  // distance between sample and data[i]
					sizes[minIndex] += addEntrySize + data[i]->CompressedLength();
					ns[minIndex]++;
					sizes[minIndex] >= MINSIZE ? delete []dist : distm[minIndex].Append (dist);  // correspond with lists
				}
			}

			// redistribute underfilled parents
			int i;
			while (sizes[i = FindMin (sizes, nSamples)] < MINSIZE) {
				GiSTlist<int> list = lists[i];  // each sample set
				while (!dists[i].IsEmpty()) {  // clear distance between each sample and its members
					dists[i].RemoveFront ();
				}

				// substitute this set with last set
				for (int j=0; j<nSamples; j++) {
					for (GiSTlistnode<double *> *node=distm[j].front; node; node=node->next) {
						node->entry[i] = node->entry[nSamples-1];
					}
				}
				GiSTlist<double *> dlist = distm[i];  // relative distances between sample[i] and other samples, reposition by myself

				distm[i] = distm[nSamples-1];
				lists[i] = lists[nSamples-1];
				dists[i] = dists[nSamples-1];
				samples[i] = samples[nSamples-1];
				sizes[i] = sizes[nSamples-1];
				ns[i] = ns[nSamples-1];
				nSamples--;
				while (!list.IsEmpty()) {  // assign each entry to its nearest parent
					double *dist = dlist.RemoveFront ();  // relative distances between sample[i] (old) and other samples (old)
					int minIndex = -1;
					for (int j=0; j<nSamples && minIndex<0; j++) {  // search for a computed distance
						if (dist[j] > 0) {
							minIndex = j;
						}
					}
					int k = list.RemoveFront ();
					if (minIndex < 0) {  // no distance was computed (i.e. all distances were pruned)
						dist[0] = data[samples[0]]->object().distance(data[k]->object());
						minIndex = 0;
					}
					for (int j=0; j<nSamples; j++) {
						if (j == minIndex) {
							continue;
						}
						if (dist[j] < 0) {  // distance wasn't computed
							if (fabs (data[samples[j]]->Key()->distance - data[k]->Key()->distance) >= dist[minIndex]) {
								continue;  // pruning
							}
							BOOL flag = TRUE;
							for (int i=0; i<j && flag; i++) { // pruning (other samples)
								if (dist[i] < 0) {
									continue;
								} else {
									flag = fabs (dist[i] - distm[j].front->entry[i]) < dist[minIndex];
								}
							}
							if (!flag) {
								continue;
							}
							dist[j] = data[samples[j]]->object().distance(data[k]->object());  // have to compute this distance
						}
						if (dist[j] < dist[minIndex]) {
							minIndex = j;
						}
					}
					lists[minIndex].Append (k);
					dists[minIndex].Append (dist[minIndex]);
					sizes[minIndex] += addEntrySize + data[k]->CompressedLength();
					ns[minIndex]++;
					sizes[minIndex] >= MINSIZE ? delete []dist : distm[minIndex].Append (dist);  // correspond with lists
				}
				assert (dlist.IsEmpty());  // so is the list
			}
		} while (nSamples == 1);  // if there's only one child, repeat the sampling phase
		MTentry ***array = new MTentry **[nSamples];  // array of the entries for each sub-tree
		for (int i=0; i<nSamples; i++) {  // convert the lists into arrays
			array[i] = new MTentry *[ns[i]];
			for (int j=0; j<ns[i]; j++) {
				array[i][j] = (MTentry *) data[lists[i].RemoveFront ()]->Copy();
				array[i][j]->Key()->distance = dists[i].RemoveFront ();
			}
			assert (lists[i].IsEmpty());
			assert (dists[i].IsEmpty());
		}
		delete []lists;
		delete []dists;
		delete []sizes;
		delete []bSampled;
		for (int i=0; i<nSamples; i++) {
			while (!distm[i].IsEmpty()) {
				delete [](distm[i].RemoveFront());
			}
		}
		free (distm);

		// build an M-tree under each parent
		int nInit = nSamples;
		MT *subtree = new MT;
		GiSTlist<char *> subtreeNames;  // list of the subtrees names
		GiSTlist<MTentry *> topEntries;  // list of the parent entries of each subtree
		int nCreated = 0, minHeight = MAXINT;
		char newName[50];
		for (int i=0; i<nInit; i++) {
			sprintf (newName, "%s.%i", name, ++nCreated);
			unlink (newName);
			subtree->Create(newName);  // create the new subtree
			subtree->BulkLoad(array[i], ns[i], padFactor, newName);  // build the subtree

			GiSTpath path;
			path.MakeRoot ();
			MTnode *subtreeRoot = (MTnode *) subtree->ReadNode(path);
			if (subtreeRoot->IsUnderFull(*Store())) {  // if the subtree root node is underfilled, we have to split the tree
				GiSTlist<MTentry *> *parentEntries = new GiSTlist<MTentry *>;
				GiSTlist<char *> *newTreeNames = subtree->SplitTree(&nCreated, subtree->TreeHeight()-1, parentEntries, name);  // split the tree
				nSamples--;
				while (!newTreeNames->IsEmpty()) {  // insert all the new trees in the subtrees list
					subtreeNames.Append (newTreeNames->RemoveFront());
					MTentry *entry = parentEntries->RemoveFront();
					for (int j=0; j<n; j++) {
						if (data[j]->object() == entry->object()) {  // append the parent entry to the list
							topEntries.Append (data[j]);
							break;
						}
					}
					delete entry;
					nSamples++;
				}
				delete newTreeNames;
				delete parentEntries;
				minHeight = MIN (minHeight, subtree->TreeHeight()-1);
			} else {
				subtreeNames.Append (strdup(newName));
				topEntries.Append (data[samples[i]]);
				minHeight = MIN (minHeight, subtree->TreeHeight());
			}
			delete subtreeRoot;
			subtree->Close();
			delete subtree->Store();  // it was created in subtree->Create()
		}
		delete []samples;
		for (int i=0; i<nInit; i++)  {
			for (int j=0; j<ns[i]; j++) {
				delete array[i][j];
			}
			delete []array[i];
		}
		delete []array;
		delete []ns;

		// fix the subtree height
		GiSTlist<char *> subtreeNames2;  // list of the subtrees names
		GiSTlist<MTentry *> topEntries2;  // list of the parent entries of each subtree
		while (!topEntries.IsEmpty()) {  // insert the trees in the list (splitting trees if necessary)
			MTentry *parentEntry = topEntries.RemoveFront ();
			char *tmp = subtreeNames.RemoveFront ();
			strcpy (newName, tmp);
			delete []tmp;
			subtree->Open(newName);
			if (subtree->TreeHeight() > minHeight) {  // we have to split the tree to reduce its height
				nSamples--;
				GiSTlist<MTentry *> *parentEntries = new GiSTlist<MTentry *>;
				GiSTlist<char *> *newTreeNames = subtree->SplitTree(&nCreated, minHeight, parentEntries, name);  // split the tree
				while (!newTreeNames->IsEmpty()) {  // insert all the new trees in the subtrees list
					subtreeNames2.Append (newTreeNames->RemoveFront());
					MTentry *entry = parentEntries->RemoveFront();
					for (int j=0; j<n; j++) {
						if (data[j]->object() == entry->object()) {  // append the parent entry to the parents list
							topEntries2.Append (data[j]);
							break;;
						}
					}
					delete entry;
					nSamples++;
				}
				delete newTreeNames;
				delete parentEntries;
			} else {  // simply insert the tree and its parent entry to the lists
				subtreeNames2.Append (strdup(newName));
				topEntries2.Append (parentEntry);
			}
			subtree->Close();
			delete subtree->Store();  // it was created in tree->Open()
		}

		// build the super tree upon the parents
		MTentry **topEntrArr = new MTentry *[nSamples];  // array of the parent entries for each subtree
		char **subNameArr = new char *[nSamples];  // array of the subtrees names
		for (int i=0; i<nSamples; i++) {  // convert the lists into arrays
			topEntrArr[i] = topEntries2.RemoveFront ();
			subNameArr[i] = subtreeNames2.RemoveFront ();
		}
		assert (topEntries2.IsEmpty());
		assert (subtreeNames2.IsEmpty());
		sprintf (newName, "%s.0", name);
		BulkLoad (topEntrArr, nSamples, padFactor, newName);
		// attach each subtree to the leaves of the super tree
		GiSTpath path;
		path.MakeRoot ();
		MTnode *node = (MTnode *) ReadNode (path);
		GiSTlist<MTnode *> *oldList = new GiSTlist<MTnode *>;  // upper level nodes
		oldList->Append(node);
		int level = node->Level();
		while (level > 0) {  // build the leaves list for super tree
			GiSTlist<MTnode *> *newList = new GiSTlist<MTnode *>;  // lower level nodes
			while (!oldList->IsEmpty()) {
				node = oldList->RemoveFront();
				path = node->Path();
				node->SetLevel(node->Level() + minHeight);  // update level of the upper nodes of the super tree
				WriteNode (node);
				for (int i=0; i<node->NumEntries(); i++) {
					MTentry *entry = (MTentry *) (*node)[i].Ptr();
					path.MakeChild (entry->Ptr());
					newList->Append((MTnode *)ReadNode(path));
					path.MakeParent ();
				}
				delete node;
			}
			delete oldList;
			oldList = newList;
			level--;
		}
		while (!oldList->IsEmpty()) {  // attach each subtree to its leaf
			node = oldList->RemoveFront();  // retrieve next leaf (root of subtree)
			node->SetLevel(minHeight);  // update level of the root of the subtree
			path = node->Path();
			for (int i=0; i<node->NumEntries(); i++) {
				MTentry *entry = (MTentry *) (*node)[i].Ptr();
				path.MakeChild(Store()->Allocate());
				MTnode *newNode = (MTnode *) CreateNode ();
				newNode->Path() = path;
				entry->SetPtr(path.Page());
				path.MakeParent ();
				int j = 0;
				for (; entry->object() != topEntrArr[j]->object(); j++);  // search the position to append
				subtree->Open(subNameArr[j]);
				GiSTpath rootPath;
				rootPath.MakeRoot ();
				Append (newNode, (MTnode *)subtree->ReadNode(rootPath));  // append this subtree to the super tree
				subtree->Close();
				delete subtree->Store();  // it was created in tree->Open()
				delete newNode;
			}
			WriteNode (node);
			delete node;
		}
		subtree->Open(subNameArr[0]);  // in order to destroy the object tree
		delete subtree;
		for (int i=0; i<nSamples; i++) {
			delete []subNameArr[i];
		}
		delete []subNameArr;
		delete []topEntrArr;

		// update radii of the upper nodes of the result M-tree
		path.MakeRoot ();
		node = (MTnode *) ReadNode (path);
		oldList->Append(node);
		level = node->Level();
		while (level >= minHeight) {  // build the list of the nodes which radii should be recomputed
			GiSTlist<MTnode *> *newList = new GiSTlist<MTnode *>;
			while (!oldList->IsEmpty()) {
				node = oldList->RemoveFront();
				path = node->Path();
				for (int i=0; i<node->NumEntries(); i++) {
					path.MakeChild ((*node)[i].Ptr()->Ptr());
					newList->Append((MTnode *)ReadNode(path));
					path.MakeParent ();
				}
				delete node;
			}
			delete oldList;
			oldList = newList;
			level--;
		}
		while (!oldList->IsEmpty()) {  // adjust the radii of the nodes
			MTnode *node = oldList->RemoveFront();
			AdjKeys (node);
			delete node;
		}
		delete oldList;
		for (int i=0; i<=nCreated; i++) {  // delete all temporary subtrees
			sprintf (newName, "%s.%i", name, i);
			unlink (newName);
		}
	} else {  // we can insert all the entries in a single node
		GiSTpath path;
		path.MakeRoot ();
		GiSTnode *node = ReadNode (path);
		for (int i=0; i<n; i++) {
			node->Insert(*(data[i]));
		}
		assert (!node->IsOverFull(*Store()));
		WriteNode (node);
		delete node;
	}
}

// split this M-tree into a list of trees having height level, which is used in the "splitting" phase of the BulkLoad algorithm
// nCreated is the number of created subtrees,
// level is the split level for the tree,
// children is the list of the parents of each subtree,
// name is the root for the subtrees names
// the return value is the list of splitted subtrees's names
GiSTlist<char *> *
MT::SplitTree (int *nCreated, int level, GiSTlist<MTentry *> *parentEntries, const char *name)
{
	GiSTlist<MTnode *> *oldList = new GiSTlist<MTnode *>;  // upper level nodes
	MTnode *node = new MTnode;  // this is because the first operation on node is a delete
	GiSTpath path;
	path.MakeRoot ();
	oldList->Append((MTnode *) ReadNode(path));  // insert the root
	do {  // build the roots list
		GiSTlist<MTnode *> *newList = new GiSTlist<MTnode *>;  // lower level nodes
		while (!oldList->IsEmpty()) {
			delete node;  // delete the old node created by ReadNode
			node = oldList->RemoveFront();  // retrieve next node to be examined
			path = node->Path();
			for (int i=0; i<node->NumEntries(); i++) {  // append all its children to the new list
				path.MakeChild ((*node)[i].Ptr()->Ptr());
				newList->Append((MTnode *)ReadNode(path));
				path.MakeParent ();
			}
		}
		delete oldList;
		oldList = newList;
	} while (node->Level() > level);  // stop if we're at the split level
	delete node;

	GiSTlist<char *> *newTreeNames = new GiSTlist<char *>;  // this is the results list
	while (!oldList->IsEmpty()) {  // now append each sub-tree to its root
		char newName[50];
		sprintf (newName, "%s.%i", name, ++(*nCreated));
		unlink (newName);  // if this M-tree already exists, delete it

		MT *newTree = new MT;
		newTree->Create(newName);  // create a new M-tree
		path.MakeRoot ();
		MTnode *rootNode = (MTnode *) newTree->ReadNode(path);  // read the root of the new tree

		node = oldList->RemoveFront();
		newTree->Append(rootNode, (MTnode *)node->Copy());  // append the current node to the root of new tree
		parentEntries->Append(node->ParentEntry());  // insert the original parent entry into the list
		newTreeNames->Append(strdup(newName));  // insert the new M-tree name into the list
		delete node;
		delete rootNode;
		delete newTree;
	}
	delete oldList;
	return newTreeNames;
}

// append the subtree rooted at from to the node to, which is used in the "append" phase of the BulkLoad algorithm
void
MT::Append (MTnode *to, MTnode *from)
{
	GiSTlist<MTnode *> *oldList = new GiSTlist<MTnode *>;  // upper level nodes to append
	oldList->Append(from);
	GiSTlist<GiSTpath> pathList;
	pathList.Append (to->Path());
	MTnode *node = new MTnode, *newNode = NULL;
	MT *fromTree = (MT *) from->Tree();
	do {
		GiSTlist<MTnode *> *newList = new GiSTlist<MTnode *>;  // lower level nodes to append
		while (!oldList->IsEmpty()) {
			delete node;
			node = oldList->RemoveFront();
			GiSTpath path = pathList.RemoveFront ();
			newNode = (MTnode *) ReadNode (path);  // node to be appended
			for (int i=0; i<node->NumEntries(); i++) {
				MTentry *entry = (MTentry *) (*node)[i].Ptr()->Copy();
				if (node->Level() > 0) {  // if node isn't a leaf, we've to allocate its children
					GiSTpath nodePath = node->Path();
					nodePath.MakeChild (entry->Ptr());
					newList->Append((MTnode *) fromTree->ReadNode(nodePath));
					entry->SetPtr(Store()->Allocate());  // allocate its child in the inserted tree
					path.MakeChild (entry->Ptr());
					MTnode *childNode = (MTnode *) CreateNode ();
					childNode->Path() = path;
					childNode->SetTree(this);
					WriteNode (childNode);  // write the empty node
					delete childNode;
					pathList.Append (path);
					path.MakeParent ();
				}
				newNode->Insert(*entry);
				delete entry;
			}
			newNode->SetLevel(node->Level());
			WriteNode (newNode);  // write the node
			delete newNode;
		}
		delete oldList;
		oldList = newList;
	} while (node->Level() > 0);  // until we reach the leaves' level
	delete node;
	delete oldList;
}

// adjust the keys of node, which is used during the final phase of the BulkLoad algorithm
void
MT::AdjKeys (GiSTnode *node)
{
	if (node->Path().IsRoot()) {
		return;
	}

	GiSTpath parentPath = node->Path();
	parentPath.MakeParent ();
	GiSTnode *parentNode = ReadNode (parentPath);
	GiSTentry *parentEntry = parentNode->SearchPtr(node->Path().Page());  // parent entry
	assert (parentEntry != NULL);
	GiSTentry *unionEntry = node->Union();
	unionEntry->SetPtr(node->Path().Page());
	((MTkey *) unionEntry->Key())->distance = ((MTkey *) parentEntry->Key())->distance;  // necessary to keep track of the distance from the parent
	if (!parentEntry->IsEqual(*unionEntry)) {  // replace this entry
		parentNode->DeleteEntry(parentEntry->Position());
		parentNode->Insert(*unionEntry);
		WriteNode (parentNode);
		AdjKeys (parentNode);
	}
	delete unionEntry;
	delete parentEntry;
	delete parentNode;
}



