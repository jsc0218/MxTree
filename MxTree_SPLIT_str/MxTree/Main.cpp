#include "MXTree.h"
#include <fstream>
#include <string>
#include <ctime>
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
//string path = "E:\\EP\\DB\\Dimensionality & Radius\\UNIFORM\\20-100.txt";
//string BitMapPath = "H:\\EP\\INDEX\\Dimensionality & Radius\\UNIFORM\\MXTREE_SPLIT\\BitMap_20_100.txt";
//string MXTreePath = "H:\\EP\\INDEX\\Dimensionality & Radius\\UNIFORM\\MXTREE_SPLIT\\MXTREE_20_100";
string path = "H:\\authors3.txt";
string BitMapPath = "H:\\BitMap_MXTREE_DBLP.txt";
string MXTreePath = "H:\\MXTREE_DBLP";
int amount = 100000;

int MAX_ENTRY_NUM = 0;
double AccessTime = 17.55556;
double TimePerPage = 0.15;

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
	if (x==0.1 || /*x==0.2 ||*/ x==0.3 || /*x==0.4 ||*/ x==0.5 || /*x==0.6 ||*/ x==0.7 || /*x==0.8 ||*/ x==0.9) {
		time_t now_time = time(NULL);
		struct tm *tblock = localtime(&now_time);
		cout<<">>>>>>>>>>>"<<x*100<<"% "<<asctime(tblock)<<endl;
	}
}

int main() 
{
	MXTree *tree = new MXTree;
	tree->Create(MXTreePath.c_str());
	assert(tree->IsOpen());
	tree->Open(MXTreePath.c_str());
	
	time_t time_start, time_end;
	time(&time_start);

	ifstream fin(path.c_str());
	for (int i=0; i<amount; i++) {
		Object *obj = Read(fin);
		tree->Insert(MTentry(MTkey(*obj, 0, 0), i));
		delete obj;
		Progress(i, amount);
	}
	fin.close();

	time(&time_end); 
	cout<<difftime(time_end, time_start)<<endl;

	GiSTpath path;
	path.MakeRoot();
	//tree->DumpNode(cout, path);
	tree->CheckNode(path, NULL);
	tree->CollectStats();
	
	delete tree;
	//unlink(MXTreePath.c_str());
	//unlink(BitMapPath.c_str());

	cout << "Computed dists = " << compdists << endl;
	cout << "IO reads = " << IOread << endl;
	cout << "IO writes = " << IOwrite << endl;
	return 0;
}

//int main()
//{
//	MXTree *tree = new MXTree;
//	tree->Open(MXTreePath.c_str());
//	assert(tree->IsOpen());
//
//	time_t time_start, time_end;
//	time(&time_start);
//
//	ifstream fin(path.c_str());
//	for (int i=0; i<amount/1000; i++) {
//		Object *obj = Read(fin, rand()%amount);
//		Pred *pred = new Pred(*obj);
//		delete obj;
//		
//		SimpleQuery query(pred, 7.33648417159064);
//		delete pred;
//
//		GiSTlist<MTentry *> list = tree->RangeSearch(query);
//		while (!list.IsEmpty()) {
//			MTentry *e = list.RemoveFront();
//			++objs;
//			delete e;
//		}
//		Progress(i, amount/1000);
//	}
//	fin.close();
//
//	time(&time_end);
//	cout<<difftime(time_end, time_start)<<endl;
//
//	delete tree;
//
//	cout << "Computed dists = " << compdists << endl;
//	cout << "IO reads = " << IOread << endl;
//	cout << "IO writes = " << IOwrite << endl;
//	cout << "Objs = " << objs << endl;
//
//	return 0;
//}

//int main()
//{
//	MXTree *tree = new MXTree;
//	tree->Open(MXTreePath.c_str());
//	assert(tree->IsOpen());
//
//	time_t time_start, time_end;
//	time(&time_start);
//
//	ifstream fin(path.c_str());
//	for (int i=0; i<amount/1000; i++) {
//		Object *obj = Read(fin, rand()%amount);
//		MTpred *pred = new Pred(*obj);
//		delete obj;
//		TopQuery query(pred, 1);
//		delete pred;
//
//		MTentry **results = tree->TopSearch(query);
//		for (int i=0; i<query.k; i++) {
//			MTentry *e = results[i];
//			delete e;
//			++objs;
//		}
//		delete[] results;
//		Progress(i, amount/1000);
//	}
//	fin.close();
//
//	time(&time_end); 
//	cout<<difftime(time_end, time_start)<<endl;
//
//	delete tree;
//	cout << "Computed dists = " << compdists << endl;
//	cout << "IO reads = " << IOread << endl;
//	cout << "IO writes = " << IOwrite << endl;
//	cout << "Objs = " << objs << endl;
//	return 0;
//}
