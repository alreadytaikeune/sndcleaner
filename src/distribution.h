#ifndef _DISTRIBUTION_H_
#define _DISTRIBUTION_H_
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct distribution{
	int   res;  // resolution of the distribution
	int*  counters; // counters
	int   total;// total number of samples
	double   N;    // normalization factor
} distribution;


distribution* dist_alloc(int r);

distribution* dist_alloc(int r, double norm);

void dist_set_norm(distribution* d, double norm);

void dist_incrementi16(distribution* d, int16_t* data, int len);

void dist_incrementi(distribution* d, int* data, int len);

void dist_incrementf(distribution* d, float* data, int len);

void dist_incrementd(distribution* d, double* data, int len);

void dist_to_percentage(distribution* d, float per[]);

void dist_print(distribution* d);

void dist_free(distribution* d);

double dist_get_ratio(distribution* d, int r);

#endif