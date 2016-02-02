#include "distribution.h"

distribution* dist_alloc(int r){
	distribution* d = (distribution*) malloc(sizeof(distribution));
	d->res=r;
	d->N=-1;
	d->counters=(int*) calloc(r+1, sizeof(int));
	d->total=0;
}


distribution* dist_alloc(int r, double norm){
	distribution* d = (distribution*) malloc(sizeof(distribution));
	d->res=r;
	d->N=norm;
	d->counters=(int*) calloc(r+1, sizeof(int));
	d->total=0;
}


void dist_set_norm(distribution* d, double norm){
	d->N=norm;
}


void dist_incrementi16(distribution* d, int16_t* data, int len){
	assert(d->N >= 0);
	int idx;
	int r=d->res;
	for(int i=0; i<len; i++){
		if(data[i] != data[i])
			continue;		
		idx=(int) (r*data[i]/d->N);
		assert(idx <= r);
		d->counters[idx]++;
		d->total++;
	}
}


void dist_incrementi(distribution* d, int* data, int len){
	assert(d->N >= 0);
	int idx;
	int r=d->res;
	for(int i=0; i<len; i++){
		if(data[i] != data[i])
			continue;
		idx=(int) (r*data[i]/d->N);
		assert(idx <= r);
		d->counters[idx]++;
		d->total++;
	}
}

void dist_incrementf(distribution* d, float* data, int len){
	assert(d->N >= 0);
	int idx;
	int r=d->res;
	for(int i=0; i<len; i++){
		if(data[i] != data[i])
			continue;
		idx=(int) (r*data[i]/d->N);
		assert(idx <= r);
		d->counters[idx]++;
		d->total++;
	}
}

void dist_incrementd(distribution* d, double* data, int len){
	assert(d->N >= 0);
	int idx;
	int r=d->res;
	for(int i=0; i<len; i++){
		if(data[i] != data[i])
			continue;
		idx=(int) (r*data[i]/d->N);
		assert(idx <= r);
		d->counters[idx]++;
		d->total++;
	}
}

void dist_to_percentage(distribution* d, float per[]){
	assert(d->total > 0);
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