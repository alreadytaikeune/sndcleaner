#ifndef _PROCESSOR_H_
#define _PROCESSOR_H_
#include "mask.h"

void zero_crossings();
void spectral_flux();
void bit_error_rate();
void bit_error_ratio();
void kl_divergence();
void bayesian_information_score();

void to_mel_scale(double* data, double* out, int fft_size, int mel_nb, double freq_max);

void compression_level();

void print_mel_filterbank(int fft_size, int mel_nb, double freq_max);

/*
*	Mutli-threadable version of mask application without SIMD optimization
* 	data:
*   	
*/
void apply_mask(double** data, uint8_t** out, int w, int h, Mask* m, int mask_op);

#endif