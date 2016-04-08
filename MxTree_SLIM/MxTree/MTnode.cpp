#include "MT.h"
#include "MTpredicate.h"

double MIN_UTIL;  // minimum node utilization
pp_function PROMOTE_PART_FUNCTION;  // promotion method
pv_function PROMOTE_VOTE_FUNCTION;  // confirmed promotion method (if needed)
pp_function SECONDARY_PART_FUNCTION;  // root promotion method (can't use stored distances): used only for confirmed and MAX_UB_DIST methods
int NUM_CANDIDATES;  // number of candidates for sampling

extern int IOread;

// used to quick-sort the entries in a node according to their distance from the parent
int MTentryCmp (const void *x, const void *y)
{
	double d = (*(MTentry **)x)->Key()->distance - (*(MTentry **)y)->Key()->distance;
	return d==0 ? 0 : (d>0 ? 1 : -1);
}

// used in Split to find the next nearest entry
int FindMin (double *arr, int len)
{
	int min_i = -1;
	double min = MAXDOUBLE;

	for (int i=0; i<len; i++) {
		if (arr[i] < min) {
			min_i = i;
			min = arr[i];
		}
	}
	return min_i;
}

GiSTobject *
MTnode::NCopy ()
{
	MTnode *newNode = (MTnode *) Copy ();
	newNode->InvalidateEntries();
	return newNode;
}

#ifdef PRINTING_OBJECTS
void
MTnode::Print (ostream& os) const
{
	if (obj) {
		os << *obj << " ";
	}
	os << ((MTnode *)this)->Path() << " #Entries: " << NumEntries() << ", Level " << Level();
	IsLeaf () ? os << "(Leaf)" : os << "(Internal)";
	os << ", Sibling: " << Sibling() << ", Size: " << Size() << "/" << Tree()->Store()->PageSize() << endl;
	for (int i=0; i<NumEntries(); i++) {
		(*this)[i]->Print(os);
	}
}
#endif

// SearchMinPenalty returns where a new entry should be inserted.
// Overriden to insert the distance from the parent in the new entry.
GiSTpage
MTnode::SearchMinPenalty (const GiSTentry& newEntry) const
{
	MTentry *minEntry = NULL;
	MTpenalty *minPenalty = NULL;

	for (int i=0; i<NumEntries(); i++) {
		MTentry *entry = (MTentry *) (*this)[i].Ptr();
		assert ((MTnode *)(entry->Node()) == this);
		MTpenalty *penalty = (MTpenalty *) entry->Penalty(newEntry, minPenalty);  // use the alternate Penalty method in order to avoid some distance computations
		if ((minEntry==NULL) || (*penalty)<(*minPenalty)) {
			minEntry = entry;
			if (minPenalty) {
				delete minPenalty;
			}
			minPenalty = penalty;
		} else {
			delete penalty;
		}
	}
	((MTentry&)newEntry).Key()->distance = minPenalty->distance;
	delete minPenalty;
	return minEntry->Ptr();
}

GiSTlist<MTentry *>
MTnode::RangeSearch (const MTquery &query)
{
	GiSTlist<MTentry *> results;

	if (IsLeaf()) {
		for (int i=0; i<NumEntries(); i++) {
			MTentry *entry = (MTentry *) (*this)[i].Ptr()->Copy();
			MTquery *newQuery = (MTquery *) query.Copy();
			if (newQuery->Consistent(*entry)) {  // object qualifies
				entry->SetMaxRadius(newQuery->Grade());
				results.Append (entry);
			} else {
				delete entry;
			}
			delete newQuery;
		}
	} else {
		for (int i=0; i<NumEntries(); i++) {
			MTentry *entry = (MTentry *) (*this)[i].Ptr();
			MTquery *newQuery = (MTquery *) query.Copy();
			if (newQuery->Consistent(*entry)) {  // sub-tree included
				GiSTpath childPath = Path ();
				childPath.MakeChild (entry->Ptr());
				MTnode *childNode = (MTnode *) ((MT *)Tree())->ReadNode(childPath);
				GiSTlist<MTentry *> childResults = childNode->RangeSearch(*newQuery);  // recurse the search
				while (!childResults.IsEmpty()) {
					results.Append (childResults.RemoveFront());
				}
				delete childNode;
			}
			delete newQuery;
		}
	}

	return results;
}

// insert before the original number according to the sequence instead of index
void
MTnode::InsertBefore (const GiSTentry& newEntry, int index)
{
	int n = NumEntries ();
	assert (index>=0 && index<=n);

	BOOL bOrder = TRUE;
	if (index > 0) {
		bOrder = (*this)[index-1]->Compare(newEntry) <= 0;
	}
	if (index < n) {
		bOrder = bOrder && (*this)[index]->Compare(newEntry) >= 0;
	}

	if (bOrder) {  // yes, the position is right for this entry
		GiSTentry *entry = (GiSTentry *) newEntry.Copy ();
		entry->SetLevel(Level());
		entry->SetPosition(index);
		entry->SetNode(this);
		// Move everything else over
		for (int i=n; i>index; i--) {
			GiSTentry *e = (*this)[i-1];
			e->SetPosition(i);
			(*this)[i] = e;
		}
		// Stick the entry in
		(*this)[index] = entry;
		// Bump up the count
		SetNumEntries (n+1);
	} else {
		Insert (newEntry);  // find the right place
	}
}

GiSTnode *
MTnode::PickSplit ()
{
	int lDel, rDel;  // number of entries to be deleted from each node
	int *lArray = new int[NumEntries()], *rArray = new int[NumEntries()];  // array of entries to be deleted from each node

	// promote the right node (possibly reassigning the left node);
	// the right node's page is copied from left node;
	// we'll delete from the nodes as appropriate after the splitting phase
	MTnode *rNode = PromotePart ();
	// now perform the split
	Split (rNode, lArray, rArray, &lDel, &rDel);  // complexity: O(n)
	// given the deletion vectors, do bulk deletes
	DeleteBulk (lArray, lDel);
	rNode->DeleteBulk(rArray, rDel);
	// order the entries in both nodes
	SortEntries ();
	rNode->SortEntries();

	delete []lArray;
	delete []rArray;
	return rNode;  // return the right node
}

GiSTentry *
MTnode::Union () const
{
	Object *objTemp = NULL;
	if (!obj) {  // retrieve the node's parent object
		MTentry *parentEntry = ParentEntry ();
		((MTnode *)this)->obj = (objTemp = new Object(parentEntry->object()));
		delete parentEntry;
	}

	GiSTpath path = ((MTnode *)this)->Path();
	MTentry *unionEntry = new MTentry;
	unionEntry->InitKey();
	if (path.Level() > 1) {  // len>=3
		MTentry *parentEntry = ParentEntry ();
		if (parentEntry) {  // copy the entry
			unionEntry->Key()->distance = parentEntry->Key()->distance;
			if (parentEntry->Key()->splitted) {
				unionEntry->Key()->splitted = TRUE;  
			}
			delete parentEntry;
		}
		if (unionEntry->Key()->distance == -MaxDist()) {  // compute the distance from the parent
			MTnode *parentNode = ((MT *)Tree())->ParentNode((MTnode *)this);
			MTentry *grandEntry = parentNode->ParentEntry();
			unionEntry->Key()->distance = obj->distance(grandEntry->object());
			unionEntry->Key()->splitted = TRUE;  
			delete grandEntry;
			delete parentNode;
		}
	}
	unionEntry->SetObject(*obj);
	unionEntry->SetMaxRadius(0);
	unionEntry->SetMinRadius(MAXDOUBLE);
	mMRadius (unionEntry);  // compute the radii
	if (objTemp) {
		delete objTemp;
	}
	((MTnode *)this)->obj = NULL;
	return unionEntry;
}

MTnode *
MTnode::PromotePart ()
{
	MTnode *newNode = NULL;

	switch (PROMOTE_PART_FUNCTION) {
		case RANDOM: {  // pick two *different* random entries, complexity: constant
			int i = PickRandom (0, NumEntries());
			int j;
			do {
				j = PickRandom (0, NumEntries());
			} while (i == j);
			if (((MTentry *)(*this)[j].Ptr())->Key()->distance == 0) {  // if we chose the parent entry, put it in the left node
				int temp = i;
				i = j;
				j = temp;
			}
			// re-assign the nodes' object
			newNode = (MTnode *) NCopy ();
			newNode->obj = &((MTentry *)((*newNode)[j].Ptr()))->object();
			obj = &((MTentry *)((*this)[i].Ptr()))->object();
			if (((MTentry *)(*this)[i].Ptr())->Key()->distance > 0) {  // unconfirmed, invalidate also the parent
				InvalidateEntry (TRUE);
				InvalidateEntries ();
			} else {
				InvalidateEntry (FALSE);  // confirmed, invalidate only the node's radii
			}
			break;
		}
		case CONFIRMED: {  // complexity: determined by the confirmed promotion algorithm
			if (((MTentry *)((*this)[0].Ptr()))->Key()->distance == -MaxDist()) {  // if we're splitting the root we have to use a policy that doesn't use stored distances
				PROMOTE_PART_FUNCTION = SECONDARY_PART_FUNCTION;
				newNode = PromotePart ();
				PROMOTE_PART_FUNCTION = CONFIRMED;
			} else {
				int index = -1;
				for (int i=0; i<NumEntries() /*&& index<0*/; i++) {
					if (((MTentry *)((*this)[i].Ptr()))->Key()->distance == 0) {  // parent obj
						index = i;
					}
				}
				obj = &((MTentry *)((*this)[index].Ptr()))->object();
				newNode = PromoteVote ();  // now choose the right node parent
			}
			InvalidateEntry (FALSE);
			break;
		}
		case SAMPLING: {  // complexity: O(kn) distance computations
			int n = NumEntries (), count = MIN (NUM_CANDIDATES, n-1);
			double **distances = new double*[count];  // distance matrix
			int *vec = PickCandidates ();
			// initialize distance matrix
			for (int i=0; i<count; i++) {
				distances[i] = new double[n];
				for (int j=0; j<n; j++) {
					j==vec[i] ? distances[i][j]=0 : distances[i][j]=-MaxDist();
				}
			}

			// find the candidates with minimum radius
			int min1, min2;
			double min = MAXDOUBLE, secMin = MAXDOUBLE;
			for (int i=1; i<count; i++) {
				for (int j=0; j<i; j++) {
					MTnode *node1 = (MTnode *) NCopy (), *node2 = (MTnode *) NCopy ();
					for (int k=0; k<n; k++) {
						((MTentry *)(*node1)[k].Ptr())->Key()->distance = distances[i][k];
						((MTentry *)(*node2)[k].Ptr())->Key()->distance = distances[j][k];
					}
					node1->obj = &((MTentry *)(*this)[vec[i]].Ptr())->object();
					node2->obj = &((MTentry *)(*this)[vec[j]].Ptr())->object();

					// perform the split
					int lDel, rDel, *lVec = new int[n], *rVec = new int[n];
					node1->Split(node2, lVec, rVec, &lDel, &rDel);
					for (int k=0; k<n; k++) {
						distances[i][k] = ((MTentry *)(*node1)[k].Ptr())->Key()->distance;
						distances[j][k] = ((MTentry *)(*node2)[k].Ptr())->Key()->distance;
					}
					node1->DeleteBulk(lVec, lDel);
					node2->DeleteBulk(rVec, rDel);

					MTentry *entry1 = new MTentry, *entry2 = new MTentry;
					entry1->InitKey();
					entry2->InitKey();
					entry1->SetObject(*node1->obj);
					entry2->SetObject(*node2->obj);
					entry1->SetMaxRadius(0);
					entry2->SetMaxRadius(0);
					entry1->SetMinRadius(MAXDOUBLE);
					entry2->SetMinRadius(MAXDOUBLE);
					node1->mMRadius(entry1);
					node2->mMRadius(entry2);
					// check the result
					double val1 = MAX (entry1->MaxRadius(), entry2->MaxRadius()), val2 = MIN (entry1->MaxRadius(), entry2->MaxRadius());
					if (val1<min || (val1==min && val2<secMin)) {
						min = val1;
						secMin = val2;
						min1 = i;
						min2 = j;
					}
					// be tidy
					delete entry1;
					delete entry2;
					delete []lVec;
					delete []rVec;
					delete node1;
					delete node2;
				}
			}

			newNode = (MTnode *) NCopy ();
			obj = &((MTentry *)(*this)[vec[min1]].Ptr())->object();
			newNode->obj = &((MTentry *)(*newNode)[vec[min2]].Ptr())->object();
			// the parent object wasn't confirmed, invalidate also the parent
			InvalidateEntry (TRUE);
			InvalidateEntries ();
			for (int i=0; i<n; i++) {
				((MTentry *)(*this)[i].Ptr())->Key()->distance = distances[min1][i];
				((MTentry *)(*newNode)[i].Ptr())->Key()->distance = distances[min2][i];
			}

			for (int i=0; i<count; i++) {
				delete []distances[i];
			}
			delete []distances;
			break;
		}
		case mM_RAD: {  // complexity: O(n^2) distance computations
			int n = NumEntries ();
			double **distances = new double *[n];  // distance matrix
			// initialize distance matrix
			for (int i=0; i<n; i++) {
				distances[i] = new double[n];
				for (int j=0; j<n; j++) {
					j==i ? distances[i][j]=0 : distances[i][j]=-MaxDist();
				}
			}
			for (int i=0; i<n; i++) {
				if (((MTentry *)(*this)[i].Ptr())->Key()->distance == 0) {
					for (int j=0; j<n; j++) {
						distances[j][i] = distances[i][j] = ((MTentry *)(*this)[j].Ptr())->Key()->distance;
					}
					break;
				}
			}

			int min1, min2;
			double min = MAXDOUBLE, secMin = MAXDOUBLE;
			for (int i=1; i<n; i++) {
				for (int j=0; j<i; j++) {
					MTnode *node1 = (MTnode *) NCopy (), *node2 = (MTnode *) NCopy ();
					for (int k=0; k<n; k++) {
						((MTentry *)(*node1)[k].Ptr())->Key()->distance = distances[i][k];
						((MTentry *)(*node2)[k].Ptr())->Key()->distance = distances[j][k];
					}
					node1->obj = &((MTentry *)(*this)[i].Ptr())->object();
					node2->obj = &((MTentry *)(*this)[j].Ptr())->object();

					// perform the split
					int lDel, rDel, *lVec=new int[n], *rVec=new int[n];
					node1->Split(node2, lVec, rVec, &lDel, &rDel);
					for (int k=0; k<n; k++) {
						distances[k][i] = distances[i][k] = ((MTentry *)(*node1)[k].Ptr())->Key()->distance;
						distances[k][j] = distances[j][k] = ((MTentry *)(*node2)[k].Ptr())->Key()->distance;
					}
					node1->DeleteBulk(lVec, lDel);
					node2->DeleteBulk(rVec, rDel);

					MTentry *entry1 = new MTentry, *entry2 = new MTentry;
					entry1->InitKey();
					entry2->InitKey();
					entry1->SetObject(*node1->obj);
					entry2->SetObject(*node2->obj);
					entry1->SetMaxRadius(0);
					entry2->SetMaxRadius(0);
					entry1->SetMinRadius(MAXDOUBLE);
					entry2->SetMinRadius(MAXDOUBLE);
					node1->mMRadius(entry1);
					node2->mMRadius(entry2);
					// check the result
					double val1 = MAX (entry1->MaxRadius(), entry2->MaxRadius()), val2 = MIN (entry1->MaxRadius(), entry2->MaxRadius());
					if (val1<min || (val1==min && val2<secMin)) {
						min = val1;
						secMin = val2;
						min1 = i;
						min2 = j;
					}
					// be tidy
					delete entry1;
					delete entry2;
					delete []lVec;
					delete []rVec;
					delete node1;
					delete node2;
				}
			}

			((MTentry *)(*this)[min2].Ptr())->Key()->distance>0 ? newNode=(MTnode *)NCopy() : newNode=(MTnode *)Copy();
			obj = &((MTentry *)(*this)[min1].Ptr())->object();
			newNode->obj = &((MTentry *)(*newNode)[min2].Ptr())->object();
			if (((MTentry *)(*this)[min1].Ptr())->Key()->distance > 0) {  // unconfirmed, invalidate also the parent
				InvalidateEntry (TRUE);
				InvalidateEntries ();
			} else {
				InvalidateEntry (FALSE);  // else, invalidate only the node's radii
			}
			for (int i=0; i<n; i++) {
				((MTentry *)(*this)[i].Ptr())->Key()->distance = distances[min1][i];
				((MTentry *)(*newNode)[i].Ptr())->Key()->distance = distances[min2][i];
			}

			for (int i=0; i<n; i++) {
				delete []distances[i];
			}
			delete []distances;
			break;
		}
	}
	return newNode;
}

MTnode *
MTnode::PromoteVote ()
{
	MTnode *newNode = (MTnode *) NCopy ();

	switch (PROMOTE_VOTE_FUNCTION) {
		case RANDOMV: {  // complexity: constant
			int i;
			// pick a random entry (different from the parent)
			do {
				i = PickRandom (0, NumEntries());
			} while (((MTentry *)(*this)[i].Ptr())->Key()->distance == 0);
			newNode->obj = &((MTentry *)((*newNode)[i].Ptr()))->object();
			break;
		}
		case SAMPLINGV: {  // complexity: O(kn) distance computations
			int n = NumEntries (), count = MIN (NUM_CANDIDATES, n-1);
			int *vec = PickCandidates (), bestCand = 0;
			double min = MAXDOUBLE, secMin = MAXDOUBLE, **distances = new double *[count];  // distance matrix
			// find the candidate with minimum radius
			for (int i=0; i<count; i++) {
				MTentry *cand = (MTentry *) ((*this)[vec[i]].Ptr());
				MTnode *node1 = (MTnode *) Copy (), *node2 = (MTnode *) NCopy ();
				distances[i] = new double[n];
				for (int j=0; j<n; j++) {
					distances[i][j] = (vec[i]==j) ? 0 : cand->object().distance(((MTentry *)((*this)[j].Ptr()))->object());  // initialize distance matrix
					((MTentry *)((*node2)[j].Ptr()))->Key()->distance = distances[i][j];  // if parent entry is i
				}
				node1->obj = obj;
				node2->obj = &((MTentry *)((*this)[vec[i]].Ptr()))->object();

				// perform the split
				int *lVec = new int[n], *rVec = new int[n], lDel, rDel;
				node1->Split(node2, lVec, rVec, &lDel, &rDel);
				node1->DeleteBulk(lVec, lDel);
				node2->DeleteBulk(rVec, rDel);

				MTentry *entry1 = new MTentry, *entry2 = new MTentry;
				entry1->InitKey();
				entry2->InitKey();
				entry1->SetObject(*node1->obj);
				entry2->SetObject(*node2->obj);
				entry1->SetMaxRadius(0);
				entry2->SetMaxRadius(0);
				entry1->SetMinRadius(MAXDOUBLE);
				entry2->SetMinRadius(MAXDOUBLE);
				// compute the radii
				node1->mMRadius(entry1);
				node2->mMRadius(entry2);

				// check the result
				double val1 = MAX (entry1->MaxRadius(), entry2->MaxRadius()), val2 = MIN (entry1->MaxRadius(), entry2->MaxRadius());
				if (val1<min || (val1==min && val2<secMin)) {
					min = val1;
					secMin = val2;
					bestCand = i;
				}
				// be tidy
				delete entry1;
				delete entry2;
				delete []lVec;
				delete []rVec;
				delete node1;
				delete node2;
			}

			newNode->obj = &((MTentry *)((*newNode)[vec[bestCand]].Ptr()))->object();
			// update the distance of the children from the new parent
			for (int i=0; i<n; i++) {
				((MTentry *)((*newNode)[i].Ptr()))->Key()->distance = distances[bestCand][i];
			}
			for (int i=0; i<count; i++) {
				delete []distances[i];
			}
			delete []distances;
			delete []vec;
			break;
		}
		case MAX_LB_DISTV: {  // complexity: constant
			double maxDist = -1;
			int maxCand = 0;
			if (Tree()->IsOrdered()) {  // if the tree is ordered we can choose the last element
				maxCand = NumEntries() - 1;
			} else {  // otherwise we have to search the object which is farthest from the parent
				for (int i=0; i<NumEntries(); i++) {
					MTentry *entry = (MTentry *) (*this)[i].Ptr();
					if (entry->Key()->distance > maxDist) {
						maxDist = entry->Key()->distance;
						maxCand = i;
					}
				}
			}
			newNode->obj = &((MTentry *)(*newNode)[maxCand].Ptr())->object();
			break;
		}
		case mM_RADV: {  // complexity: O(n) distance computations
			int n = NumEntries ();
			double **distances = new double *[n];  // distance matrix
			for (int i=0; i<n; i++) {
				distances[i] = new double[n];
			}
			for (int i=0; i<n; i++) {  // initialize distance matrix
				for (int j=i; j<n; j++) {
					distances[j][i] = distances[i][j] = -MaxDist();
				}
			}

			// find the candidate meeting the requirement of minimizing max(1->maxradius, 2->maxradius)
			double min = MAXDOUBLE;
			int bestCand = 0;
			for (int i=0; i<n; i++) {
				MTentry *cand = (MTentry *) (*this)[i].Ptr();
				if (cand->Key()->distance == 0) {  // parent entry, actually we can neglect it
					for (int j=0; j<n; j++) {
						 distances[j][i] = distances[i][j] = ((MTentry *)(*this)[j].Ptr())->Key()->distance;
					}
					continue;
				}

				MTnode *node1 = (MTnode *) Copy (), *node2 = (MTnode *) NCopy ();
				for (int j=0; j<n; j++) {
					distances[j][i] = distances[i][j] = (i==j) ? 0 : (distances[i][j]==-MaxDist() ? cand->object().distance(((MTentry *)(*this)[j].Ptr())->object()) : distances[i][j]);
					((MTentry *)(*node2)[j].Ptr())->Key()->distance = distances[i][j];  // if parent entry is i
				}
				node1->obj = obj;
				node2->obj = &((MTentry *)(*this)[i].Ptr())->object();

				// perform the split
				int *lVec = new int[n], *rVec = new int[n], lDel, rDel;
				node1->Split(node2, lVec, rVec, &lDel, &rDel);
				node1->DeleteBulk(lVec, lDel);
				node2->DeleteBulk(rVec, rDel);

				MTentry *entry1 = new MTentry, *entry2 = new MTentry;
				entry1->InitKey();
				entry2->InitKey();
				entry1->SetObject(*node1->obj);
				entry2->SetObject(*node2->obj);
				entry1->SetMaxRadius(0);
				entry2->SetMaxRadius(0);
				entry1->SetMinRadius(MAXDOUBLE);
				entry2->SetMinRadius(MAXDOUBLE);
				node1->mMRadius(entry1);
				node2->mMRadius(entry2);

				// check the result
				double val = MAX (entry1->MaxRadius(), entry2->MaxRadius());
				if (val < min) {
					min = val;
					bestCand = i;
				}
				// be tidy
				delete entry1;
				delete entry2;
				delete []lVec;
				delete []rVec;
				delete node1;
				delete node2;
			}

			newNode->obj = &((MTentry *)(*newNode)[bestCand].Ptr())->object();
			// update the distance of the children from the new parent
			for (int i=0; i<n; i++) {
				((MTentry *)(*newNode)[i].Ptr())->Key()->distance = distances[bestCand][i];
			}

			for (int i=0; i<n; i++) {
				delete []distances[i];
			}
			delete []distances;
			break;
		}
	}
	return newNode;
}

int *
MTnode::PickCandidates ()
{
	int n = NumEntries ();
	BOOL *bUsed = new BOOL[n];
	for (int i=0; i<n; i++) {
		bUsed[i] = ((MTentry *)(*this)[i].Ptr())->Key()->distance == 0;  // exclude parent entry
	}

	int count = MIN (NUM_CANDIDATES, n-1), *results = new int[count];
	// insert in results the indices of the candidates for promotion
	for (int i=0; i<count; i++) {
		int j;
		do {
			j = PickRandom (0, n);
		} while (bUsed[j]);
		results[i] = j;
		bUsed[j] = TRUE;
	}

	delete []bUsed;
	return results;
}

void
MTnode::Split (MTnode *node, int *lArray, int *rArray, int *lDel, int *rDel)
{
	*lDel = *rDel = 0;
	int n = NumEntries ();
	double *lDist = new double[n], *rDist = new double[n];
	for (int i=0; i<n; i++) {  // compute the distance between entries and their parent entry
		lDist[i] = Distance (i);
		rDist[i] = node->Distance(i);
	}
	// balance entries up to minimum utilization
	while (*lDel < n*MIN_UTIL && *rDel < n*MIN_UTIL) {
		int i = FindMin (lDist, n);
		((MTentry *)(*this)[i].Ptr())->Key()->distance = lDist[i];
		((MTentry *)(*node)[i].Ptr())->Key()->distance = rDist[i];
		rArray[(*rDel)++] = i;
		lDist[i] = rDist[i] = MAXDOUBLE;
		i = FindMin (rDist, n);
		if (i >= 0) {
			((MTentry *)(*node)[i].Ptr())->Key()->distance = rDist[i];
			((MTentry *)(*this)[i].Ptr())->Key()->distance = lDist[i];
			lArray[(*lDel)++] = i;
			lDist[i] = rDist[i] = MAXDOUBLE;
		}
	}
	// then perform the hyperplane split (assigning each entry to its nearest node)
	for (int i=0; i<n; i++) {
		if (rDist[i] == MAXDOUBLE) {  // entry have been assigned through balanced policy
			continue;
		}
		((MTentry *)(*this)[i].Ptr())->Key()->distance = lDist[i];
		((MTentry *)(*node)[i].Ptr())->Key()->distance = rDist[i];
		((MTentry *)(*this)[i].Ptr())->Key()->distance < ((MTentry *)(*node)[i].Ptr())->Key()->distance ? rArray[(*rDel)++] = i : lArray[(*lDel)++] = i;
	}
	delete []rDist;
	delete []lDist;
}

int
MTnode::IsUnderFull (const GiSTstore &store) const
{
	return MIN_UTIL>0 && Size()<(int)(store.PageSize()*MIN_UTIL);
}

void
MTnode::InvalidateEntry (BOOL bNew)
{
	GiSTpath path = Path ();
	if (path.Level() > 1) {  // len>=3
		MTnode *parentNode = ((MT *)Tree())->ParentNode((MTnode *)this);
		for (int i=0; i<parentNode->NumEntries(); i++) {  // search the entry in the parent's node
			MTentry *entry = (MTentry *) (*parentNode)[i].Ptr();
			if (entry->Ptr() == path.Page()) {
				if (bNew) {
					entry->Key()->distance = -MaxDist();
				}
				entry->Key()->splitted = TRUE;
				break;
			}
		}
		path.MakeParent ();
		MTnode *grandNode = ((MT *)Tree())->ParentNode(parentNode);
		for (int i=0; i<grandNode->NumEntries(); i++) {  // search the entry in the grandparent's node
			MTentry *entry = (MTentry *) (*grandNode)[i].Ptr();
			if (entry->Ptr() == path.Page()) {
				entry->SetMaxRadius(-1);
				break;
			}
		}
		((MT *)Tree())->WriteNode(parentNode);  // write parent node (in inconsistent state)
		((MT *)Tree())->WriteNode(grandNode);  // write grandparent node (to invalidate the parent's entry)
		delete parentNode;
		delete grandNode;
	}
}

void
MTnode::InvalidateEntries ()
{
	for (int i=0; i<NumEntries(); i++) {
		((MTentry *)((*this)[i].Ptr()))->Key()->distance = -MaxDist();
	}
}

void
MTnode::SortEntries ()
{
	int n = NumEntries ();
	MTentry **array = new MTentry *[n], *objEntry = NULL;
	for (int i=0; i<n; i++) {
		array[i] = (MTentry *) ((MTentry *)(*this)[i].Ptr())->Copy();
		if (&((MTentry *)(*this)[i].Ptr())->object() == obj) {
			objEntry = array[i];
		}
	}
	qsort (array, n, sizeof(MTentry *), MTentryCmp);
	while (NumEntries() > 0) {
		DeleteEntry (0);  // also delete the obj
	}

	int obji = -1;
	for (int i=0; i<n; i++) {
		InsertBefore (*(array[i]), i);
		if (array[i] == objEntry) {
			obji = i;
		}
		delete array[i];
	}
	delete []array;

	if (obji >= 0) {
		obj = &((MTentry *)(*this)[obji].Ptr())->object();
	}
}

void
MTnode::mMRadius (MTentry *unionEntry) const
{
	for (int i=0; i<NumEntries(); i++) {
		MTentry *entry = (MTentry *) (*this)[i].Ptr();
		entry->Key()->splitted = FALSE;
		if (entry->Key()->distance < 0) {
			entry->Key()->distance = obj->distance(entry->object());
		}

		double tempMax = entry->Key()->distance + entry->MaxRadius();
		if (tempMax > unionEntry->MaxRadius()) {
			unionEntry->SetMaxRadius(tempMax);
		}
		double tempMin = MAX(entry->Key()->distance - entry->MaxRadius(), 0);
		if (tempMin < unionEntry->MinRadius()) {
			unionEntry->SetMinRadius(tempMin);
		}
	}
}

MTentry *
MTnode::ParentEntry () const
{
	MTnode *parentNode = ((MT *)Tree())->ParentNode((MTnode *)this);
	MTentry *parentEntry = NULL;

	for (int i=0; i < parentNode->NumEntries() && !parentEntry; i++) {
		if ((*parentNode)[i].Ptr()->Ptr() == ((MTnode *)this)->Path().Page()) {
			parentEntry = (MTentry *) (*parentNode)[i].Ptr()->Copy();
		}
	}
	delete parentNode;
	return parentEntry;
}

double
MTnode::Distance (int i) const
{
	MTentry *entry = (MTentry *) ((*this)[i].Ptr());
	double dist = entry->Key()->distance;
	return (dist < 0) ? ((dist > -MaxDist()) ? -dist : obj->distance(entry->object())) : dist;
}