#include "MXTnode.h"
#include "MSTree.h"
#include "VPTree.h"
#include "MTpredicate.h"
#include "MXTree.h"

MXTnode::MXTnode(const MXTnode& node)
{
	pageNum = node.pageNum;
	vptree = NULL;  // Do not copy!

	maxEntries = node.maxEntries;
	numEntries = node.numEntries;
	level = node.level;
	sibling = node.sibling;
	tree = node.tree;
	path = node.path;
	if (node.packedNode) {
		packedNode = new char[tree->Store()->PageSize()*pageNum];
		memcpy(packedNode, node.packedNode, tree->Store()->PageSize()*pageNum);
	} else {
		packedNode = NULL;
	}
	if (maxEntries) {
		entries = new GiSTentryptr[maxEntries];
	} else {
		entries = NULL;
	}
	for (int i=0; i<numEntries; i++) {
		entries[i] = (GiSTentry*) node.entries[i]->Copy();
		((GiSTentry *)entries[i])->SetNode(this);
	}

	obj = node.obj;
}

MXTnode::~MXTnode()
{
	delete vptree;
}

int MXTnode::Size() const
{
	int size = sizeof(MXTnodeHeader) + numEntries*(FixedLength()+sizeof(GiSTpage));
	if (this->IsLeaf()) {
		size += (vptree->GetVPNum()*(sizeof(double)+sizeof(int)) + sizeof(int));
	}
	return size;
}

int MXTnode::IsOverFull(const GiSTstore &store) const
{
	int size = sizeof(MXTnodeHeader) + numEntries*(FixedLength()+sizeof(GiSTpage));
	if (this->IsLeaf()) {
		size += sizeof(int);
	}
	return size > store.PageSize()*pageNum;
}

GiSTcompressedEntry MXTnode::Entry(int entryPos) const
{
	// Look up the line table
	GiSTlte keyPhysicalPos, nextKeyPhysicalPos;
	int fixlen = FixedLength();

	keyPhysicalPos = sizeof(MXTnodeHeader) + (sizeof(GiSTpage) + fixlen) * entryPos;
	nextKeyPhysicalPos = keyPhysicalPos + sizeof(GiSTpage) + fixlen;

	// Allocate and set up the return key
	GiSTcompressedEntry entry;
	entry.keyLen = nextKeyPhysicalPos - keyPhysicalPos - sizeof(GiSTpage);
	if (entry.keyLen > 0) {
		entry.key = new char[entry.keyLen];
		memcpy(entry.key, packedNode+keyPhysicalPos, entry.keyLen);
	}
	memcpy(&entry.ptr, packedNode+keyPhysicalPos+entry.keyLen, sizeof(GiSTpage));
	return entry;
}

void MXTnode::Pack(char *buffer)
{
	// Pack the header
	MXTnodeHeader *h = (MXTnodeHeader *)buffer;

	h->level = level;
	h->numEntries = numEntries;

	int fixlen = FixedLength();
	GiSTlte ltptr = sizeof(MXTnodeHeader);

	for (int i=0; i<numEntries; i++) {
		GiSTcompressedEntry compressedEntry = (*this)[i]->Compress();
		assert(fixlen == compressedEntry.keyLen);
		// Copy the entry onto the page
		if (compressedEntry.keyLen > 0) {
			memcpy(buffer+ltptr, compressedEntry.key, compressedEntry.keyLen);
		}
		memcpy(buffer+ltptr+compressedEntry.keyLen, &compressedEntry.ptr, sizeof(GiSTpage));
		// Be tidy
		if (compressedEntry.key) {
			delete compressedEntry.key;
		}
		// Enter a pointer to the entry in the line table
		ltptr += compressedEntry.keyLen + sizeof(GiSTpage);
	}

	if (this->IsLeaf()) {
		char *pos = buffer + tree->Store()->PageSize()*pageNum;
		int freeNum = (tree->Store()->PageSize()*pageNum-sizeof(int)-ltptr) / (sizeof(double)+sizeof(int));
		int vpNum = (this->numEntries<freeNum)? this->numEntries: freeNum;   
		memcpy(pos-sizeof(int), &vpNum, sizeof(int));
		if (vptree) {
			delete vptree;
		}
		vptree = new VPTree(this, vpNum);
		vptree->Build();
		vptree->Compress(pos-sizeof(int)-vpNum*(sizeof(double)+sizeof(int)));
	}
}

void MXTnode::Unpack(const char *buffer)
{
	const MXTnodeHeader *h = (const MXTnodeHeader *)buffer;

	Reset();
	level = h->level;
	
	int size = sizeof(MXTnodeHeader) + h->numEntries*(EntrySize()+sizeof(GiSTpage));
	if (this->IsLeaf()) {
		size += sizeof(int);
	}
	pageNum = ceil((float)size/(float)tree->Store()->PageSize());

	if (!packedNode) {
		packedNode = new char[tree->Store()->PageSize()*pageNum];
	}
	memcpy(packedNode, buffer, tree->Store()->PageSize()*pageNum);

	Expand(h->numEntries);
	numEntries = h->numEntries;

	for (int i=0; i<numEntries; i++) {
		GiSTcompressedEntry tmpentry = Entry(i);
		GiSTentry *e = CreateEntry();
		e->SetLevel(level);
		e->SetPosition(i);
		e->SetNode(this);
		e->Decompress(tmpentry);
		// be tidy
		if (tmpentry.key) {
			delete tmpentry.key;
		}
		// Append the body with the entry
		entries[i] = e;
	}

	if (this->IsLeaf()) {
		const char *pos = packedNode + tree->Store()->PageSize()*pageNum;
		int vpNum;
		memcpy(&vpNum, pos-sizeof(int), sizeof(int));
		assert(vptree == NULL);
		vptree = new VPTree(this, vpNum);
		vptree->Decompress(pos-sizeof(int)-vpNum*(sizeof(double)+sizeof(int)));
	}
}

#ifdef PRINTING_OBJECTS
void MXTnode::Print(ostream& os) const
{
	if (obj) {
		os << *obj << " ";
	}
	os << path << " #Entries: " << numEntries << ", Level " << level;
	IsLeaf () ? os << "(Leaf)" : os << "(Internal)";
	os << ", Size: " << Size() << "/" << Tree()->Store()->PageSize()*pageNum << endl;
	for (int i=0; i<numEntries; i++) {
		(*this)[i]->Print(os);
	}
}
#endif

int MXTnode::Condense()
{
	int size = sizeof(MXTnodeHeader) + numEntries*(FixedLength()+sizeof(GiSTpage));
	if (this->IsLeaf()) {
		size += sizeof(int);
	}
	pageNum = ceil((float)size/(float)tree->Store()->PageSize());
	return pageNum;
}

void MXTnode::TestPromotion(double radii[], double *dist, int pageNums[], int cands[], double *dists, vector<vector<int>>& vec)
{
	int n = NumEntries();
	double **distances = new double *[n]; 
	for (int i=0; i<n; i++) {
		distances[i] = new double[n];
		for (int j=0; j<n; j++) {
			j==i ? distances[i][j]=0 : distances[i][j]=-MaxDist();
		}
	}

	int start = -1;
	for (int i=0; i<n; i++) {
		if (((MTentry *)(*this)[i].Ptr())->Key()->distance == 0) {
			start = i;
			for (int j=0; j<n; j++) {
				distances[j][i] = distances[i][j] = ((MTentry *)(*this)[j].Ptr())->Key()->distance;
			}
			break;
		}
	}
	start = (start==-1 ? 0:start); 

	for (int i=1; i<n; i++) {
		Object& obji = ((MTentry *)(*this)[i].Ptr())->object();
		for (int j=0; j<i; j++) {
			if (distances[i][j] != -MaxDist()) {
				continue;
			}
			Object& objj = ((MTentry *)(*this)[j].Ptr())->object();
			distances[j][i] = distances[i][j] = obji.distance(objj);
		}
	}

	MSTree *mstree = new MSTree(start);
	for (int i=0; i<n; i++) {
		vector<double> v(n);
		for (int j=0; j<n; j++) {
			v[j] = ((i==j) ? MAXDOUBLE : distances[i][j]);
		}
		mstree->Input(v);
	}
	mstree->Build();
	vector<double> maxRadii;
	for (int i=0; i<n; i++) {
		maxRadii.push_back(((MTentry *)(*this)[i].Ptr())->MaxRadius());
	}
	mstree->Divide(vec[0], vec[1], radii, cands, maxRadii);
	delete mstree;

	(*dist) = distances[cands[0]][cands[1]];

	int size = sizeof(MXTnodeHeader);
	if (this->IsLeaf()) {
		size += sizeof(int);
	}
	int fixlen = FixedLength();
	pageNums[0] = ceil((float)(vec[0].size()*(fixlen+sizeof(GiSTpage))+size)/(float)tree->Store()->PageSize());
	pageNums[1] = ceil((float)(vec[1].size()*(fixlen+sizeof(GiSTpage))+size)/(float)tree->Store()->PageSize());

	for (int i=0; i<n; i++) {
		dists[i] = distances[cands[0]][i];
		dists[n+i] = distances[cands[1]][i];
	}

	for (int i=0; i<n; i++) {
		delete[] distances[i];
	}
	delete[] distances;
}

MTnode *MXTnode::Promote(int cands[], double *dists)
{
	MTnode *newNode = ((MTentry *)(*this)[cands[1]].Ptr())->Key()->distance>0 ? (MTnode *)NCopy() : (MTnode *)Copy();

	obj = &((MTentry *)(*this)[cands[0]].Ptr())->object();
	((MXTnode *)newNode)->obj = &((MTentry *)(*newNode)[cands[1]].Ptr())->object();
	if (((MTentry *)(*this)[cands[0]].Ptr())->Key()->distance > 0) {  // unconfirmed, invalidate also the parent
		InvalidateEntry(TRUE);
		InvalidateEntries();
	} else {
		InvalidateEntry(FALSE);  // else, invalidate only the node's radii
	}
	for (int i=0; i<numEntries; i++) {
		((MTentry *)(*this)[i].Ptr())->Key()->distance = dists[i];
		((MTentry *)(*newNode)[i].Ptr())->Key()->distance = dists[numEntries+i];
	}
	return newNode;
}

GiSTnode *MXTnode::PickSplit(int cands[], double *dists, vector<vector<int>>& vec)
{
	MTnode *rNode = Promote(cands, dists);

	int lDel = vec[1].size(), rDel = vec[0].size();  
	int *lArray = new int[lDel], *rArray = new int[rDel];  
	for (int i=0; i<lDel; i++) {
		lArray[i] = vec[1][i];
	}
	for (int i=0; i<rDel; i++) {
		rArray[i] = vec[0][i];
	}
	DeleteBulk(lArray, lDel);
	rNode->DeleteBulk(rArray, rDel);

	SortEntries();
	rNode->SortEntries();

	Condense();
	((MXTnode *)rNode)->Condense();

	delete []lArray;
	delete []rArray;
	return rNode;
}

GiSTlist<MTentry *> MXTnode::RangeSearch(const MTquery &query)
{
	GiSTlist<MTentry *> results;

	if (IsLeaf()) {
		vector<int> vec;
		Object& obj = ((Pred *)((SimpleQuery *)(&query))->Pred())->obj();
		double radius = ((SimpleQuery *)(&query))->Radius();
		vptree->RangeQuery(obj, radius, vec, query.Grade());
		for (int i=0; i<vec.size(); i++) {
			results.Append((MTentry *) (*this)[vec[i]].Ptr()->Copy());
		}

		for (int i=vptree->GetVPNum(); i<NumEntries(); i++) {
			MTentry *entry = (MTentry *) (*this)[i].Ptr()->Copy();
			SimpleQuery *newQuery = (SimpleQuery *) query.Copy();
			if (newQuery->Consistent(*entry)) {  // object qualifies
				entry->SetMaxRadius(newQuery->Grade());
				results.Append(entry);
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
				GiSTpath childPath = Path();
				childPath.MakeChild(entry->Ptr());
				MXTnode *childNode = (MXTnode *) ((MXTree *)Tree())->ReadNode(childPath);
				GiSTlist<MTentry *> childResults = childNode->RangeSearch(*newQuery);  // recurse the search
				while (!childResults.IsEmpty()) {
					results.Append(childResults.RemoveFront());
				}
				delete childNode;
			}
			delete newQuery;
		}
	}

	return results;
}