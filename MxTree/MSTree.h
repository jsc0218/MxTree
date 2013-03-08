#pragma once

#include <vector>
using namespace std;

class MSTree 
{
public:
	MSTree(int _start=0);
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
		TreeEdge(): head(-1), tail(-1), len(MAXDOUBLE) {}
		TreeEdge(int _head, int _tail, double _len): head(_head), tail(_tail), len(_len) {}

		int head;
		int tail;
		double len;
	};

	class Record
	{
	public:
		Record(): closest(-1), lowCost(MAXDOUBLE) {}

		int closest;
		double lowCost;
	};
	
	const TreeEdge& LongestEdge();

	vector<vector<double>> vGraph;
	vector<TreeEdge> eMSTree;
	double pathLen;
	int start;
};
