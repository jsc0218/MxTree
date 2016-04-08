// -*- Mode: C++ -*-

//          GiSTfile.cpp
//
// Copyright (c) 1996, Regents of the University of California
// $Header: /usr/local/devel/GiST/libGiST/libGiST/GiSTfile.cpp,v 1.1.1.1 1996/08/06 23:47:21 jmh Exp $

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#ifdef UNIX
#include <unistd.h>
#else
#include <io.h>
#endif

#ifdef UNIX
#define O_BINARY 0
#endif

#include "GiSTfile.h"

// The first page in the file has these "magic words"
// and the head of the deleted page list.
static char magic[] = "GiST data file";

void 
GiSTfile::Create (const char *filename)
{
	if (IsOpen()) {
		return;
	}
	fileHandle = open (filename, O_RDWR|O_BINARY);  // expect to -1
	if (fileHandle >= 0) {  // already have one
		close (fileHandle);
		return;
	}
	fileHandle = open (filename, O_BINARY|O_RDWR|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE);
	if (fileHandle < 0) {  // error
		return;
	}
	SetOpen (1);

	/* Reserve page 0 */
	char *page = new char[PageSize()];

	memset (page, 0, PageSize());
	memcpy (page, magic, sizeof(magic));
	write (fileHandle, page, PageSize());  // write the first page to disk(file)
	delete [] page;
}

void 
GiSTfile::Open (const char *filename)
{
	if (IsOpen()) {
		return;
	}
	fileHandle = open(filename, O_RDWR|O_BINARY);
	if (fileHandle < 0) {  // error
		return;
	}
	// Verify that the magic words are there
	char *page = new char[PageSize()];
	read (fileHandle, page, PageSize());
	if (memcmp (page, magic, sizeof(magic))) {  // not there
		close (fileHandle);
		delete [] page;
		return;
	}
	delete [] page;
	SetOpen (1);
}

void 
GiSTfile::Close ()
{
	if (!IsOpen()) {  // not open
		return;
	}
	close (fileHandle);
	SetOpen (0);
}

void
GiSTfile::Read (GiSTpage page, char *buf)
{
	if (IsOpen()) {
		lseek (fileHandle, page*PageSize(), SEEK_SET);
		read (fileHandle, buf, PageSize());
	}
}

void
GiSTfile::Write (GiSTpage page, const char *buf)
{
	if (IsOpen()) {
		lseek (fileHandle, page*PageSize(), SEEK_SET);
		write (fileHandle, buf, PageSize());
	}
}

GiSTpage 
GiSTfile::Allocate ()
{
	if (!IsOpen()) {  // closed
		return (0);
	}
	// See if there's a deleted page
	char *buf = new char[PageSize()];
	Read (0, buf);
	GiSTpage page;
	memcpy (&page, buf+sizeof(magic), sizeof(GiSTpage));
	if (page) {  // have
		// Reclaim this page
		Read (page, buf);  // read the deleted page
		Write (0, buf);  // write it to the first page
	} else {  // don't have
		memset (buf, 0, PageSize());
		page = lseek (fileHandle, 0, SEEK_END) / PageSize ();
		write (fileHandle, buf, PageSize());
	}
	delete [] buf;
	return page;  // page number
}

void 
GiSTfile::Deallocate (GiSTpage page)
{
	if (!IsOpen()) {
		return;
	}
	// Get the old head of the list
	char *buf = new char[PageSize()];
	Read (0, buf);
	GiSTpage temp;
	memcpy (&temp, buf+sizeof(magic), sizeof(GiSTpage));
	// Write the new head of the list
	memcpy (buf+sizeof(magic), &page, sizeof(GiSTpage));
	Write (0, buf);
	// In our new head, put link to old head
	memcpy (buf+sizeof(magic), &temp, sizeof(GiSTpage));
	Write (page, buf);
	delete [] buf;
}
