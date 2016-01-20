#ifndef _CLEANSOUND_H_
#define _CLEANSOUND_H_
#include "mask.h"
#include "utils.h"
#include "processor.h"

void compute_bit_error_ratios(void* s1, void* s2,float* ratios, int byte_size, int nb_bits, int len1, int len2);

void compute_bit_ratios(void* s1, void* s2,float* ratios, int byte_size, int nb_bits, int len1, int len2);


int find_time_occurences(float**occurrences, double** data1, double** data2, int mel_size, 
	int c1, int c2, Mask* m, int fft_size, float sampling);

#endif