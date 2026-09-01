/*
 * The one call the storage layer needs that POSIX does not provide.
 *
 * The sources were written against the MSVC C runtime, whose filelength()
 * has no POSIX equivalent. Everything else they used (<io.h>, _lseeki64,
 * S_IREAD/S_IWRITE, O_BINARY) has been replaced by its POSIX spelling at the
 * call sites.
 */

#ifndef MXTREE_PLATFORM_H
#define MXTREE_PLATFORM_H

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* Restores the file offset so callers can treat this as a pure query. */
inline long filelength(int fileHandle)
{
	off_t saved = lseek(fileHandle, 0, SEEK_CUR);
	off_t size = lseek(fileHandle, 0, SEEK_END);
	lseek(fileHandle, saved, SEEK_SET);
	return (long) size;
}

#endif /* MXTREE_PLATFORM_H */
