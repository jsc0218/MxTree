#ifndef MTCURSOR_H
#define MTCURSOR_H

#ifndef MAXDOUBLE
#ifdef _WIN32  // for MAXDOUBLE
#include <float.h>
#define MAXDOUBLE DBL_MAX
#else
#include <values.h>
#endif
#endif
#include "GiST.h"
#include "MTlist.h"

class MT;
class MTentry;
class MTpred;

class MTrecord {  // this class is used in the KNN-search algorithm for the priority queue and ordered access for tree
public:
	MTrecord (): bound (0), distance (MAXDOUBLE) {}
	MTrecord (GiSTpath path, double bound, double dist): path (path), bound (bound), distance (dist) {}

	double Bound () const { return bound; }
	double Distance () const { return distance; }
	GiSTpath Path () const { return path; }

private:
	GiSTpath path;  // current node path
	double bound;  // distance between node area and pred
	double distance;  // distance between parent entry and pred
};

int comparedst (MTrecord *a, MTrecord *b);

// the sorted access for the M-tree index structure according to the distance from pred
class MTcursor {
public:
	MTcursor (const MT& tree, const MTpred& pred);
	~MTcursor ();
	MTentry *Next ();
	void FetchNode ();  // originally it is private, due to it is friend function

private:
	BOOL IsReady () const;

	const MT& tree;
	MTpred *pred;
	MTorderedlist<MTrecord *> queue;  // the queue of active nodes
	MTorderedlist<MTentry *> results;  // the queue of partial result;
};

#endif
