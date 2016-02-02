#include "processor.h"
#include <stdexcept>
#include "cleansound.h"
#include <iostream>
/*
	Computes iteratively the bits error ratio between s1 and a moving window on s2.
	s1 and s2 have been obtained by binarizing a spectrogram
*/
void compute_bit_error_ratios(void* s1, void* s2, float* ratios, int byte_size, int nb_bits, int len1, int len2){
	if(len2 < len1){
		 throw std::invalid_argument( "received negative value" );
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

}

/*
	Computes iteratively the bits correctness ratio between s1 and a moving window on s2.
	s1 and s2 have been obtained by binarizing a spectrogram
*/
void compute_bit_ratios(void* s1, void* s2, float* ratios, int byte_size, int nb_bits, int len1, int len2){
	if(len2 < len1){
		 throw std::invalid_argument( "received negative value" );
	}
	void* bit_set;
	int i=0;
	int32_t* ps1_32=(int32_t*) s1;
	int64_t* ps1_64=(int64_t*) s1;
	int32_t* ps2_32=(int32_t*) s2;
	int64_t* ps2_64=(int64_t*) s2;
	if(byte_size==4){
		for(i=0; i+len1<len2; i++){
			ratios[i] = 1-bit_error_ratio((void*) ps1_32, (void*)(ps2_32+i), byte_size, nb_bits, len1);
		}
	}
	else{
		for(i=0; i+len1<len2; i++){
			ratios[i] = 1-bit_error_ratio((void*)ps1_64, (void*)(ps2_64+i), byte_size, nb_bits, len1);
		}
	}

}

/*
	Finds the occurrences of data1 in data2 using the method shown in the article 
	"Audio fingerprinting systems for broadcast streams"
*/
int find_time_occurences(float** occurrences, double** data1, double** data2, int mel_size, 
	int c1, int c2, Mask* m, int fft_size, float sampling){
	
	int byte_size = 4*(mel_size/32+1);
	const int nb_bits = mel_size;

	void* bin1 = calloc(c1, byte_size);
	void* bin2 = calloc(c2, byte_size);
	float* ratios = (float*) calloc(c2-c1, sizeof(float)); 


	apply_mask_to_bit_value(data1, bin1, byte_size, c1, nb_bits, m, CROPPED);
	apply_mask_to_bit_value(data2, bin2, byte_size, c2, nb_bits, m, CROPPED);
	int j;
	compute_bit_ratios((void*) bin1, (void*) bin2, ratios, byte_size, nb_bits, c1, c2);
	threshold(ratios, c2-c1, 0.75f);
	int l_ind=10; // length indices
	int* indices = (int*) malloc(l_ind*sizeof(int));
	int nb=0;
	int s = local_maxima_zero(ratios, c2-c1, &indices, &l_ind, &nb);
	if(s < 0){
		 return -1;
	}
	float time_step = fft_size/sampling;
	*occurrences = (float*) realloc(*occurrences, nb*sizeof(float));
	for(int i=0; i<nb; i++){
		(*occurrences)[i] = (float) time_step*indices[i];
	}

	free(bin1);
	free(bin2);
	free(ratios);
	free(indices);
	return nb;
}



