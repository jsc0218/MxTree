#pragma once

#include <vector>
using namespace std;


class UFSet 
{
public:
	UFSet(int size);
	~UFSet();
	int Find(int name);
	void Union(int name1, int name2);
	int CollapsingFind(int name);
	void WeightedUnion(int name1, int name2);
private:
	vector<int> parent;
};


class MSTree 
{
public:
	MSTree();
	void Input(const vector<double>& vec);
	void Build();
	void DumpMatrix();
	void DumpPathLen();
	void DumpPath();
	void Divide(vector<int>& vec1, vector<int>& vec2, double radii[], int cands[], const vector<double>& maxRadii);

private:
	class TreeEdge
	{
	public:
		TreeEdge(int _head, int _tail, double _len): head(_head), tail(_tail), len(_len) {}
		bool operator<(const TreeEdge& edge) const { return len > edge.len; }

		int head;
		int tail;
		double len;
	};

	vector<vector<double>> vGraph;
	vector<TreeEdge> eMSTree;
	double pathLen;
};
