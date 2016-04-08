#pragma once
#include <vector>
using namespace std;
#include "MTnode.h"

struct MXTnodeHeader {
	int level;
	int numEntries;
};

class MXTnode : public MTnode
{
public:
	MXTnode(int _pageNum=1): MTnode(), pageNum(_pageNum) {}
	MXTnode(const MXTnode& node);
	GiSTobjid IsA() const { return MXTNODE_CLASS; }
	GiSTobject *Copy() const { return new MXTnode(*this); }

	int Size() const;
	int IsOverFull(const GiSTstore &store) const;
	GiSTcompressedEntry Entry(int entryPos) const;

	void Pack(char *buffer) const;
	void Unpack(const char *buffer);

#ifdef PRINTING_OBJECTS
	void Print(ostream& os) const;
#endif

	int Condense();  // after split, compress the super node
	void TestPromotion(double radii[], double *dist, int pageNums[], int cands[], double *dists, vector<vector<int>>& vec);
	MTnode *Promote(int cands[], double *dists);
	GiSTnode *PickSplit(int cands[], double *dists, vector<vector<int>>& vec);

	int GetPageNum() const { return pageNum; }
	void SetPageNum(int _pageNum) { pageNum = _pageNum; }

private:
	int pageNum;
};
