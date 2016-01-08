#include <cmath>

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


