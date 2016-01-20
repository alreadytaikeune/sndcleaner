#include "utils.h"
#include <limits.h>

/*
	TODO: to optimize with cache use
*/
unsigned binomial(unsigned n, unsigned k) {
	unsigned c = 1, i;
	if (k > n-k) k = n-k;  /* take advantage of symmetry */
	for (i = 1; i <= k; i++, n--) {
		if (c/i > UINT_MAX/n) return 0;  /* return 0 on overflow */
		c = c/i * n + c%i * n / i;  /* split c*n/i into (c/i*i + c%i)*n/i */
	}
	return c;
}

int bernoulli_cumulated_inv(unsigned n, float p, float a){
	if(p==0)
		return 0;
	int k=0;
	float tmp=0;
	while(tmp < a){
		tmp+=binomial(n,k)*pow(p,k)*pow(p,n-k);
		k++;
	}
	return k;

}