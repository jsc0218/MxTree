/*
 * Command line driver for the M-tree / MX-tree index.
 *
 * Each variant directory holds an identical copy of this file. The tree class
 * it drives is decided by whichever headers sit next to it, so one driver
 * serves every variant; tests/check_drivers.sh fails the build if the copies
 * drift apart.
 *
 * Queries are addressed by dataset row number rather than by a literal object,
 * which keeps the syntax the same whether the objects are vectors, integers or
 * strings, and lets the scan-* commands answer the identical question without
 * an index. tests/happy_path.sh relies on that to check the index against a
 * linear scan.
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#if __has_include("MXTree.h")
#include "MXTree.h"
typedef MXTree IndexTree;
#define MXTREE_FAMILY 1
#else
#include "MT.h"
typedef MT IndexTree;
#endif

#include "MTpredicate.h"

using namespace std;

/* Counters the tree sources keep; they are read back by the stats command. */
int compdists;
int IOread, IOwrite;

/* Object dimensionality. Must be set before any Object is constructed. */
int dimension = 0;

#ifdef MXTREE_FAMILY
/*
 * Super-node capacity, and the disk cost model the MX-tree uses to decide
 * whether a trade pays for itself. The values are the ones the ICCSA'13
 * measurements were taken with.
 */
int MAX_ENTRY_NUM = 80;
double AccessTime = 17.55556;
double TimePerPage = 0.15;
double MAX_OVERLAP = 0;
string BitMapPath;
#endif

/* Split policy, defined in the node sources and set from the command line. */
extern double MIN_UTIL;
extern pp_function PROMOTE_PART_FUNCTION;
extern pv_function PROMOTE_VOTE_FUNCTION;
extern pp_function SECONDARY_PART_FUNCTION;
extern int NUM_CANDIDATES;

namespace {

struct Options {
	string command;
	string data;
	string index;
	int dim;
	long count;
	long query;
	double radius;
	int k;
	double minUtil;
	int promote;
	int secondary;
	int vote;
	int candidates;

	Options()
		: dim(0), count(-1), query(0), radius(0), k(1), minUtil(0.4),
		  promote(mM_RAD), secondary(mM_RAD), vote(mM_RADV), candidates(10) { }
};

void Usage(const char *program)
{
	cerr << "usage: " << program << " <command> [options]\n"
	     << "\n"
	     << "commands:\n"
	     << "  build        insert every object of a dataset into a new index\n"
	     << "  range        range query answered from the index\n"
	     << "  knn          k nearest neighbours answered from the index\n"
	     << "  scan-range   the same range query answered by a linear scan\n"
	     << "  scan-knn     the same k-NN query answered by a linear scan\n"
	     << "  stats        per-level statistics of an existing index\n"
	     << "  check        verify the covering radii of an existing index\n"
	     << "\n"
	     << "options:\n"
	     << "  --data PATH    dataset, one object per line\n"
	     << "  --index PATH   index file to create or read\n"
	     << "  --dim N        object dimensionality (required)\n"
	     << "  --count N      stop after N objects (default: whole file)\n"
	     << "  --query N      dataset row to use as the query object\n"
	     << "  --radius R     range query radius\n"
	     << "  -k N           number of neighbours\n"
	     << "  --min-util U   minimum node utilisation, 0 to 0.5 (default 0.4)\n"
	     << "  --promote N    split promotion function 0..3 (default 3, mM_RAD)\n"
	     << "  --secondary N  root promotion function 0..3 (default 3, mM_RAD)\n"
	     << "  --vote N       confirmed promotion function 0..3 (default 3)\n"
	     << "  --candidates N sample size when promotion is 2 (default 10)\n"
	     << "\n"
	     << "range and scan-range print the matching row numbers in ascending\n"
	     << "order; knn and scan-knn print the neighbour distances in ascending\n"
	     << "order, so index and scan output can be compared directly.\n";
}

/* Reports a fatal usage or I/O problem and exits. */
void Fail(const string &message)
{
	cerr << "error: " << message << endl;
	exit(2);
}

Options Parse(int argc, char **argv)
{
	if (argc < 2) {
		Usage(argv[0]);
		exit(2);
	}

	Options opt;
	opt.command = argv[1];

	for (int i = 2; i < argc; i++) {
		string flag = argv[i];
		bool needsValue = (flag != "--help");
		if (needsValue && i + 1 >= argc) {
			Fail(flag + " needs a value");
		}
		const char *value = needsValue ? argv[i + 1] : "";

		if (flag == "--data") {
			opt.data = value;
		} else if (flag == "--index") {
			opt.index = value;
		} else if (flag == "--dim") {
			opt.dim = atoi(value);
		} else if (flag == "--count") {
			opt.count = atol(value);
		} else if (flag == "--query") {
			opt.query = atol(value);
		} else if (flag == "--radius") {
			opt.radius = atof(value);
		} else if (flag == "-k") {
			opt.k = atoi(value);
		} else if (flag == "--min-util") {
			opt.minUtil = atof(value);
		} else if (flag == "--promote") {
			opt.promote = atoi(value);
		} else if (flag == "--secondary") {
			opt.secondary = atoi(value);
		} else if (flag == "--vote") {
			opt.vote = atoi(value);
		} else if (flag == "--candidates") {
			opt.candidates = atoi(value);
		} else if (flag == "--help") {
			Usage(argv[0]);
			exit(0);
		} else {
			Fail("unknown option " + flag);
		}
		i++;
	}

	if (opt.dim <= 0) {
		Fail("--dim is required and must be positive");
	}
	dimension = opt.dim;

	MIN_UTIL = opt.minUtil;
	PROMOTE_PART_FUNCTION = (pp_function) opt.promote;
	SECONDARY_PART_FUNCTION = (pp_function) opt.secondary;
	PROMOTE_VOTE_FUNCTION = (pv_function) opt.vote;
	NUM_CANDIDATES = opt.candidates;

	if (SECONDARY_PART_FUNCTION == CONFIRMED) {
		Fail("--secondary cannot be 1: the root promotion cannot use stored distances");
	}

#ifdef MXTREE_FAMILY
	/*
	 * The MX-tree keeps a bitmap of super-node pages alongside the index.
	 * MXTfile opens it as soon as the store is constructed, so the path has
	 * to be known before the tree is created or opened.
	 */
	if (!opt.index.empty()) {
		BitMapPath = opt.index + ".bitmap";
	}
#endif

	return opt;
}

typedef vector<Object *> Dataset;

Dataset LoadDataset(const string &path, long count)
{
	if (path.empty()) {
		Fail("--data is required");
	}
	ifstream in(path.c_str());
	if (!in) {
		Fail("cannot read dataset " + path);
	}

	Dataset data;
	while (count < 0 || (long) data.size() < count) {
		Object *object = Read(in);
		if (!in) {  /* short or empty final record */
			delete object;
			break;
		}
		data.push_back(object);
	}
	if (data.empty()) {
		Fail("dataset " + path + " holds no objects at --dim " + to_string(dimension));
	}
	return data;
}

void Release(Dataset &data)
{
	for (size_t i = 0; i < data.size(); i++) {
		delete data[i];
	}
	data.clear();
}

Object *QueryObject(const Dataset &data, long row)
{
	if (row < 0 || row >= (long) data.size()) {
		Fail("--query is outside the dataset");
	}
	return data[row];
}

IndexTree *OpenIndex(const Options &opt)
{
	if (opt.index.empty()) {
		Fail("--index is required");
	}
	IndexTree *tree = new IndexTree;
	tree->Open(opt.index.c_str());
	if (!tree->IsOpen()) {
		Fail("cannot open index " + opt.index);
	}
	return tree;
}

int Build(const Options &opt)
{
	if (opt.index.empty()) {
		Fail("--index is required");
	}
	Dataset data = LoadDataset(opt.data, opt.count);

	/*
	 * The store refuses to create an index on top of a file that already
	 * exists, so clear both the index and its bitmap first.
	 */
	remove(opt.index.c_str());
#ifdef MXTREE_FAMILY
	remove(BitMapPath.c_str());
#endif

	IndexTree *tree = new IndexTree;
	tree->Create(opt.index.c_str());
	if (!tree->IsOpen()) {
		Fail("cannot create index " + opt.index);
	}
	for (size_t i = 0; i < data.size(); i++) {
		tree->Insert(MTentry(MTkey(*data[i], 0, 0), (GiSTpage) i));
	}
	delete tree;

	cerr << "indexed " << data.size() << " objects of dimension " << dimension
	     << " into " << opt.index << " (" << compdists << " distance computations)" << endl;
	Release(data);
	return 0;
}

int Range(const Options &opt)
{
	Dataset data = LoadDataset(opt.data, opt.count);
	Object *query = QueryObject(data, opt.query);

	IndexTree *tree = OpenIndex(opt);
	Pred predicate(*query);
	SimpleQuery search(&predicate, opt.radius);
	GiSTlist<MTentry *> matches = tree->RangeSearch(search);

	vector<long> rows;
	while (!matches.IsEmpty()) {
		MTentry *entry = matches.RemoveFront();
		rows.push_back((long) entry->Ptr());
		delete entry;
	}
	delete tree;

	sort(rows.begin(), rows.end());
	for (size_t i = 0; i < rows.size(); i++) {
		cout << rows[i] << "\n";
	}
	cerr << rows.size() << " matches, " << compdists << " distance computations" << endl;
	Release(data);
	return 0;
}

int ScanRange(const Options &opt)
{
	Dataset data = LoadDataset(opt.data, opt.count);
	Object *query = QueryObject(data, opt.query);

	/*
	 * Mirrors the leaf-level test in SimpleQuery::Consistent, where a leaf
	 * entry has a zero covering radius: distance <= radius, inclusive.
	 */
	long matches = 0;
	for (size_t i = 0; i < data.size(); i++) {
		if (query->distance(*data[i]) <= opt.radius) {
			cout << i << "\n";
			matches++;
		}
	}
	cerr << matches << " matches by linear scan" << endl;
	Release(data);
	return 0;
}

void PrintDistances(vector<double> &distances)
{
	sort(distances.begin(), distances.end());
	cout << fixed << setprecision(6);
	for (size_t i = 0; i < distances.size(); i++) {
		cout << distances[i] << "\n";
	}
}

int Knn(const Options &opt)
{
	Dataset data = LoadDataset(opt.data, opt.count);
	Object *query = QueryObject(data, opt.query);
	if (opt.k < 1 || opt.k > (long) data.size()) {
		Fail("-k must be between 1 and the number of objects");
	}

	IndexTree *tree = OpenIndex(opt);
	MTpred *predicate = new Pred(*query);
	TopQuery search(predicate, opt.k);
	delete predicate;

	MTentry **results = tree->TopSearch(search);
	vector<double> distances;
	for (int i = 0; i < opt.k; i++) {
		if (results[i] == NULL) {
			continue;
		}
		distances.push_back(query->distance(results[i]->object()));
		delete results[i];
	}
	delete[] results;
	delete tree;

	/*
	 * Distances rather than row numbers: equidistant objects make the set of
	 * neighbours ambiguous, but the distances themselves are not.
	 */
	PrintDistances(distances);
	cerr << distances.size() << " neighbours, " << compdists << " distance computations" << endl;
	Release(data);
	return 0;
}

int ScanKnn(const Options &opt)
{
	Dataset data = LoadDataset(opt.data, opt.count);
	Object *query = QueryObject(data, opt.query);
	if (opt.k < 1 || opt.k > (long) data.size()) {
		Fail("-k must be between 1 and the number of objects");
	}

	vector<double> distances;
	for (size_t i = 0; i < data.size(); i++) {
		distances.push_back(query->distance(*data[i]));
	}
	sort(distances.begin(), distances.end());
	distances.resize(opt.k);

	PrintDistances(distances);
	cerr << distances.size() << " neighbours by linear scan" << endl;
	Release(data);
	return 0;
}

int Stats(const Options &opt)
{
	IndexTree *tree = OpenIndex(opt);
#ifndef MXTREE_FAMILY
	/*
	 * TreeHeight is defined in BulkLoad.cpp, which only the M-tree variants
	 * carry; the MX-tree ones declare it in MT.h but never define it.
	 */
	cout << "height " << tree->TreeHeight() << endl;
#endif
	tree->CollectStats();
	delete tree;
	return 0;
}

int Check(const Options &opt)
{
	IndexTree *tree = OpenIndex(opt);
	GiSTpath root;
	root.MakeRoot();
	BOOL consistent = tree->CheckNode(root, NULL);
	delete tree;
	if (!consistent) {
		cerr << "index is inconsistent" << endl;
		return 1;
	}
	cerr << "index is consistent" << endl;
	return 0;
}

}  /* namespace */

int main(int argc, char **argv)
{
	Options opt = Parse(argc, argv);

	if (opt.command == "build") {
		return Build(opt);
	}
	if (opt.command == "range") {
		return Range(opt);
	}
	if (opt.command == "scan-range") {
		return ScanRange(opt);
	}
	if (opt.command == "knn") {
		return Knn(opt);
	}
	if (opt.command == "scan-knn") {
		return ScanKnn(opt);
	}
	if (opt.command == "stats") {
		return Stats(opt);
	}
	if (opt.command == "check") {
		return Check(opt);
	}

	Usage(argv[0]);
	return 2;
}
