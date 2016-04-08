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

class Object : public GiSTobject  // the DB object class
{
public:
	Object () { x = new double[dimension]; }  // default constructor (needed)
	Object (const Object& obj) {  // copy constructor (needed)
		x = new double[dimension];
		for (int i=0; i<dimension; i++) {
			x[i] = obj.x[i];
		}
	}
	Object (char *buf) { x = new double[dimension]; memcpy (x, buf, dimension*sizeof(double)); }  // member constructor (needed)
	Object (double *buf) {  // member constructor (needed)
		x = new double[dimension];
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
		double dist = 0;
		for (int i=0; i<dimension; i++) {  // L2
			dist += pow (x[i]-other.x[i], 2);
		}
		return sqrt(dist);
	}
	int CompressedLength () const { return ObjectSize (); }  // return the compressed size of this object
	void Compress (char *buf) { memcpy (buf, x, dimension*sizeof(double)); }  // object compression
#ifdef PRINTING_OBJECTS
	void Print (ostream& os) const {
		os << "(" << x[0];
		for (int i=1; i<dimension; i++) {
			os << ", " << x[i];
		}
		os << ")";
	}
#endif

	double *x;  // the coords
};

Object *Read (istream& in);  // read an object from standard input or a file

#endif
