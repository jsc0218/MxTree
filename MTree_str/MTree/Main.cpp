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
#include "time.h"
#include <fstream>
#include <string>
using namespace std;
#ifdef UNIX
#include <unistd.h>
#include <ctype.h>
#endif

#include "MT.h"
#include "MTpredicate.h"

int compdists;
int IOread, IOwrite;
int objs;
int dimension = 59;
MT *mtree = NULL;
string path = "H:\\authors3.txt";
string db = "H:\\MTREE_DBLP";
//string path = "H:\\English.dic";
//string db = "H:\\MTREE_ENG";
int amount = 100000;

extern double MIN_UTIL;
extern pp_function PROMOTE_PART_FUNCTION;
extern pv_function PROMOTE_VOTE_FUNCTION;
extern pp_function SECONDARY_PART_FUNCTION;
extern int NUM_CANDIDATES;

// close the tree and delete it
void
CommandClose ()
{
	if (mtree) {
		delete mtree;
		mtree = NULL;
	}
}

// create a new tree and open it
void
CommandCreate (const char *method, const char *table)
{
	CommandClose ();
	if (!strcmp(method, "mtree")) {
		mtree = new MT;
	} else {
		cerr << "The only supported method is mtree."<<endl;
		return;
	}
	mtree->Create(table);
	if (!mtree->IsOpen()) {
		cout << "Error opening index."<<endl;
		delete mtree;
		mtree = NULL;
		cout << "Error opening table."<<endl;
		return;
	}
	cout << "Table " << table << " created as type " << method << "."<<endl;
}

// delete the tree from disk
void
CommandDrop (const char *table)
{
	if (unlink(table)) {
		cout << "There is no table by that name."<<endl;
	} else {  // delete file successfully
		cout << "Table " << table << " dropped."<<endl;
	}
}

// open the specified tree
void
CommandOpen (const char *method, const char *table)
{
	CommandClose ();
	if (!strcmp(method, "mtree")) {
		mtree = new MT;
	} else {
		cerr << "The only supported method is mtree."<<endl;
		return;
	}
	mtree->Open(table);
	if (!mtree->IsOpen()) {
		delete mtree;
		mtree = NULL;
		cout << "Error opening table."<<endl;
		return;
	}
	cout << "Table " << table << " opened."<<endl;
}

// execute a nearest neighbor query
void
CommandNearest (const TopQuery &query)
{
	MTentry **results = mtree->TopSearch(query);

	for (int i=0; i<query.k; i++) {
		MTentry *e = results[i];
		cout << e;
		delete e;
		objs++;
	}
	//cout << endl;
	delete []results;
}

// execute a range query
void
CommandSelect (MTquery& query)
{
	GiSTlist<MTentry *> list = mtree->RangeSearch(query);

	while (!list.IsEmpty()) {
		MTentry *e = list.RemoveFront ();
		delete e;
		objs++;
	}
	//cout << endl;
}

// insert an object in the tree
void
CommandInsert (const MTkey& key, int ptr)
{
	mtree->Insert(MTentry(key, (GiSTpage)ptr));
}

// use the BulkLoading alogrithm to create the tree
void
CommandLoad (const char *table, MTentry **data, int n)
{
	CommandDrop (table);
	CommandCreate ("mtree", table);
	cout << "MinUtil = " << MIN_UTIL<<endl;
	mtree->BulkLoad(data, n);
}

// perform a check of the nodes of the tree
void
CommandCheck ()
{
	GiSTpath path;
	path.MakeRoot ();
	mtree->CheckNode(path, NULL);
}

// print a dump of each node of the tree
void
CommandDump ()
{
	GiSTpath path;
	path.MakeRoot ();
#ifdef PRINTING_OBJECTS
	mtree->DumpNode(cout, path);
#endif
}

// collect and print statistics on the tree
void
CommandStats ()
{
	mtree->CollectStats();
}

void
SetStrategy (int argc, char**argv)
{
	if (argc < 4) {
		cout << "Usage is: MTree [min_util] [promote_f] [sec_promote_f] ([vote_f] ([n_cand]))"<<endl;
		exit (-1);
	}
	MIN_UTIL = atof (argv[1]);
	PROMOTE_PART_FUNCTION = (pp_function) atoi (argv[2]);
	SECONDARY_PART_FUNCTION = (pp_function) atoi (argv[3]);
	if (SECONDARY_PART_FUNCTION == CONFIRMED) {
		cout << "The secondary promotion function must be an unconfirmed one"<<endl;
		exit (-1);
	}
	if (PROMOTE_PART_FUNCTION == SAMPLING) {
		if (argc < 5) {
			cout << "Usage is: MTree [min_util] [promote_f] ([vote_f] ([n_cand]))"<<endl;
			exit (-1);
		}
		NUM_CANDIDATES = atoi (argv[4]);
	}
	if (PROMOTE_PART_FUNCTION == CONFIRMED) {
		if (argc < 5) {
			cout << "Usage is: MTree [min_util] [promote_f] [sec_promote_f] ([vote_f] ([n_cand]))"<<endl;
			exit (-1);
		}
		PROMOTE_VOTE_FUNCTION = (pv_function) atoi (argv[4]);
		if (PROMOTE_VOTE_FUNCTION == SAMPLINGV || SECONDARY_PART_FUNCTION == SAMPLING) {
			if (argc < 6) {
				cout << "Usage is: MTree [min_util] [promote_f] ([vote_f] ([n_cand]))"<<endl;
				exit (-1);
			}
			NUM_CANDIDATES = atoi (argv[5]);
		}
	}
	switch (PROMOTE_PART_FUNCTION) {
		case RANDOM:
			cout << "RAN_2 ";
			break;
		case CONFIRMED:
			switch (PROMOTE_VOTE_FUNCTION) {
				case RANDOMV:
					cout << "RAN_1 ";
					break;
				case SAMPLINGV:
					cout << "SAMP" << NUM_CANDIDATES << "_1 ";
					break;
				case MAX_LB_DISTV:
					cout << "M_LB_d_1 ";
					break;
				case mM_RADV:
					cout << "mM_R_1";
					break;
			}
			break;
		case SAMPLING:
			cout << "SAMP" << NUM_CANDIDATES << "_2 ";
			break;
		case mM_RAD:
			cout << "mM_R_2 ";
			break;
	}
	switch (SECONDARY_PART_FUNCTION) {
		case RANDOM:
			cout << "(RAN_2)"<<endl;
			break;
		case SAMPLING:
			cout << "(SAMP" << NUM_CANDIDATES << "_2)"<<endl;
			break;
		case mM_RAD:
			cout << "(mM_R_2)"<<endl;
			break;
		default:
			break;
	}
}

Object *Read(ifstream& fin, int k)
{
	fin.seekg(0, ios::beg);
	for (int i=0; i<k; i++) {
		string str;
		getline(fin, str);
	}
	
	Object *obj = Read(fin);
	return obj;
}

void Progress(int i, int number)
{
	double x = ((double)(i+1))/number;
	if (x==0.1 || x==0.3 || x==0.5 || x==0.7 || x==0.9) {
		time_t now_time = time(NULL);
		struct tm *tblock = localtime(&now_time);
		cout<<">>>>>>>>>>>"<<x*100<<"% "<<asctime(tblock)<<endl;
	}
}

int main (int argc, char **argv)
{
	string cmdLine;
	compdists = IOread = IOwrite = objs = 0;
	cout << "**  MTree: An M-Tree based on Generalized Search Trees  **"<<endl;
	while (cmdLine != "quit") {
		cout<<endl<<"Please command!"<<endl;
		cin>>cmdLine;
		if (cmdLine == "drop") {
			CommandDrop (db.c_str());
			SetStrategy (argc, argv);
			CommandCreate ("mtree", db.c_str());
		} else if (cmdLine == "insert") {
			if (!mtree) {
				CommandOpen ("mtree", db.c_str());
			}

			time_t time_start, time_end;
			time(&time_start);
			
			ifstream fin(path.c_str());
			for (int i=0; i<amount; i++) {
				Object *obj = Read (fin);
				CommandInsert (MTkey (*obj, 0, 0), i);
				delete obj;
				Progress(i, amount);
			}
			fin.close();

			time(&time_end); 
			cout<<difftime(time_end, time_start)<<endl;

		} else if (cmdLine == "load") {
			if (argc < 2) {
				cout << "Usage is: MTree [min_util]"<<endl;
				exit (-1);
			}
			MIN_UTIL = atof (argv[1]);

			ifstream fin(path.c_str());

			cin>>cmdLine;
			int n = atoi(cmdLine.c_str());

			MTentry **entries = new MTentry*[n];
			for (int i=0; i<n; i++) {
				Object *obj = Read (fin);
				entries[i] = new MTentry (MTkey (*obj, 0, 0), 0);
				delete obj;
			}
			CommandLoad (db.c_str(), entries, n);

			for (int i=0; i<n; i++) {
				delete entries[i];
			}
			delete []entries;

			fin.close();
		} else if (cmdLine == "select") {
			if (!mtree) {
				CommandOpen ("mtree", db.c_str());
			}
			
			time_t time_start, time_end;
			time(&time_start);  
			
			ifstream fin(path.c_str());
			for (int i=0; i<amount/1000; i++) {
				Object *obj = Read(fin, rand()%amount);
				Pred *pred = new Pred(*obj);
				delete obj;
				SimpleQuery query(pred, 45);
				delete pred;
				CommandSelect (query);
				Progress(i, 1200);
			}
			fin.close();

			time(&time_end); 
			cout<<difftime(time_end, time_start)<<endl;

			CommandClose ();
		} else if (cmdLine == "nearest") {
			if (!mtree) {
				CommandOpen ("mtree", db.c_str());
			}

			time_t time_start, time_end;
			time(&time_start);  

			ifstream fin(path.c_str());
			for (int i=0; i<amount/1000; i++) {
				Object *obj = Read(fin, rand()%amount);
				MTpred *pred = new Pred(*obj);
				delete obj;
				TopQuery query (pred, 1);
				delete pred;
				CommandNearest (query);
				Progress(i, amount/1000);
			}
			fin.close();
			
			time(&time_end); 
			cout<<difftime(time_end, time_start)<<endl;

			CommandClose ();
		} else if (cmdLine == "cursor") {
			ifstream fin(path.c_str());
			Object *obj = Read (fin);
			fin.close();
			Pred pred (*obj);
			delete obj;

			if (!mtree) {
				CommandOpen ("mtree", db.c_str());
			}
			MTcursor cursor (*mtree, pred);

			cin>>cmdLine;
			while (!(cmdLine == "close")) {
				if (cmdLine == "next") {
					cin>>cmdLine;
					int k = atoi (cmdLine.c_str());
					GiSTlist<MTentry *> list;
					for (; k>0; k--) {
						list.Append (cursor.Next());
					}
					while (!list.IsEmpty()) {
						MTentry *e = list.RemoveFront ();
						cout << e;
						delete e;
						objs++;
					}
				}
				cin>>cmdLine;
			}
			CommandClose ();
		} else if (cmdLine == "dump") {
			if (!mtree) {
				CommandOpen ("mtree", db.c_str());
			}
			CommandDump ();
			CommandClose ();
		} else if (cmdLine == "stats") {
			if (!mtree) {
				CommandOpen ("mtree", db.c_str());
			}
			CommandStats ();
			CommandClose ();
		} else if (cmdLine == "check") {
			if (!mtree) {
				CommandOpen ("mtree", db.c_str());
			}
			CommandCheck ();
			CommandClose ();
		}  else if (cmdLine == "add") {
			if (!mtree) {
				CommandOpen ("mtree", db.c_str());
			}
			SetStrategy (argc, argv);
		}
	}
	cout << "Computed dists = " << compdists << endl;
	cout << "IO reads = " << IOread << endl;
	cout << "IO writes = " << IOwrite << endl;
	cout << "Objs = " << objs << endl;
	CommandClose ();
	cout << "Goodbye."<<endl;
}