#include "MT.h"
#include "MTpredicate.h"

int 
SimpleQuery::Consistent (const GiSTentry& entry)
{
	assert (entry.IsA() == MTENTRY_CLASS);
	if (grade==0 || fabs(grade - ((MTentry &)entry).Key()->distance) <= radius + ((MTentry &)entry).MaxRadius()) {  // prune for reference point
		grade = pred->distance(((MTentry &)entry).object());
		return grade <= radius+((MTentry &)entry).MaxRadius();
	} else {
		return FALSE;
	}
}

int
SimpleQuery::NonConsistent (const GiSTentry& entry)
{
	return FALSE;
}
