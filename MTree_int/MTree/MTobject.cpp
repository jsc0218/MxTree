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
	return dimension * sizeof(int);  // objects of constant size
//	return 0;  // objects of different sizes
}

Object *Read (istream& in)
{
	string cmdLine;
	int *x = new int[dimension];
	for (int i=0; i<dimension; i++) {
		in>>cmdLine;
		x[i] = atoi (cmdLine.c_str());
	}
	Object *obj = new Object (x);
	delete []x;
	return obj;
}
