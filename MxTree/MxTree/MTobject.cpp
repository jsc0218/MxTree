#include "MTobject.h"
#include "MTcursor.h"
#include <string>
using namespace std;

double MaxDist ()
{
//	return dimension;
//	return sqrt (dimension);
	return MAXDOUBLE;
}

int ObjectSize ()
{
	return dimension * sizeof(double);  // objects of constant size
//	return 0;  // objects of different sizes
}

Object *Read (istream& in)
{
	string cmdLine;
	double *x = new double[dimension];
	for (int i=0; i<dimension; i++) {
		in>>cmdLine;
		x[i] = atof (cmdLine.c_str());
	}
	Object *obj = new Object (x);
	delete []x;
	return obj;
}
