#ifndef MTENTRY_H
#define MTENTRY_H

#include <string.h>
#include <stdio.h>
#ifndef MAXDOUBLE
#ifdef _WIN32  // for MAXDOUBLE
#include <float.h>
#define MAXDOUBLE DBL_MAX
#else
#include <values.h>
#endif
#endif
#include "GiST.h"
#include "MTobject.h"

class MTentry;

// MTkey is a covering region indicated by an object and its covering radii: obj, rmin, rmax
// We also throw in some standard geo operators to support various query predicates, etc.

class MTkey: public GiSTobject {
public:
	// constructors, destructors, assignment, etc.
	MTkey (): distance (-MaxDist()), splitted (FALSE), rmin (0), rmax (MAXDOUBLE) { }
	MTkey (const MTkey& k): distance (k.distance), splitted (k.splitted), obj (k.obj), rmin (k.rmin), rmax (k.rmax) { }
	MTkey (Object& p_obj, const double p_rmin, const double p_rmax, const double p_distance=-MaxDist(), const BOOL p_splitted=FALSE):
		distance (p_distance), splitted (p_splitted), obj (p_obj), rmin (p_rmin), rmax (p_rmax) { }
	GiSTobject *Copy () const { return new MTkey (*this); }
	~MTkey () { }
	GiSTobjid IsA () { return MTKEY_CLASS; }

	MTkey& operator= (const MTkey& k) {
		distance = k.distance;
		splitted = k.splitted;
		obj = k.obj;
		rmin = k.rmin;
		rmax = k.rmax;
		return *this;
	}
	// covering region property and comparison methods
	int operator== (const MTkey& k) {
		return (!splitted) && (obj==k.obj) && (rmin==k.rmin) && (rmax==k.rmax) && (distance==k.distance);
	}

	Object object () const { return obj; }
	double MinRadius () const { return rmin; }
	double MaxRadius () const { return rmax; }

	// I/O methods
#ifdef PRINTING_OBJECTS
	void Print (ostream& os) const {
		os << '"';
		os << "(" << obj << ", " << rmin << ", " << rmax << ")" << '"';
	}
#endif

	friend class MTentry;

	double distance;  // the distance between the entry and its parent, only for index performance, in order to minimize the number of distance computations;
	BOOL splitted;  // indicate that the child node has been splitted

private:
	Object obj;  // object
	double rmin, rmax;  // covering radii
};


#ifdef PRINTING_OBJECTS
inline ostream& operator<< (ostream& os, const MTkey& k) {
	k.Print (os);
	return os;
}
#endif


class MTpenalty: public GiSTpenalty {  // we have to create a sub-class in order to add the member value distance
public:
	MTpenalty (): GiSTpenalty (), distance (-MaxDist()) { }
	MTpenalty (const double v, const double d): GiSTpenalty (v), distance (d) { }

	double distance;  // distance to the parent object
};

int EntrySize ();

class MTentry: public GiSTentry {  // this are the entries stored in each node
public:
	// constructors, destructors, etc.
	MTentry () { }
	MTentry (const MTkey& k, const GiSTpage p) { key = new MTkey (k); ptr = p; }
	MTentry (const MTentry& e): GiSTentry (e) { key = new MTkey (*(MTkey *)(e.key)); }
	GiSTobjid IsA () const { return MTENTRY_CLASS; }
	GiSTobject *Copy () const { return new MTentry (*this); }
	void InitKey () { key = new MTkey; }

	int IsEqual (const GiSTobject& obj) const {
		if (obj.IsA() != MTENTRY_CLASS) {
			return 0;
		}
		return ( *((MTkey *)key) ) == ( *( (MTkey *)((const MTentry&)obj).key ) );
	}

	// cast key member to class MTkey *.  Prevents compiler warnings.
	MTkey *Key () const { return (MTkey *)key; }

	// basic GiST methods
	GiSTpenalty *Penalty (const GiSTentry &newEntry) const;
	GiSTpenalty *Penalty (const GiSTentry &newEntry, MTpenalty *minPenalty) const;  // redefined in order to optimize distance computations

	// Other methods we're required to supply, compressedEntry is: Object, rmin, rmax, distance, splitted
	int CompressedLength () const { return object().CompressedLength() + 3*sizeof(double) + sizeof(BOOL); }

	GiSTcompressedEntry Compress () const;
	void Decompress (const GiSTcompressedEntry entry);

	// Method for ordered domains, according to the distance
	int Compare (const GiSTentry& entry) const {
		assert (entry.IsA() == MTENTRY_CLASS);
		double d = Key()->distance - ((MTentry &)entry).Key()->distance;
		return d==0 ? 0 : (d>0 ? 1 : 0);
	}

	// I/O methods
#ifdef PRINTING_OBJECTS
	void Print (ostream& os) const {
		key->Print (os);
		os << "->" << Ptr() << " (" << Key()->distance << ")";
		if (Key()->splitted) {
			os << " *";
		}
		os << endl;
	}
#endif

	// access to private members
	Object& object () const { return Key()->obj; }
	double MinRadius () const { return Key()->rmin; }
	double MaxRadius () const { return Key()->rmax; }
	void SetObject (const Object& o) { Key()->obj = o; }
	void SetMinRadius (double d) { Key()->rmin = d; }
	void SetMaxRadius (double d) { Key()->rmax = d; }
};

#endif
