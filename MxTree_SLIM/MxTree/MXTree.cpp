#include "MXTree.h"

extern int MAX_ENTRY_NUM;
extern double AccessTime;
extern double TimePerPage;

MXTree::~MXTree()
{
	char *buf = new char[store->PageSize()];
	memset(buf, 0, store->PageSize());
	memcpy(buf, &rootPage, sizeof(int));
	store->Write(GiSTRootPage, buf);
	delete[] buf;
}

void MXTree::Create(const char *filename)
{
	if (IsOpen()) {
		return;
	}
	store = CreateStore();
	store->Create(filename);
	if (!store->IsOpen()) { 
		return;
	}
	store->Allocate();  // reserved for root pointer
	rootPage = store->Allocate();
	assert(rootPage == 1);
	GiSTnode *node = NewNode(this);
	node->Path().MakeRoot();
	WriteNode(node);
	delete node;
	isOpen = 1;
}

void MXTree::Open(const char *filename)
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

	char *buf = new char[store->PageSize()];
	memset(buf, 0, store->PageSize());
	store->Read(GiSTRootPage, buf);
	memcpy(&rootPage, buf, sizeof(int));
	delete[] buf;
}

GiSTnode* MXTree::ReadNode(const GiSTpath& path) const
{
	char *firstBuffer = new char[store->PageSize()];
	int startPage = (path.IsRoot() ? rootPage : path.Page());
	store->Read(startPage, firstBuffer);
	int pageNum = ceil((float)(((MXTnodeHeader *)firstBuffer)->numEntries*(EntrySize()+sizeof(GiSTpage))+sizeof(MXTnodeHeader))/(float)PAGE_SIZE);
	GiSTnode *node = new MXTnode(pageNum);
	node->SetTree((GiST *)this);
	node->Path() = path;
	if (pageNum > 1) {
		char *buffer = new char[store->PageSize()*pageNum];
		memset(buffer, 0, store->PageSize()*pageNum);
		memcpy(buffer, firstBuffer, store->PageSize());
		((MXTfile *)store)->Read(startPage+1, buffer+store->PageSize(), pageNum-1);  // do the deed
		node->Unpack(buffer);
		delete[] buffer;
	} else {
		node->Unpack(firstBuffer);
	}
	delete[] firstBuffer;

	return node;
}

void MXTree::WriteNode(GiSTnode *node)
{
	int pageNum = ((MXTnode *)node)->GetPageNum();
	char *buffer = new char[store->PageSize()*pageNum];
	memset(buffer, 0, store->PageSize()*pageNum);
	node->Pack(buffer);
	int startPage = (node->Path().IsRoot() ? rootPage : node->Path().Page());
	((MXTfile *)store)->Write(startPage, buffer, pageNum);  // do the deed
	delete[] buffer;
}

void MXTree::Split(GiSTnode **node, const GiSTentry& entry)
{
	double radii[2], dist, *dists = new double[(*node)->NumEntries()*2];
	int pageNums[2], cands[2];
	vector<vector<int>> vec(2);
	((MXTnode *)(*node))->TestPromotion(radii, &dist, pageNums, cands, dists, vec);
	if (Trade((*node)->Path().IsRoot(), radii, dist, pageNums, ((MXTnode *)(*node))->GetPageNum()+1, (*node)->NumEntries())) {
		// don't split now
		delete[] dists;
		GiSTpath oldPath = (*node)->Path();

		int startPage = ((*node)->Path().IsRoot() ? rootPage : (*node)->Path().Page());
		int pageNum = ((MXTnode *)(*node))->GetPageNum();
		((MXTfile *)store)->Deallocate(startPage, pageNum);
		startPage = ((MXTfile *)store)->Allocate(++pageNum);
		(*node)->Path().MakeSibling(startPage);
		rootPage = ((*node)->Path().IsRoot() ? startPage : rootPage);
		((MXTnode *)(*node))->SetPageNum(pageNum);
		WriteNode(*node);

		if (!(*node)->Path().IsRoot() && startPage != oldPath.Page()) {
			GiSTpath parentPath = oldPath;
			parentPath.MakeParent();
			GiSTnode *parentNode = ReadNode(parentPath);
			GiSTentry *e = parentNode->SearchPtr(oldPath.Page());
			assert(e != NULL);
			int pos = e->Position();
			e->SetPtr(startPage);
			parentNode->DeleteEntry(pos);
			parentNode->InsertBefore(*e, pos);
			WriteNode(parentNode);
			delete parentNode;
			delete e;
		}
	} else {
		// split now
		bool bLeft = false, bNewRoot = false;

		if ((*node)->Path().IsRoot()) {
			bNewRoot = true;
			(*node)->Path().MakeChild(rootPage);
			rootPage = store->Allocate();
		}

		int oldPageNum = ((MXTnode *)(*node))->GetPageNum();
		GiSTnode *node2 = ((MXTnode *)(*node))->PickSplit(cands, dists, vec);
		delete[] dists;
		int curPageNum = ((MXTnode *)(*node))->GetPageNum();
		assert(oldPageNum >= curPageNum);
		if (oldPageNum > curPageNum) {
			((MXTfile *)store)->Deallocate((*node)->Path().Page()+curPageNum, oldPageNum-curPageNum);
		}
		node2->Path().MakeSibling(((MXTfile *)store)->Allocate(((MXTnode *)node2)->GetPageNum()));

		WriteNode(*node);
		WriteNode(node2);
	
		GiSTentry *e = (*node)->SearchPtr(entry.Ptr());
		if (e != NULL) {
			bLeft = true;
			delete e;
		}
	
		GiSTentry *e1 = (*node)->Union();
		GiSTentry *e2 = node2->Union();
	
		e1->SetPtr((*node)->Path().Page());
		e2->SetPtr(node2->Path().Page());
		// Create new root if root is being split
		if (bNewRoot) {
			GiSTnode *root = NewNode(this);
			root->SetLevel((*node)->Level() + 1);
			root->InsertBefore(*e1, 0);
			root->InsertBefore(*e2, 1);
			root->Path().MakeRoot();
			WriteNode(root);
			delete root;
		} else {
			// Insert entry for N' in parent
			GiSTpath parentPath = (*node)->Path();
			parentPath.MakeParent();
			GiSTnode *parent = ReadNode(parentPath);
			// Find the entry for N in parent
			GiSTentry *e = parent->SearchPtr((*node)->Path().Page());
			assert(e != NULL);
			// Insert the new entry right after it
			int pos = e->Position();
			parent->DeleteEntry(pos);
			parent->InsertBefore(*e1, pos);
			parent->InsertBefore(*e2, pos+1);
			delete e;
			if (!parent->IsOverFull(*store)) {
				WriteNode(parent);
			} else {
				Split(&parent, bLeft? *e1: *e2);  // parent is the node which contains the entry inserted
				GiSTpage page = (*node)->Path().Page();
				(*node)->Path() = parent->Path();  // parent's path may change
				(*node)->Path().MakeChild(page);
				page = node2->Path().Page();
				node2->Path() = (*node)->Path();
				node2->Path().MakeSibling(page);
			}
			delete parent;
		}
		if (!bLeft) {
			delete *node;
			*node = node2;  // return it
		} else {
			delete node2;
		}
		delete e1;
		delete e2;
	}
}

bool MXTree::Trade(bool isRoot, double radii[], double dist, int pageNums[], int pageNum, int entryNum)
{
	if (entryNum > MAX_ENTRY_NUM) {
		return false;
	}

	double overlap = (dist+MIN(radii[0],radii[1]) >= MAX(radii[0],radii[1])) ? radii[0]+radii[1]-dist : 2*MIN(radii[0],radii[1]);
	double pow1 = pow(radii[0], 1);
	double pow2 = pow(radii[1], 1);
	double pow3 = overlap>0 ? pow(overlap/2, 1) : 0;

	double p1 = pow1/(pow1+pow2-pow3);
	double p2 = pow2/(pow1+pow2-pow3);

	if (isRoot) {
		return TimePerPage*(1+pageNums[0]*p1+pageNums[1]*p2-pageNum)>0 ? true : false;
	} else {
		return AccessTime*(p1+p2-1)+TimePerPage*(pageNums[0]*p1+pageNums[1]*p2-pageNum)>0 ? true : false;
	}
}