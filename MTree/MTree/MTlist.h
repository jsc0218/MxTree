#ifndef MTLIST_H
#define MTLIST_H

#include <ostream>
using namespace std;
#include <assert.h>

template<class T> struct GiSTlistnode;

// this class extends the GiSTlist by adding /*the count and*/ some access functions
template <class T>
class MTlist {
public:
	MTlist (): front(NULL), rear(NULL) {}
	int IsEmpty () const { return front==NULL; }

	T First () const {
		assert (front != NULL);
		return front->entry;
	}

	T Last () const {
		assert (rear != NULL);
		return rear->entry;
	}

	T RemoveFirst () {
		assert (front != NULL);
		GiSTlistnode<T> *temp = front;
		T e = front->entry;

		front = front->next;
		if (front) {
			front->prev = NULL;
		} else {
			rear = NULL;
		}
		delete temp;
		return e;
	}

	T RemoveLast () {
		assert (rear != NULL);
		GiSTlistnode<T> *temp = rear;

		T e = rear->entry;
		rear = rear->prev;
		if (rear) {
			rear->next = NULL;
		} else {
			front = NULL;
		}
		delete temp;
		return e;
	}

	virtual void Insert (T entry) { Append (entry); }

#ifdef PRINTING_OBJECTS
	void Print (ostream& os) const {
		GiSTlistnode<T> *temp = front;
		os << "List entries:"<<endl;
		while (temp) {
			os << "\t" << temp->entry;
			temp = temp->next;
		}
		os << endl;
	}
#endif

protected:
	void Append (T entry) {
		GiSTlistnode<T> *temp = new GiSTlistnode<T>;

		temp->entry = entry;
		temp->prev = rear;
		temp->next = NULL;
		if (front == NULL) {
			front = temp;
		} else {
			rear->next = temp;
		}
		rear = temp;
	}

	GiSTlistnode<T> *front, *rear;
};

// this class implements an ordered list
template <class T>
class MTorderedlist: public MTlist<T> {
public:
	MTorderedlist (int func(T, T)): MTlist<T>(), compare(func) { }

	// insert in order
	virtual void Insert (T entry) {
		GiSTlistnode<T> *temp = this->front;
		while (temp && compare(temp->entry, entry)<0) {
			temp = temp->next;
		}
		if (temp) {
			InsertBefore (temp, entry);
		} else {
			Append (entry);
		}
	}

private:
	void InsertAfter (GiSTlistnode<T> *node, T entry) {
		GiSTlistnode<T> *temp = new GiSTlistnode<T>;
		temp->entry = entry;
		temp->prev = node;
		temp->next = node->next;
		node->next = temp;
		if (this->rear == node) {
			this->rear = temp;
		} else {
			temp->next->prev = temp;
		}
	}

	void InsertBefore (GiSTlistnode<T> *node, T entry) {
		GiSTlistnode<T> *temp = new GiSTlistnode<T>;
		temp->entry = entry;
		temp->prev = node->prev;
		temp->next = node;
		node->prev = temp;
		if (this->front == node) {
			this->front = temp;
		} else {
			temp->prev->next = temp;
		}
	}

	int (*compare) (T, T);	// should return <0 if T1<T2
};

#endif
