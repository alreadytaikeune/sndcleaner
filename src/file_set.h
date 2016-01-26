#include <set>
#include <string>
#include "boost/filesystem.hpp"
#include <iterator>
using namespace std;
using namespace boost;
using namespace boost::filesystem;




typedef struct iterator_stack{
	directory_iterator* it;
	iterator_stack* stack;
} iterator_stack;


void is_init(iterator_stack* is){
	memset(is, 0, sizeof(iterator_stack));
}

int is_pop(iterator_stack* is, directory_iterator* it){
	if(!is)
		return -1;
	it=is->it;
	iterator_stack* tmp=is;
	is=is->stack;
	free(tmp);
	return 0;
}


int is_peek(iterator_stack* is, directory_iterator* it){
	if(!is)
		return -1;
	it=is->it;
	return 0;
}

void is_put(iterator_stack* is, directory_iterator* it){
	iterator_stack* tmp = (iterator_stack*) malloc(sizeof(iterator_stack));
	tmp->stack=is;
	tmp->it=it;
	is=tmp;
}


bool is_empty(iterator_stack* is, directory_iterator end_itr){
	return *(is->it) == end_itr;
}



class FileSetIterator : iterator_facade<directory_iterator, string, single_pass_traversal_tag>{
	public:
		FileSetIterator(){
			recursive=false;
			is_init(&is);
		}

		FileSetIterator(const path & dir_path){
			itr=new directory_iterator(dir_path);
			recursive=true;
			is_init(&is);
			is_put(&is, itr);
		}

		FileSetIterator(const path & dir_path, bool rec){
			itr=new directory_iterator(dir_path);
			recursive=rec;
			is_init(&is);
			is_put(&is, itr);

		}

		// Pre-increment
		FileSetIterator& operator++(){
			while(is_peek(&is, itr) >= 0){
				// depth search strategy
				while(*itr != end_itr){
					++(*itr); 
					if(is_directory((*itr)->status()) && recursive){

						is_put(&is, new directory_iterator((*itr)->path()));
						break; // go to the previous while loop and deal with this directory
					}
					return *this;
				}
				is_pop(&is, itr);
				delete itr;
			}
			
	        return *this;
   		}

   		// Post-increment
	    FileSetIterator operator++ (int) 
	    {
	        (*itr)++;
	        return *this; 
	    }


	private:
		bool recursive=true;
		directory_iterator* itr;
		directory_iterator end_itr;
		iterator_stack is;
};

class FileSet{
	public:
		FileSet(const path & dir_path, bool rec);


	private:
		set<string> filenames;
		bool reccursive;

};