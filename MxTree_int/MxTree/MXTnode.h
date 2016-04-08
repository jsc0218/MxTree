#pragma once

#include <vector>
using namespace std;
#include "MTnode.h"

struct MXTnodeHeader {
	int level;
	int numEntries;
};

class VPTree;
class MXTnode : public MTnode
{
public:
	MXTnode(int _pageNum=1): MTnode(), pageNum(_pageNum), vptree(NULL) {}
	MXTnode(const MXTnode& node);
	virtual ~MXTnode();
	GiSTobjid IsA() const { return MXTNODE_CLASS; }
	GiSTobject *Copy() const { return new MXTnode(*this); }

	int Size() const;
	int IsOverFull(const GiSTstore &store) const;
	GiSTcompressedEntry Entry(int entryPos) const;

	void Pack(char *buffer);
	void Unpack(const char *buffer);

#ifdef PRINTING_OBJECTS
	void Print(ostream& os) const;
#endif

	int Condense();  // after split, compress the super node
	void TestPromotion(double radii[], double *dist, int pageNums[], int cands[], double *dists, vector<vector<int>>& vec);
	MTnode *Promote(int cands[], double *dists);
	GiSTnode *PickSplit(int cands[], double *dists, vector<vector<int>>& vec);

	GiSTlist<MTentry *> RangeSearch (const MTquery& query);

	int GetPageNum() const { return pageNum; }
	void SetPageNum(int _pageNum) { pageNum = _pageNum; }

	Object& GetObject(int i) const { return ((MTentry *)((GiSTentry *)(*this)[i]))->object(); }

private:
	int pageNum;
	VPTree *vptree;
};
