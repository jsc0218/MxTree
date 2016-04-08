#include <limits>
#include <iostream>
using namespace std;
#include "VPTree.h"

VPTree::VPTree(const VPTree& tree)
{
	outerNode = tree.outerNode;
	vpNum = tree.vpNum;
	root = new Node(*(tree.root));
	curDist = tree.curDist;
}

VPTree::Node* VPTree::Build(vector<int>& indices, int lower, int upper, int id)
{
	if (upper == lower) {
		return NULL;
	}

	Node* node = new Node();
	node->index = indices[lower];
	node->id = id;

	if (upper-lower > 1) {  // at least two elements
		// choose an arbitrary point and move it to the start as the pivot
		int i = rand()%(upper-lower) + lower;
		while (((MTentry *)((GiSTentry *)(*outerNode)[indices[i]]))->Key()->distance == 0) {
			i = rand()%(upper-lower) + lower;
		}
		swap(indices[lower], indices[i]);

		int median = (upper+lower)/2;  // for partitioning evenly

		struct DistanceComparator {
			DistanceComparator(const int _index, const MXTnode *_outerNode): index(_index), outerNode(_outerNode) {}
			bool operator()(const int a, const int b) { 
				return outerNode->GetObject(index).distance(outerNode->GetObject(a)) 
					< outerNode->GetObject(index).distance(outerNode->GetObject(b));
			}
			const int index;
			const MXTnode *outerNode;
		};
		nth_element(indices.begin()+lower+1, indices.begin()+median, indices.begin()+upper, DistanceComparator(indices[lower], outerNode));

		node->index = indices[lower];
		node->radius = outerNode->GetObject(indices[lower]).distance(outerNode->GetObject(indices[median]));
		node->left = Build(indices, lower+1, median, id*2+1);
		node->right = Build(indices, median, upper, id*2+2);
	}
	
	return node;
}

void VPTree::RangeQuery(const Node* node, const Object& target, const double radius, vector<int>& results)
{
	if (node == NULL) {
		return;
	}

	if (curDist==0 || fabs(curDist-((MTentry *)((GiSTentry *)(*outerNode)[node->index]))->Key()->distance) <= radius) {
		double distance = outerNode->GetObject(node->index).distance(target);
		if (distance <= radius) {
			results.push_back(node->index);
		}

		if (distance <= radius+node->radius) {
			RangeQuery(node->left, target, radius, results);
		}
		if (distance+radius >= node->radius) {
			RangeQuery(node->right, target, radius, results);
		}
	} else {
		RangeQuery(node->left, target, radius, results);
		RangeQuery(node->right, target, radius, results);
	}
}

void VPTree::KNNSearch(const Node* node, const Object& target, const int k, priority_queue<HeapItem>& heap)
{
	if (node == NULL) {
		return;
	}

	double distance = outerNode->GetObject(node->index).distance(target);

	// update the current distance
	if (distance < curDist) {
		if (heap.size() == k) {
			heap.pop();
		}
		heap.push(HeapItem(node->index, distance));
		if (heap.size() == k) {
			curDist = heap.top().dist;
		}
	}

	if (distance < node->radius) {
		KNNSearch(node->left, target, k, heap);
		if (node->radius-distance <= curDist) {
			KNNSearch(node->right, target, k, heap);
		}
	} else {
		KNNSearch(node->right, target, k, heap);
		if (distance-node->radius <= curDist) {
			KNNSearch(node->left, target, k, heap);
		}
	}
}

void VPTree::Build() 
{
	vector<int> indices;
	for (int i=0; i<vpNum; i++) {
		indices.push_back(i);
	}

	root = Build(indices, 0, indices.size(), 0);
}

void VPTree::RangeQuery(const Object& target, const double radius, vector<int>& results, double dist)
{
	curDist = dist;
	RangeQuery(root, target, radius, results);
}

void VPTree::KNNSearch(const Object& target, const int k, vector<int>& results, vector<double>& distances)
{
	priority_queue<HeapItem> heap;
	curDist = numeric_limits<double>::max();
	KNNSearch(root, target, k, heap);

	while(!heap.empty()) {
		results.push_back(heap.top().index);
		distances.push_back(heap.top().dist);
		heap.pop();
	}
	reverse(results.begin(), results.end());
	reverse(distances.begin(), distances.end());
}

void VPTree::Dump()
{
	queue<Node *> q;
	q.push(root);
	while (!q.empty()) {
		Node *node = q.front();
		cout<<node->index<<" "<<node->id<<" "<<node->radius<<endl;
		if (node->left) {
			q.push(node->left);
			cout<<"->"<<node->left->index<<" "<<node->left->id<<endl;
		} else {
			cout<<"->NULL"<<endl;
		}
		if (node->right) {
			q.push(node->right);
			cout<<"->"<<node->right->index<<" "<<node->right->id<<endl;
		} else {
			cout<<"->NULL"<<endl;
		}
		q.pop();
	}
}

void VPTree::Compress(char *buffer)
{
	queue<Node *> q;
	if (root) {
		q.push(root);
	}
	while (!q.empty()) {
		Node *node = q.front();
		memcpy(buffer+node->index*(sizeof(int)+sizeof(double)), &(node->id), sizeof(int));
		memcpy(buffer+node->index*(sizeof(int)+sizeof(double))+sizeof(int), &(node->radius), sizeof(double));
		if (node->left) {
			q.push(node->left);
		}
		if (node->right) {
			q.push(node->right);
		}
		q.pop();
	}
}

void VPTree::Decompress(const char *buffer)
{
	if (root) {
		delete root;
	}

	vector<Node *> vec;
	for (int i=0; i<vpNum; i++) {
		Node *node = new Node();
		node->index = i;
		memcpy(&(node->id), buffer+(sizeof(int)+sizeof(double))*i, sizeof(int));
		memcpy(&(node->radius), buffer+(sizeof(int)+sizeof(double))*i+sizeof(int), sizeof(double));
		vec.push_back(node);
	}

	struct IDComparator {
		bool operator()(const Node *a, const Node *b) { return a->id < b->id; }
	};
	sort(vec.begin(), vec.end(), IDComparator());
	
	if (!vec.empty()) {
		root = vec[0];
		for (int i=1, j=1, k=0; i<=vec[vec.size()-1]->id; i++) {
			if (vec[j]->id != i) {
				continue;
			}

			int parentID = ((i%2==0) ? (i-2)/2 : (i-1)/2);

			while (vec[k]->id != parentID) {
				++k;
			}

			if (i%2 == 0) {
				vec[k]->right = vec[j];
			} else {
				vec[k]->left = vec[j];
			}

			++j;
		}
	}
}
