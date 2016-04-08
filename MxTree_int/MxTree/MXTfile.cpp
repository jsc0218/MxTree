#include "MXTfile.h"
#include <fcntl.h>
#include <io.h>
#include <sys\stat.h>

extern int IOread, IOwrite;

void MXTfile::Create(const char *filename)
{
	if (IsOpen()) {
		return;
	}
	fileHandle = open(filename, O_RDWR|O_BINARY);
	if (fileHandle >= 0) {
		close(fileHandle);
		return;
	}
	fileHandle = open(filename, O_BINARY|O_RDWR|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE);
	if (fileHandle < 0) {
		return;
	}
	SetOpen(1);
}

void MXTfile::Open(const char *filename)
{
	if (IsOpen()) {
		return;
	}
	fileHandle = open(filename, O_RDWR|O_BINARY);
	if (fileHandle < 0) {
		return;
	}
	SetOpen(1);
}

void MXTfile::Close()
{
	if (!IsOpen()) {
		return;
	}
	close(fileHandle);
	SetOpen(0);
}

void MXTfile::Read(GiSTpage page, char *buf)
{
	if (IsOpen()) {
		lseek(fileHandle, page*PageSize(), SEEK_SET);
		read(fileHandle, buf, PageSize());
		IOread++;
	}
}

void MXTfile::Write(GiSTpage page, const char *buf)
{
	if (IsOpen()) {
		lseek(fileHandle, page*PageSize(), SEEK_SET);
		write(fileHandle, buf, PageSize());
		IOwrite++;
	}
}

GiSTpage MXTfile::Allocate()
{
	assert(IsOpen());

	GiSTpage page = bitMap->Allocate();
	char *buf = new char[PageSize()];
	memset(buf, 0, PageSize());
	lseek(fileHandle, page*PageSize(), SEEK_SET);
	write(fileHandle, buf, PageSize());
	delete[] buf;
	return page;
}

void MXTfile::Deallocate(GiSTpage page)
{
	assert(IsOpen());

	bitMap->Deallocate(page);
}

void MXTfile::Read(GiSTpage page, char *buf, int pageNum)
{
	assert(pageNum >= 1);
	if (IsOpen()) {
		lseek(fileHandle, page*PageSize(), SEEK_SET);
		read(fileHandle, buf, pageNum*PageSize());
		IOread += pageNum;
	}
}

void MXTfile::Write(GiSTpage page, const char *buf, int pageNum)
{
	assert(pageNum >= 1);
	if (IsOpen()) {
		lseek(fileHandle, page*PageSize(), SEEK_SET);
		write(fileHandle, buf, pageNum*PageSize());
		IOwrite += pageNum;
	}
}

GiSTpage MXTfile::Allocate(int pageNum)
{
	assert(IsOpen());
	assert(pageNum >= 1);

	GiSTpage page = bitMap->Allocate(pageNum);
	char *buf = new char[PageSize()];
	memset(buf, 0, PageSize());
	lseek(fileHandle, page*PageSize(), SEEK_SET);
	for (int i=0; i<pageNum; i++) {
		write(fileHandle, buf, PageSize());
	}
	delete[] buf;
	return page;
}

void MXTfile::Deallocate(GiSTpage page, int pageNum)
{
	assert(IsOpen());
	assert(pageNum >= 1);

	bitMap->Deallocate(page, pageNum);
}
