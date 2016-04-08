#ifndef MTOBJECT_H
#define MTOBJECT_H

extern int compdists;  // used for performance test
extern int dimension;

#ifndef MIN
#define MIN(x, y) ((x<y)? (x): (y))
#define MAX(x, y) ((x>y)? (x): (y))
#endif

#include <istream>
using namespace std;
#include <string.h>
#include "GiSTdefs.h"

double MaxDist ();  // return the maximum value for the distance between two objects
int ObjectSize ();  // return the compressed size of each object (0 if objects have different sizes)
int EditDistance(const char* strA, int lenA, const char* strB, int lenB);

class Object : public GiSTobject  // the DB object class
{
public:
	Object () { x = new char[dimension]; }  // default constructor (needed)
	Object (const Object& obj) {  // copy constructor (needed)
		x = new char[dimension];
		for (int i=0; i<dimension; i++) {
			x[i] = obj.x[i];
		}
	}
	Object (char *buf) {  // member constructor (needed)
		x = new char[dimension];
		for (int i=0; i<dimension; i++) {
			x[i] = buf[i];
		}
	}
	~Object () { delete []x; }  // destructor

	Object& operator= (const Object& obj) {  // assignment operator (needed)
		for (int i=0; i<dimension; i++) {
			x[i] = obj.x[i];
		}
		return *this;
	}
	int operator== (const Object& obj) {  // equality operator (needed)
		for (int i=0; i<dimension; i++) {
			if (x[i]!=obj.x[i]) {
				return 0;
			}
		}
		return 1;
	}
	int operator!= (const Object& obj) { return !(*this==obj); }  // inequality operator (needed)

	double distance (const Object& other) const {  // distance method (needed)
		compdists++;
		string strA = string(x, dimension);
		string strB = string(other.x, dimension);
		int lenA = strA.find("!");
		int lenB = strB.find("!");
		lenA = (lenA!=-1 ? lenA: strA.size());
		lenB = (lenB!=-1 ? lenB: strB.size());
		return EditDistance(x, lenA, other.x, lenB);
	}
	int CompressedLength () const { return ObjectSize (); }  // return the compressed size of this object
	void Compress (char *buf) { memcpy (buf, x, dimension*sizeof(char)); }  // object compression
#ifdef PRINTING_OBJECTS
	void Print (ostream& os) const {
		os << "(" << x[0];
		for (int i=1; i<dimension; i++) {
			os << ", " << x[i];
		}
		os << ")";
	}
#endif

	char *x;  // the coords
};

Object *Read (istream& in);  // read an object from standard input or a file

#endif
