#include <stdio.h>


typedef struct distribution{
	int   res;  // resolution of the distribution
	int*  counters; // counters
	int   total;// total number of samples
	double   N;    // normalization factor
} distribution;


distribution* dist_alloc(int r, double norm){
	distribution* d = (distribution*) malloc(sizeof(distribution));
	d->res=r;
	d->N=norm;
	d->counters=(int*) calloc(r, sizeof(int));
	d->total=0;
}


void dist_incrementi16(distribution* d, int16_t* data, int len){
	int idx;
	int r=d->res;
	for(int i=0; i<len; i++){
		idx=(int) (r*data[i]/(d->N+1));
		if(idx >= d->res)
			exit(1);
		d->counters[idx]++;
		d->total++;
	}
}


void dist_incrementi(distribution* d, int* data, int len){
	int idx;
	int r=d->res;
	for(int i=0; i<len; i++){
		idx=(int) (r*data[i]/(d->N+1));
		if(idx >= d->res)
			exit(1);
		d->counters[idx]++;
		d->total++;
	}
}

void dist_incrementf(distribution* d, float* data, int len){
	int idx;
	int r=d->res;
	for(int i=0; i<len; i++){
		idx=(int) (r*data[i]/(d->N+1));
		if(idx >= d->res)
			exit(1);
		d->counters[idx]++;
		d->total++;
	}
}

void dist_incrementd(distribution* d, double* data, int len){
	int idx;
	int r=d->res;
	for(int i=0; i<len; i++){
		idx=(int) (r*data[i]/(d->N+1));
		if(idx >= d->res)
			exit(1);
		d->counters[idx]++;
		d->total++;
	}
}

void dist_to_percentage(distribution* d, float per[]){
	for(int i=0; i<d->res; i++){
		per[i] = (float) d->counters[i]/d->total;
	}
}

void dist_print(distribution* d){
	printf("========================\n");
	printf("distribution at %p:\n", d);
	printf("\t resolution: %d\n", d->res);
	printf("\t total: %d\n", d->total);
	printf("\t normalization factor: %.4lf\n", d->N);
	for(int i=0; i<d->res; i++){
		printf("%.2f ", (float) d->counters[i]/d->total);
	}
	printf("\n");
	printf("========================\n");
}


void dist_free(distribution* d){
	free(d->counters);
	free(d);
}