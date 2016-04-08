#include <vector>
#include <queue>
using namespace std;
#include "MXTnode.h"

class VPTree
{
public:
	VPTree(const MXTnode *_outerNode=NULL, int _vpNum=0): outerNode(_outerNode), vpNum(_vpNum), root(NULL), curDist(0) {}
	VPTree(const VPTree& tree);
	~VPTree() { 
		if (root) {
			delete root; 
		}
		outerNode = NULL;
	}

	void Build();
	void RangeQuery(const Object& target, const double radius, vector<int>& results, double dist);
	void KNNSearch(const Object& target, const int k, vector<int>& results, vector<double>& distances);
	void Dump();
	void Compress(char *buffer);
	void Decompress(const char *buffer);

	int GetVPNum() const { return vpNum; }
	void SetVPNum(int _vpNum) { vpNum = _vpNum; }

private:
	struct HeapItem {
		HeapItem(int _index, double _dist): index(_index), dist(_dist) {}
		bool operator<(const HeapItem& heapItem) const { return dist < heapItem.dist; }
		int index;
		double dist;
	};

	struct Node {
		Node(): index(0), id(0), radius(0), left(NULL), right(NULL) {}
		Node(const Node& node): index(node.index), id(node.id), radius(node.radius) {
			if (node.left) {
				left = new Node(*(node.left));
			} else {
				left = NULL;
			}
			if (node.right) {
				right = new Node(*(node.right));
			} else {
				right = NULL;
			}
		}
		~Node() { 
			if (left) {
				delete left; 
			}
			if (right) {
				delete right; 
			}
		}

		int index;
		int id;
		double radius;
		Node* left;
		Node* right;
	};

	Node* Build(vector<int>& indices, int lower, int upper, int id);
	void RangeQuery(const Node* node, const Object& target, const double radius, vector<int>& results);
	void KNNSearch(const Node* node, const Object& target, const int k, priority_queue<HeapItem>& heap);
	
	const MXTnode *outerNode;
	int vpNum;
	Node* root;  // linked list
	double curDist;  // for KNN
};

