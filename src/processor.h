#ifndef _PROCESSOR_H_
#define _PROCESSOR_H_
#include "mask.h"
#include "utils.h"

int zero_crossings(int16_t* data, int len);
double zero_crossing_rate(int16_t* data, int len);

void kl_divergence();
void bayesian_information_score();

/*
	Sum of the number of bits difference between the values pointed to by in1 and the values pointed to by in2.
	- byte_size is the number of bytes of the value pointed to by the void pointers
	- nb_bits is the number of bits starting from the LSB to consider in the error nb computation
	- len is the number of values on which to perform the bit difference, it should be lesser or equal to the 
		min length of in1 and in2.

	WARNING: to work, the offset should be less than 32, meaning that for 32 bits types, nb_bits should be
	greater than 0, and for 64 bits types nb_bits should be greater than 32. This is the case because bit shifts
	has undefined behaviour if the number of bits by which the value is shifted is greater than the bit width
	of the value.
	"Shifts on unsigned types are well defined (as long as the right operand is in non-negative and less than 
	the width of the left operand), and they always zero-fill."
*/
int bit_error(void* in1, void* in2, int byte_size, int nb_bits, int len);


float bit_error_ratio(void* in1, void* in2, int byte_size, int nb_bits, int len);



/*
*	Takes a spectrum as input and maps it to the mel scale.
	Arguments:
		- data: the spectrum to rescale
		- out: the previously allocated array for the mel spectrum, should be at least mel_nb double long
		- fft_size: half the size of the fft, e.g the size of the power spectrum.
		- mel_nb: the number of windows in the mel filterbank
		- freq_max: max frequency of the fft. Half of the sampling frequency, generally 22050Hz
*/
void to_mel_scale(double* data, double* out, int fft_size, int mel_nb, double freq_max);

/*
	Computes the histogram of the value distribution to determine the level of compression. The value range 
	is brought down to [0, 1]. This interval is divided into nb_bands subbands. For each frame the 
	corresponding subband is computed and the number of values in this subband, stored in the array bands, is
	incremented. The bands array must be 0 filled before the first call to this function.
*/
void compression_levels(int16_t* data, int len, int* bands, int nb_bands);


/*
*	Debbuging function to print the different triangular windows
*/
void print_mel_filterbank(int fft_size, int mel_nb, double freq_max);

/*
*	Mutli-threadable version of bit mask application without SIMD optimization
*   	
*/
void apply_mask(double** data, uint8_t** out, int w, int h, Mask* m, int mask_op);


int apply_mask_to_bit_value(double** data, void* out, int out_byte_size, int w, int h, Mask* m, int mask_op);

double root_mean_square(int16_t* data, int len);

/*
	Computes the p LPC coefficients on len samples of data. The array coefs should be at least of length p.
*/
double lpc_filter(int16_t* data, int len, int p, double* coefs);

float lpc_filterf(int16_t* data, int len, int p, float* coefs);

void autocorrelate(float Sn[], float Rn[], int Nsam, int order);

void levinson_durbin(float R[],	float lpcs[], int order);


void lpc_filter_optimized(int16_t Sn[], float a[], int Nsam, int order, float *E);

float lpc_from_data(float *data,float *lpci,int n,int m);

void inverse_filter(float Sn[], float a[], int Nsam, float res[], int order);

void synthesis_filter(float res[], float a[], int Nsam, int order,float Sn_[]);

int resample(int16_t* in, int16_t* out, int n, float fin, float fout, int off);


#endif