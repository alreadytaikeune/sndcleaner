#include <stdlib.h>
#include <iostream>
template<typename T>
class List{
public:
	List();

	List(T d);

	~List(){
		// List* cur = this;
		// List* tmp;
		// while(cur){
		// 	delete cur->data;
		// 	tmp=cur->next;
		// 	delete cur;
		// 	cur=tmp;
		// }
	}

	int get_size(){
		return size;
	}

	void add_all(List<T>* l){
		if(size==0){
			data=l->data;
			next=l->next;
			size+=l->get_size();
			return;
		}
		List* cur = this;
		while(cur->next){
			cur=cur->next;
		}
		cur->next=l;
		size+=l->get_size();
	}

	void add(T data){
		List* l = new List(data);
		add_all(l);
	}

	void print(){
		if(get_size()==0){
			std::cout << "\n";
			return;
		}
			
		else{
			std::cout << data;
			if(next){
				std::cout << "->";
				next->print();
			}
			else{
				std::cout << "\n";
			}
				
		}
	}

	T data;
	int size;
	List<T> * next;


};

template <typename T> List<T>::List(T d)
{
  data=d;
  next=0;
  size=1;
}

template <typename T> List<T>::List()
{
	next=0;
	size=0;
}


