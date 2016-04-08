#include <string.h>
#include "GiSTpredicate.h"

int PtrPredicate::Consistent (const GiSTentry& entry) const
{
	return !entry.IsLeaf() || entry.Ptr()==page;
}

GiSTobject* PtrPredicate::Copy() const
{
	return new PtrPredicate(page);
}

#ifdef PRINTING_OBJECTS
void PtrPredicate::Print (ostream& os) const
{
	os << "ptr = " << page;
}
#endif
