#ifndef _FILE_SET_H_
#define _FILE_SET_H_
#include <set>
#include <string>
#include "boost/filesystem.hpp"
#include <iterator>
#include <iostream>

namespace bf=boost::filesystem;




typedef struct iterator_stack{
	bf::directory_iterator* it;
	iterator_stack* stack;
} iterator_stack;


void is_init(iterator_stack* is){
	is->it=NULL;
	is->stack=NULL;
}



int is_pop(iterator_stack** is, bf::directory_iterator** it){
	if(!is)
		return -1;
	*it=(*is)->it;
	iterator_stack* tmp=*is;
	*is=(*is)->stack;
	std::cout << "popped " << *it << " from " << tmp << std::endl;
	free(tmp);
	std::cout << "new head at: " << *is << std::endl;
	return 0;
}


int is_peek(iterator_stack* is, bf::directory_iterator** it){
	if(!is)
		return -1;
	*it=is->it;
	return 0;
}

void is_put(iterator_stack** is, bf::directory_iterator* it){
	std::cout << "put in " << *is << " iterator at: " << it << std::endl;
	iterator_stack* tmp = (iterator_stack*) malloc(sizeof(iterator_stack)); // freed in pop
	tmp->stack=*is;
	tmp->it=it;
	*is=tmp;
	std::cout << "new head at: " << *is << std::endl;
}


bool is_is_empty(iterator_stack* is, bf::directory_iterator end_itr){
	return is==NULL;
}



class FileSetIterator : boost::iterator_facade<bf::directory_iterator, std::string, boost::single_pass_traversal_tag>{
	public:
		FileSetIterator(){
			std::cout << "default constructor" << std::endl;
			recursive=false;
			itr=new bf::directory_iterator();
			// is=(iterator_stack*) malloc(sizeof(iterator_stack));
			// is_init(is);

		}

		FileSetIterator(const bf::path & dir_path){
			itr=new bf::directory_iterator(dir_path); // deleted when iterating
			recursive=true;
			// is=(iterator_stack*) malloc(sizeof(iterator_stack));
			// is_init(is);
			is_put(&is, itr);
		}

		FileSetIterator(const bf::path & dir_path, bool rec){
			itr=new bf::directory_iterator(dir_path);
			recursive=rec;
			// is=(iterator_stack*) malloc(sizeof(iterator_stack));
			// is_init(is);
			is_put(&is, itr);

		}

		~FileSetIterator(){
			if(!is_is_empty(is, end_itr)){
				while(is_pop(&is, &itr)>=0){
					std::cout << "iterator popped: " << itr << std::endl;
					delete itr;
				}
			}
			//free(is);		
		}

		void move_to_first_file(){
			while(is_peek(is, &itr) >= 0){
				if(!is_directory((*itr)->status())){
					return;
				}
				else{
					is_put(&is, new bf::directory_iterator((*itr)->path()));
				}
			}
		}

		// Pre-increment
		FileSetIterator& operator++(){
			bool to_pop=true;
			while(is_peek(is, &itr) >= 0){
				to_pop=true;
				//std::cout << "head is " << (*itr)->path() << std::endl;
				// depth search strategy
				while(*itr!=end_itr){
					if(is_directory((*itr)->status()) && recursive){
						std::cout << "directory: " << (*itr)->path() << std::endl; 
						is_put(&is, new bf::directory_iterator((*itr)->path()));
						++(*itr);
						to_pop=false;
						break; // go to the previous while loop and deal with this directory
					}
					else if(is_directory((*itr)->status())){
						++(*itr);
						continue;
					}
					++(*itr);
					if(*itr==end_itr){
						to_pop=true;
						break;
					}
						
					return *this;
				}
				
				if(to_pop){ 
					is_pop(&is, &itr);
					delete itr;
				}
				
			}	
			*itr=end_itr;
	        return *this;
   		}

   		// Post-increment
	    FileSetIterator operator++ (int) 
	    {
	    	while(is_peek(is, &itr) >= 0){
				// depth search strategy
				while(*itr != end_itr){
					(*itr)++;
					if(is_directory((*itr)->status()) && recursive){

						is_put(&is, new bf::directory_iterator((*itr)->path()));
						break; // go to the previous while loop and deal with this directory
					}
					return *this;
				}
				is_pop(&is, &itr);
				delete itr;
			}
	        return *this; 
	    }

	    bool operator== (const FileSetIterator& other) const{
	    	return (*itr==*other.itr);
	    }

		bool operator!=(const FileSetIterator& other) const{
			return (*itr != *other.itr);
		}

		std::string operator* () const{
        	assert(*itr != end_itr);
        	return ((*itr)->path()).string();
    	}

    	std::string operator-> () const{
        	assert(*itr != end_itr);
        	return ((*itr)->path()).string();
    	}

	private:
		bool recursive=true;
		bf::directory_iterator* itr;
		bf::directory_iterator end_itr;
		iterator_stack* is=NULL;
};

class FileSet{
	public:
		FileSet(const bf::path & dir_path, bool rec){
			root_path=dir_path;
			recursive=rec;
		}
		FileSetIterator begin(){
			bf::path &arg=root_path;
			FileSetIterator f(arg, recursive);
			f.move_to_first_file();
			return f;
		}

		FileSetIterator end(){
			return FileSetIterator();
		}

	private:
		//set<std::string> filenames;
		bf::path root_path = bf::path(".");
		bool recursive;

};

#endif