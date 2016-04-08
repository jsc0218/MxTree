#include <string>
using namespace std;
#include "BitMap.h"
#include <io.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <math.h>
#include <assert.h>

extern string BitMapPath;
const int BITS_PER_CHAR = 8;

BitMap::BitMap()
{
	fileHandle = open(BitMapPath.c_str(), O_BINARY|O_RDWR|O_CREAT, S_IREAD|S_IWRITE);
	assert(fileHandle != -1);
	len = filelength(fileHandle);
	map = new char[len];
	memset(map, 0, len);
	lseek(fileHandle, 0, SEEK_SET);
	read(fileHandle, map, len);
}

BitMap::~BitMap()
{
	lseek(fileHandle, 0, SEEK_SET);
	write(fileHandle, map, len);
	delete[] map;
	close(fileHandle);
}

void BitMap::Mark(long which, long num)
{
	for (long i=0; i<num; i++) {
		map[(which+i)/BITS_PER_CHAR] |= 1<<(BITS_PER_CHAR-(which+i)%BITS_PER_CHAR-1);
	}
}

void BitMap::Clear(long which, long num)
{
	for (long i=0; i<num; i++) {
		map[(which+i)/BITS_PER_CHAR] &= ~(1<<(BITS_PER_CHAR-(which+i)%BITS_PER_CHAR-1));
	}
}

bool BitMap::IsEmpty(long which, long num)
{
	for (long i=0; i<num; i++) {
		if (map[(which+i)/BITS_PER_CHAR] & (1<<(BITS_PER_CHAR-(which+i)%BITS_PER_CHAR-1))) {
			return false;
		}
	}
	return true;
}

long BitMap::LastMarked()
{
	for (long i=len*BITS_PER_CHAR-1; i>=0; i--) {
		if (!IsEmpty(i)) {
			return i;
		}
	}
	return -1;
}

long BitMap::Allocate(long num)
{
	for (long i=0; i<=(len*BITS_PER_CHAR-num); i++) {
		if (IsEmpty(i, num)) {
			Mark(i, num);
			return i;
		}
	}  

	char *temp = map;
	long oldLen = len;
	long last = LastMarked();
	len = ceil(((float)(last+1+num))/((float)BITS_PER_CHAR));
	map = new char[len];
	memset(map, 0, len);
	memcpy(map, temp, oldLen);
	delete[] temp;

	Mark(last+1, num);
	return last+1;
}

void BitMap::Deallocate(long which, long num)
{
	Clear(which, num);
}