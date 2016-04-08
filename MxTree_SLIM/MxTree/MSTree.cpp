#include <iostream>
#include <queue>
using namespace std;
#include <cassert>
#include "MT.h"
#include "MSTree.h"

UFSet::UFSet(int size)
{
	parent.resize(size, -1);
}

UFSet::~UFSet()
{
	parent.clear(); 
}

int UFSet::Find(int name)
{
	while (parent[name] >= 0) 
	{
		name = parent[name];
	}

	return name;
}

void UFSet::Union(int name1, int name2)
{
	int root1 = CollapsingFind(name1);
	int root2 = CollapsingFind(name2);
	assert(root1 != root2);

	parent[root1] += parent[root2];
	parent[root2] = root1;
}

int UFSet::CollapsingFind(int name)
{
	int root = Find(name);

	while (name != root)
	{
		int temp = parent[name];
		parent[name] = root;
		name = temp;
	}

	return root;
}

void UFSet::WeightedUnion(int name1, int name2)
{
	int root1 = CollapsingFind(name1);
	int root2 = CollapsingFind(name2);
	assert(root1 != root2);

	int amount = parent[root1] + parent[root2];

	if (parent[root2] < parent[root1])
	{
		parent[root1] = root2;
		parent[root2] = amount;
	} 
	else
	{
		parent[root2] = root1;
		parent[root1] = amount;
	}
}

MSTree::MSTree()
{
	pathLen = 0;
}

void MSTree::Input(const vector<double>& vec)
{
	vGraph.push_back(vec);
}

void MSTree::Build()
{
	priority_queue<TreeEdge> pq;
	for (int i=0; i<vGraph.size(); i++)
	{
		for (int j=0; j<i; j++)
		{
			TreeEdge edge(i, j, vGraph[i][j]);
			pq.push(edge);
		}
	}
	UFSet ufset(vGraph.size());
	for (int i=0; i<vGraph.size()-1;)
	{
		TreeEdge edge = pq.top();
		pq.pop();
		int head = ufset.Find(edge.head);	
		int tail = ufset.Find(edge.tail);
		if (head != tail)
		{
			ufset.Union(head, tail);
			eMSTree.push_back(edge);
			pathLen += edge.len;
			++i;
		}
	}
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

void MSTree::Divide(vector<int>& vec1, vector<int>& vec2, double radii[], int cands[], const vector<double>& maxRadii)
{
	vector<vector<int>> table(vGraph.size());
	for (int i=0; i<eMSTree.size(); i++) {
		table[eMSTree[i].tail].push_back(eMSTree[i].head);
		table[eMSTree[i].head].push_back(eMSTree[i].tail);
	}

	bool *used = new bool[vGraph.size()];

	vector<int> temp1, temp2;
	for (int j=eMSTree.size()-1; j>=eMSTree.size()/2; j--)
	{
		TreeEdge edge = eMSTree[j];

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

		if (vec1.size()>=vGraph.size()*0.1 && vec2.size()>=vGraph.size()*0.1)
		{
			break;
		}
		else
		{
			if (j == eMSTree.size()-1)
			{
				temp1.assign(vec1.begin(), vec1.end());
				temp2.assign(vec2.begin(), vec2.end());
			}
			vec1.clear();
			vec2.clear();
		}
	}
	delete[] used;

	if (vec1.size() == 0)
	{
		assert(vec2.size() == 0);
		vec1.assign(temp1.begin(), temp1.end());
		vec2.assign(temp2.begin(), temp2.end());
	}

	radii[0] = radii[1] = MAXDOUBLE;

	for (int i=0; i<vec1.size(); i++) {
		double radius = 0;
		for (int j=0; j<vec1.size(); j++) {
			if (i == j) {
				continue;
			}
			double maxRadius = MAX(vGraph[vec1[i]][vec1[j]]+maxRadii[vec1[j]], maxRadii[vec1[i]]);
			if (maxRadius > radius) {
				radius = maxRadius;
			}
		}
		if (radius < radii[0]) {
			radii[0] = radius;
			cands[0] = vec1[i];
		}
	}

	for (int i=0; i<vec2.size(); i++) {
		double radius = 0;
		for (int j=0; j<vec2.size(); j++) {
			if (i == j) {
				continue;
			}
			double maxRadius = MAX(vGraph[vec2[i]][vec2[j]]+maxRadii[vec2[j]], maxRadii[vec2[i]]);
			if (maxRadius > radius) {
				radius = maxRadius;
			}
		}
		if (radius < radii[1]) {
			radii[1] = radius;
			cands[1] = vec2[i];
		}
	}
}