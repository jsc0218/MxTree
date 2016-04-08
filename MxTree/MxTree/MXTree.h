#pragma once
#include "MT.h"
#include "MXTnode.h"
#include "MXTfile.h"

class MXTree: public MT
{
public:
	MXTree(): MT() { rootPage = 0; }
	virtual ~MXTree();
	GiSTobjid IsA() { return MXTREE_CLASS; }

	void Create(const char *filename);
	void Open(const char *filename);

	GiSTnode *CreateNode() const { return new MXTnode; }
	GiSTstore *CreateStore() const { return new MXTfile; }

	GiSTnode *ReadNode(const GiSTpath& path) const;
	void WriteNode(GiSTnode *node);

	GiSTlist<MTentry *> RangeSearch(const MTquery& query);

	void Split(GiSTnode **node, const GiSTentry& entry);
	bool Trade(bool isRoot, double radii[], double dist, int pageNums[], int pageNum, int entryNum);

	friend GiSTlist<MTentry *> MXTnode::RangeSearch(const MTquery& query);

private:
	int rootPage; 
};
