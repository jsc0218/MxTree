#pragma once

class BitMap
{
public:
	BitMap();
	virtual ~BitMap();
	
	long Allocate(long num=1);
	void Deallocate(long which, long num=1);

private:
	void Mark(long which, long num=1);   	
	void Clear(long which, long num=1);
	bool IsEmpty(long which, long num=1);
	long LastMarked(); 

	char *map;
	long len;
	int fileHandle;
};
