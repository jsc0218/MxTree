#pragma once

#include "GiSTstore.h"
#include "BitMap.h"

const int PAGE_SIZE = 4096;

class MXTfile: public GiSTstore 
{
public:
	MXTfile(): GiSTstore() { bitMap = new BitMap(); }
	virtual ~MXTfile() { delete bitMap; }

	void Create(const char *filename);
	void Open(const char *filename);
	void Close();
	void Read(GiSTpage page, char *buf);
	void Write(GiSTpage page, const char *buf);
	GiSTpage Allocate();
	void Deallocate(GiSTpage page);
	void Sync() {}
	int PageSize() const { return PAGE_SIZE; }

	void Read(GiSTpage page, char *buf, int pageNum);
	void Write(GiSTpage page, const char *buf, int pageNum);
	GiSTpage Allocate(int pageNum);
	void Deallocate(GiSTpage page, int pageNum);

private:
	int fileHandle;
	BitMap *bitMap;
};
