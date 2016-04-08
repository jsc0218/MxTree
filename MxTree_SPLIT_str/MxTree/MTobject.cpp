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
	getline(in, cmdLine);
	char *x = new char[dimension];
	for (int i=0; i<dimension; i++) {
		x[i] = i<cmdLine.size() ? cmdLine[i]: '!';
	}
	Object *obj = new Object (x);
	delete []x;
	return obj;
}

int EditDistance(const char* strA, int lenA, const char* strB, int lenB)
{
	int **rec = new int *[2];
	rec[0] = new int[lenB+1];
	rec[1] = new int[lenB+1];
	for (int j=0; j<lenB+1; j++) {
		rec[0][j] = j;
	}

	for (int i=0; i<lenA; i++) {
		rec[(i+1)%2][0] = i+1;
		for (int j=0; j<lenB; j++) {
			if (strA[i] == strB[j]) {
				rec[(i+1)%2][j+1] = rec[i%2][j];
			} else {
				rec[(i+1)%2][j+1] = min(min(rec[i%2][j+1], rec[(i+1)%2][j]), rec[i%2][j])+1;
			}
		}
	}

	int result = rec[lenA%2][lenB];
	delete[] rec[0];
	delete[] rec[1];
	delete[] rec;
	return result;
}
