#ifndef _UTILS_H_
#define _UTILS_H_
#include <cmath>
#include <stdlib.h>

template<typename T>
T max(T* data, int len){
	T m = 0;
	for(T* it = data; it < data + len; ++it){
		if(*it > m)
			m = *it;
	}
	return m;
}

template<typename T>
T max_abs(T* data, int len){
	T m = 0;
	for(T* it = data; it < data + len; ++it){
		if(std::abs(*it) > m)
			m = std::abs(*it);
	}
	return m;
}


template<typename T>
T sum(T* data, int len){
	T m = 0;
	for(T* it = data; it < data + len; ++it){
		m += *it;
	}
	return m;
}

template<typename T, typename U>
void apply_coef(T* in, U* out, U coef, int len){
	for(int i=0;i<len;i++){
		out[i] = in[i]*coef;
	}
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


template<typename T>
double std_deviation(T* data, int len){
	T m = mean(data, len);
	double v=0;
	for(int i=0;i<len;i++){
		v+=pow(data[i]-m, 2);
	}
	return std::sqrt(v);
}

template<typename T>
double compute_centroid(T* data, int len, int order){
	double centroid=0;
	double s= (double) sum(data,len);
	for(int i=0;i<len;i++){
		centroid+=i*pow(data[i],order);
	}
	return centroid/s;
}


double solve_linear(double* matrix, double* coefs, double* vector, const int dim);

float solve_linearf(float* matrix, float* coefs, float* vector, const int dim);

#endif