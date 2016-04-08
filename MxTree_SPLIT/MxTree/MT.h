#ifndef MT_H
#define MT_H

#include "MTfile.h"
#include "MTnode.h"
#include "MTcursor.h"
#ifdef _WIN32  // for MAXDOUBLE
#include <float.h>
#include <limits.h>
#define MAXDOUBLE DBL_MAX
#else  // under UNIX these constants are defined in values.h
#include <values.h>
#endif

typedef enum {
    RANDOM = 0,
    CONFIRMED = 1,
    SAMPLING = 2,
    mM_RAD = 3
} pp_function;  // promotion functions

typedef enum {
    RANDOMV = 0,
    SAMPLINGV = 1,
    MAX_LB_DISTV = 2,
    mM_RADV = 3
} pv_function;  // confirmed promotion functions

class TopQuery;

class MT: public GiST {
public:
	MT(): GiST() {}  // added by myself
    GiSTobjid IsA () { return MT_CLASS; }

    int IsOrdered () const { return 1; }  // the children of each node are ordered on increasing distances from the parent
    int TreeHeight () const;  // height of the M-tree
    MTnode *ParentNode (MTnode *node);  // return the parent of node (the caller should delete the object)
    void CollectStats ();  // level-stats collecting function: total number of pages, average radius and average covered volume (if applicable)
    BOOL CheckNode (GiSTpath path, MTentry *parentEntry);  // debug function: check for node consistency
    GiSTlist<MTentry *> RangeSearch (const MTquery& query);  // range search, actually in the class node function
    MTentry **TopSearch (const TopQuery& query);  // generic search algorithm: k-NN search for a TopQuery

    // friend methods and functions
    friend void MTnode::InvalidateEntry (BOOL isNew);
    friend GiSTlist<MTentry *> MTnode::RangeSearch (const MTquery& query);
    friend void MTcursor::FetchNode ();

protected:
    // Required members
    GiSTnode *CreateNode () const { return new MTnode; }
    GiSTstore *CreateStore () const { return new MTfile; }
};

int PickRandom (int from, int to);  // pick a determined number of samples in the range [from, to)

#endif
