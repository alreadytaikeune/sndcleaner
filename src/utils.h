#ifndef _UTILS_H_
#define _UTILS_H_
#include <cmath>
#include <stdlib.h>

template<typename T>
int max(T* data, int len){
	T m = 0;
	for(T* it = data; it < data + len; ++it){
		if(*it > m)
			m = *it;
	}
	return m;
}

template<typename T>
int max_abs(T* data, int len){
	T m = 0;
	for(T* it = data; it < data + len; ++it){
		if(std::abs(*it) > m)
			m = std::abs(*it);
	}
	return m;
}

template<typename T>
void threshold(T* data, int len, T thresh){
	for(T* it = data; it < data + len; ++it){
		if(*it < thresh){
			*it=0;
		}
	}
}

template<typename T>
int local_maxima_zero(T* data, int len, int** indices, int* l_ind, int* nb){
	int k=0;
	int i;
	int toggle=0;
	T tmp=0;
	int tmp_idx;
	for(i=0;i<len;i++){
		if(data[i] > 0){
			if(!toggle)
				toggle=1;
			if(data[i] > tmp){
				tmp=data[i];
				tmp_idx=i;
			}
		}
		else{
			if(toggle){ // add the local maxima
				if(k>=*l_ind){
					*indices = (int*) realloc(*indices, (*l_ind+100)*sizeof(int)); // arbitrary... to change?
					if(! *indices){
						return -1;
					}
					*l_ind = (*l_ind)+100;
				}
				(*indices)[k]=tmp_idx;
				k++;
				toggle=0;
				tmp=0;
			}
		}
		
	}
	//shrink to fit data
	//*indices = (int*) realloc(*indices, k*sizeof(int));
	*nb=k; // change the value pointed by nb only if success
	return 0;
}

unsigned binomial(unsigned n, unsigned k);


template<typename T>
double mean(T* data, int len){ 
	T sum=0;
	for(T* it = data; it < data + len; ++it){
		sum+=*it;
	}
	return sum/((double) len);
}

#endif