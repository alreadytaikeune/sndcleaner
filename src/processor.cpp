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
void apply_mask(double** data, int** out, int w, int h, Mask* m){
	int i,j,k,l;
	double tmp;
	if(m->msk_type==0){
		int start_i = m->pvt_y;
		int end_i = h-(m->msk_h-m->pvt_y-1);
		int start_j = m->pvt_x;
		int end_j = w-(m->msk_w-m->pvt_x-1);
		for(i=start_i;i<end_i;i++){
			for(j=start_j;j<end_j;j++){
				tmp=0.;
				for(k=0;k<m->msk_h;k++){
					for(l=0;l<m->msk_w;l++){
						tmp+=m->msk[m->msk_w-l-1][m->msk_h-k-1]*data[j+l-m->pvt_x][i+k-m->pvt_y];
					}
				}
				out[j][i] = tmp > 0 ? 1 : 0;
			}
		}


	}
}