#include "MTentry.h"
#include "MT.h"

int EntrySize ()
{
	if (ObjectSize()) {
		return (ObjectSize() + 3*sizeof(double) + sizeof(BOOL));  // compressedEntry is: Object, rmin, rmax, distance, splitted
	}
	return 0;
}

GiSTpenalty *
MTentry::Penalty (const GiSTentry &newEntry) const
{
	assert (newEntry.IsA() == MTENTRY_CLASS);
	const MTentry& entry= (const MTentry &) newEntry;
	double dist = object().distance (entry.object()) + entry.MaxRadius();
	double retval = dist - MaxRadius();
	//if (retval < 0) {  // indicates that entry is contained
	//	retval = -MaxDist() + dist;
	//}
	// if the entry can fit in (more than) a node without enlarging its radius, we assign it to its nearest node
	return new MTpenalty (retval, dist);
}

GiSTpenalty * 
MTentry::Penalty (const GiSTentry &newEntry, MTpenalty *minPenalty) const
// in this case we can avoid to compute some distances by using some stored information:
// minPenalty (the minimum penalty achieved so far),
// newEntry.key->distance (the distance of the entry from the parent of this entry, stored in the SearchMinPenalty method) and maxradius.
{
	assert (newEntry.IsA() == MTENTRY_CLASS);

	if (((MTkey *) newEntry.Key())->distance > 0 && minPenalty) {  // in this case is possible to prune some entries using triangular inequality
		double distPen = fabs(((MTkey *) newEntry.Key())->distance - Key()->distance) - MaxRadius();
		MTpenalty *tmpPen = NULL;
		if (distPen >= 0) {
			tmpPen = new MTpenalty (distPen, 0);
		} else {
			tmpPen = new MTpenalty (distPen + MaxRadius() - MaxDist(), 0);
		}
		if (!((*tmpPen)<(*minPenalty))) {  // larger than or equal to minPenalty
			delete tmpPen;
			return new MTpenalty (MAXDOUBLE, 0);  // avoid to compute this distance
		}
		delete tmpPen;
	}

	return Penalty (newEntry);
}

GiSTcompressedEntry
MTentry::Compress () const
{
	double rmin = ((MTkey *)key)->MinRadius(), rmax = ((MTkey *)key)->MaxRadius(), dist = ((MTkey *)key)->distance;
	BOOL splitted = ((MTkey *)key)->splitted;

	GiSTcompressedEntry compressedEntry;
	compressedEntry.keyLen = CompressedLength ();
	compressedEntry.ptr = ptr;
	compressedEntry.key = new char[compressedEntry.keyLen];
	((MTkey *)key)->object().Compress(compressedEntry.key);  // compress the object

	int objSize = ((MTkey *)key)->object().CompressedLength ();
	memcpy (compressedEntry.key+objSize, &rmin, sizeof(double));
	memcpy (compressedEntry.key+objSize+sizeof(double), &rmax, sizeof(double));
	memcpy (compressedEntry.key+objSize+2*sizeof(double), &dist, sizeof(double));
	memcpy (compressedEntry.key+objSize+3*sizeof(double), &splitted, sizeof(BOOL));

	return compressedEntry;
}

void
MTentry::Decompress (const GiSTcompressedEntry entry)
{
	BOOL splitted = FALSE;
	double rmin = 0, rmax = 0, dist = 0;
	Object obj (entry.key);  // decompress the object
	int sizeofObject = obj.CompressedLength ();
	memcpy (&rmin, entry.key+sizeofObject, sizeof(double));
	memcpy (&rmax, entry.key+sizeofObject+sizeof(double), sizeof(double));
	memcpy (&dist, entry.key+sizeofObject+2*sizeof(double), sizeof(double));
	memcpy (&splitted, entry.key+sizeofObject+3*sizeof(double), sizeof(BOOL));

	key = new MTkey (obj, rmin, rmax, dist, splitted);
	ptr = entry.ptr;
}
