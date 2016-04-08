/*********************************************************************
*                                                                    *
* Copyright (c) 1997,1998, 1999                                      *
* Multimedia DB Group and DEIS - CSITE-CNR,                          *
* University of Bologna, Bologna, ITALY.                             *
*                                                                    *
* All Rights Reserved.                                               *
*                                                                    *
* Permission to use, copy, and distribute this software and its      *
* documentation for NON-COMMERCIAL purposes and without fee is       *
* hereby granted provided  that this copyright notice appears in     *
* all copies.                                                        *
*                                                                    *
* THE AUTHORS MAKE NO REPRESENTATIONS OR WARRANTIES ABOUT THE        *
* SUITABILITY OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING  *
* BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY,      *
* FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT. THE AUTHOR  *
* SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A      *
* RESULT OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS    *
* DERIVATIVES.                                                       *
*                                                                    *
*********************************************************************/

#ifndef MTPREDICATE_H
#define MTPREDICATE_H

#include "MTobject.h"

class MTpred: public GiSTobject {  // the base class for predicates
public:
	virtual GiSTobjid IsA () { return MTPREDICATE_CLASS; }
	virtual double distance (const Object &obj) const=0;
	virtual ~MTpred () {}

#ifdef PRINTING_OBJECTS
	virtual void Print (ostream& os) const=0;
#endif
};

class Pred: public MTpred {  // a simple predicate
public:
	// constructors, destructors, etc.
	Pred (const Object &obj): object (obj) {}
	Pred (const Pred& p): object (p.object) {}
	GiSTobject *Copy () const { return new Pred (*this); }

	// virtual methods
	GiSTobjid IsA () { return MTPREDICATE_CLASS; }
	double distance (const Object &obj) const { return object.distance(obj); }

	// access to private methods
	Object obj () const { return object; }

#ifdef PRINTING_OBJECTS
	void Print (ostream& os) const { os << object; }
#endif

private:
	Object object;
};

// for the queries, the conjunction corresponds to intersection, the disjunction to union, and the negation to difference

class MTquery: public GiSTobject {  // the base class for range queries
public:
	// constructors, destructors, etc.
	MTquery (): grade (0), isOpen (FALSE) {}
	MTquery (const double g, const BOOL o=FALSE): grade (g), isOpen (o) {}
	virtual ~MTquery () {}

	// pure virtual methods
	virtual int Consistent (const GiSTentry& entry)=0;
	virtual int NonConsistent (const GiSTentry& entry)=0;

	// access to private members
	double Grade () const { return grade; }
	void SetGrade (double p_grade) { grade = p_grade; }

protected:
	double grade;  // similarity score computed for the currently examined MTentry
	BOOL isOpen;  // indicates if the query is open
};

class SimpleQuery: public MTquery {  // a simple query
public:
	// constructors, destructors, etc.
	SimpleQuery (const MTpred *p, const double r, const BOOL o=FALSE): MTquery (0,o), pred ((MTpred *)p->Copy()), radius (r) {}
	SimpleQuery (const SimpleQuery& q): MTquery (q.grade, q.isOpen), pred ((MTpred *)q.pred->Copy()), radius (q.radius) {}
	GiSTobject *Copy () const { return new SimpleQuery (*this); }
	~SimpleQuery () {
		if (pred) {
			delete pred;
		}
	}

	// basic consistency methods
	int Consistent (const GiSTentry& entry);
	int NonConsistent (const GiSTentry& entry);

	// access to private members
	double Radius () const { return radius; }
	void SetRadius (double p_radius) { radius = p_radius; }

private:
	MTpred *pred;
	double radius;
};

class TopQuery: public GiSTobject {  // a simple k-NN query
public:
	// constructors, destructors, etc.
	TopQuery (const MTpred *p, const int n):  k(n), pred ((MTpred *)p->Copy()) {}
	TopQuery (const TopQuery& q): k(q.k), pred ((MTpred *)q.pred->Copy()) {}
	GiSTobject *Copy () const { return new TopQuery (*this); }
	~TopQuery () {
		if (pred) {
			delete pred;
		}
	}

	// access to private members
	const MTpred *Pred () const { return pred; }

	int k;  // which is indicated manually

private:
	MTpred *pred;
};

#endif
