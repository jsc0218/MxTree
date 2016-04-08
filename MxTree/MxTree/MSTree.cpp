#include <iostream>
#include <queue>
using namespace std;
#include <cassert>
#include "MT.h"
#include "MSTree.h"

MSTree::MSTree(int _start)
{
	start = _start;
	pathLen = 0;
}

void MSTree::Input(const vector<double>& vec)
{
	vGraph.push_back(vec);
}

void MSTree::Build()
{
	bool *vbInTree = new bool[vGraph.size()];
	for (int i=0; i<vGraph.size(); i++) {
		vbInTree[i] = false;
	}
	vbInTree[start] = true;

	Record *record = new Record[vGraph.size()];
	for (int i=0; i<vGraph.size(); i++) {
		record[i].closest = start;
		record[i].lowCost = vGraph[start][i];
	}

	for (int i=0; i<vGraph.size()-1; i++) {
		int k=0;
		for (; vbInTree[k]; k++);

		for (int j=k+1; j<vGraph.size(); j++) {
			if (!vbInTree[j] && record[j].lowCost<record[k].lowCost) {
				k = j;
			}
		}

		eMSTree.push_back(TreeEdge(k, record[k].closest, record[k].lowCost));
		pathLen += record[k].lowCost;
		vbInTree[k] = true;

		for (int j=0; j<vGraph.size(); j++) {
			if (!vbInTree[j] && vGraph[j][k]<record[j].lowCost) {
				record[j].lowCost = vGraph[j][k];
				record[j].closest = k;
			}
		}
	}
	delete[] vbInTree;
	delete[] record;
}

void MSTree::DumpMatrix()
{
	cout << "Matrix:" << endl;
	for (int i=0; i<vGraph.size(); i++) {
		cout << i <<": ";
		for (int j=0; j<vGraph[i].size(); j++) {
			cout<< j <<"(" << vGraph[i][j] << ") ";
		}
		cout << endl;
	}
}

void MSTree::DumpPathLen()
{
	cout << "Min path length: " << pathLen << endl;
}

void MSTree::DumpPath()
{
	cout << "Path selection process:" << endl;
	for (int i=0; i<eMSTree.size(); i++) {
		cout << eMSTree[i].tail << "-->" << eMSTree[i].head << "("<< eMSTree[i].len <<")" << endl;
	}
}

const MSTree::TreeEdge& MSTree::LongestEdge()
{
	double max = -1;
	int pos;
	for (int i=0; i<eMSTree.size(); i++) {
		if (eMSTree[i].len > max) {
			max = eMSTree[i].len;
			pos = i;
		}
	}
	return eMSTree[pos];
}

void MSTree::Divide(vector<int>& vec1, vector<int>& vec2, double radii[], int cands[], const vector<double>& maxRadii)
{
	TreeEdge edge = LongestEdge();

	vector<vector<int>> table(vGraph.size());
	for (int i=0; i<eMSTree.size(); i++) {
		table[eMSTree[i].tail].push_back(eMSTree[i].head);
		table[eMSTree[i].head].push_back(eMSTree[i].tail);
	}

	bool *used = new bool[vGraph.size()];
	for (int i=0; i<vGraph.size(); i++) {
		used[i] = false;
	}
	used[edge.tail] = used[edge.head] = true;

	queue<int> q;
	q.push(edge.tail);
	while (!q.empty()) {
		for (int i=0; i<table[q.front()].size(); i++) {
			if (!used[table[q.front()][i]]) {
				q.push(table[q.front()][i]);
				used[table[q.front()][i]] = true;
			}
		}
		vec1.push_back(q.front());
		q.pop();
	}

	q.push(edge.head);
	while (!q.empty()) {
		for (int i=0; i<table[q.front()].size(); i++) {
			if (!used[table[q.front()][i]]) {
				q.push(table[q.front()][i]);
				used[table[q.front()][i]] = true;
			}
		}
		vec2.push_back(q.front());
		q.pop();
	}

	delete[] used;

	radii[0] = radii[1] = MAXDOUBLE;
	int max1, max2;
	for (int i=0; i<vec1.size(); i++) {
		double radius = 0;
		int pos = i;
		for (int j=0; j<vec1.size(); j++) {
			if (i == j) {
				continue;
			}
			double maxRadius = MAX(vGraph[vec1[i]][vec1[j]]+maxRadii[vec1[j]], maxRadii[vec1[i]]);
			if (maxRadius > radius) {
				radius = maxRadius;
				pos = j;
			}
		}
		if (radius < radii[0]) {
			radii[0] = radius;
			cands[0] = vec1[i];
			max1 = vec1[pos];
		}
	}

	for (int i=0; i<vec2.size(); i++) {
		double radius = 0;
		int pos = i;
		for (int j=0; j<vec2.size(); j++) {
			if (i == j) {
				continue;
			}
			double maxRadius = MAX(vGraph[vec2[i]][vec2[j]]+maxRadii[vec2[j]], maxRadii[vec2[i]]);
			if (maxRadius > radius) {
				radius = maxRadius;
				pos = j;
			}
		}
		if (radius < radii[1]) {
			radii[1] = radius;
			cands[1] = vec2[i];
			max2 = vec2[pos];
		}
	}

	if (radii[1]/radii[0] > 1.00001) {
		double delta = MAXDOUBLE;
		int pos = -1;
		for (int i=0; i<vec2.size(); i++) {
			if (vec2[i]==cands[1] || vec2[i]==max2) {
				continue;
			}
			double test = MAX(vGraph[cands[0]][vec2[i]]+maxRadii[vec2[i]], maxRadii[cands[0]]);
			if (fabs(test-radii[1]) < delta) {
				delta = fabs(test-radii[1]);
				pos = i;
			}
		}
		if (pos >= 0) {
			double distance = MAX(vGraph[cands[0]][vec2[pos]]+maxRadii[vec2[pos]], maxRadii[cands[0]]); 
			if (distance>radii[0] && distance/radii[1]<radii[1]/radii[0]) {
				radii[0] = distance;
				vec1.push_back(vec2[pos]);
				vec2.erase(vec2.begin()+pos);
			}
		}
	} else if (radii[0]/radii[1] > 1.00001) {
		double delta = MAXDOUBLE;
		int pos = -1;
		for (int i=0; i<vec1.size(); i++) {
			if (vec1[i]==cands[0] || vec1[i]==max1) {
				continue;
			}
			double test = MAX(vGraph[cands[1]][vec1[i]]+maxRadii[vec1[i]], maxRadii[cands[1]]);
			if (fabs(test-radii[0]) < delta) {
				delta = fabs(test-radii[0]);
				pos = i;
			}
		}
		if (pos >= 0) {
			double distance = MAX(vGraph[cands[1]][vec1[pos]]+maxRadii[vec1[pos]], maxRadii[cands[1]]);
			if (distance>radii[1] && distance/radii[0]<radii[0]/radii[1]) {
				radii[1] = distance;
				vec2.push_back(vec1[pos]);
				vec1.erase(vec1.begin()+pos);
			}
		}
	}
}

