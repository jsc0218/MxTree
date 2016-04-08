#ifndef MTFILE_H
#define MTFILE_H

#include "GiSTstore.h"

//	MTfile is a simple storage class for GiSTs to work over 
//	UNIX/NT files. It is a copy of the GiSTfile class.

class MTfile: public GiSTstore {
public:
	MTfile (): GiSTstore () {}

	void Create (const char *filename);
	void Open (const char *filename);
	void Close ();

	void Read (GiSTpage page, char *buf);
	void Write (GiSTpage page, const char *buf);
	GiSTpage Allocate ();
	void Deallocate (GiSTpage page);
	void Sync () {}
	int PageSize () const { return 4096; }

private:
	int fileHandle;
};

#endif
