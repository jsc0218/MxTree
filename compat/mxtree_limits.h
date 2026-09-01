/*
 * MAXDOUBLE and MAXINT for the tree sources.
 *
 * The sources use the old BSD spellings, which came from <values.h>. That
 * header declares itself obsolete and points at <float.h> and <limits.h>; it
 * is nothing but a set of aliases for them. Defining the two aliases this code
 * actually uses keeps the algorithm sources reading like the original listings
 * without depending on an obsolete header to supply them.
 */

#ifndef MXTREE_LIMITS_H
#define MXTREE_LIMITS_H

#include <float.h>
#include <limits.h>

#ifndef MAXDOUBLE
#define MAXDOUBLE DBL_MAX
#endif

#ifndef MAXINT
#define MAXINT INT_MAX
#endif

#endif /* MXTREE_LIMITS_H */
