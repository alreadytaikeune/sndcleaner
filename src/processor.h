#ifndef _PROCESSOR_H_
#define _PROCESSOR_H_
#include "mask.h"

void zero_crossings();
void spectral_flux();
void bit_error_rate();
void bit_error_ratio();
void kl_divergence();
void bayesian_information_score();

void to_mel_scale();

void compression_level();

/*
*	Mutli-threadable version of mask application without SIMD optimization
* 	data:
*   	
*/
void apply_mask(double** data, uint8_t** out, int w, int h, Mask* m, int mask_op);

#endif