/*
 * Portability shim for the storage layer.
 *
 * The tree sources were written against the MSVC C runtime and call it
 * directly: <io.h>, filelength(), _lseeki64(), S_IREAD/S_IWRITE and O_BINARY.
 * None of those are POSIX. Mapping them here keeps the algorithm sources
 * byte-for-byte close to the original Bologna/Berkeley code instead of
 * scattering #ifdefs through every read and write.
 */

#ifndef MXTREE_PLATFORM_H
#define MXTREE_PLATFORM_H

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32

#include <io.h>

#else /* POSIX */

#include <unistd.h>

/*
 * MSVC distinguishes text and binary streams and needs to be told which it
 * wants; POSIX has only one kind, so the flag is a no-op.
 */
#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
 * off_t is 64 bit here because the build defines _FILE_OFFSET_BITS=64, so
 * plain lseek already has the range the _lseeki64 call sites were reaching
 * for. Index files above 2 GB depend on this.
 */
#define _lseeki64 lseek

/* Pre-POSIX permission-bit spellings, still used by the open() call sites. */
#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif

/* Restores the file offset so callers can treat this as a pure query. */
inline long filelength(int fileHandle)
{
	off_t saved = lseek(fileHandle, 0, SEEK_CUR);
	off_t size = lseek(fileHandle, 0, SEEK_END);
	lseek(fileHandle, saved, SEEK_SET);
	return (long) size;
}

#endif /* _WIN32 */

#endif /* MXTREE_PLATFORM_H */
