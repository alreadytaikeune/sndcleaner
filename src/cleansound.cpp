#include "processor.h"
/*
	Tries to find s1 in s2
*/
int find_stream_index(void* s1, void* s2, float* ratios, int byte_size, int nb_bits, int len1, int len2){
	if(len2 < len1){
		return -1;
	}
	void* bit_set;
	int i=0;
	int32_t* ps1_32=(int32_t*) s1;
	int64_t* ps1_64=(int64_t*) s1;
	int32_t* ps2_32=(int32_t*) s2;
	int64_t* ps2_64=(int64_t*) s2;
	if(byte_size==4){
		for(i=0; i+len1<len2; i++){

			ratios[i] = bit_error_ratio((void*) ps1_32, (void*)(ps2_32+i), byte_size, nb_bits, len1);
		}
	}
	else{
		for(i=0; i+len1<len2; i++){
			ratios[i] = bit_error_ratio((void*)ps1_64, (void*)(ps2_64+i), byte_size, nb_bits, len1);
		}
	}

	return 0;
}