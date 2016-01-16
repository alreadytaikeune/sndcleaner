#include "processor.h"
#include <stdio.h>
void zero_crossings(){};
void spectral_flux(){};
void bit_error_rate(){};
void bit_error_ratio(){};
void kl_divergence(){};
void bayesian_information_score(){};

void to_mel_scale(){};

void compression_level(){};

/*
*	Mutli-threadable version of mask application without SIMD optimization
* 	data:
*   	
*/
void apply_mask(double** data, uint8_t** out, int w, int h, Mask* m, int mask_op){
	int i,j,k,l;
	double tmp;
	printf("w: %d, h: %d \n", w, h);
	if(mask_op==CROPPED){
		int start_i = m->pvt_y;
		int end_i = h-(m->msk_h-m->pvt_y-1);
		int start_j = m->pvt_x;
		int end_j = w-(m->msk_w-m->pvt_x-1);
		printf("start_i: %d, end_i: %d, start_j: %d, end_j %d \n", start_i, end_i, start_j, end_j);
		for(i=start_i;i<end_i;i++){
			for(j=start_j;j<end_j;j++){
				tmp=0.;
				//printf("i:%d\tj:%d\n", i, j);
				for(k=0;k<m->msk_h;k++){
					for(l=0;l<m->msk_w;l++){
						tmp+=m->msk[m->msk_w-l-1][m->msk_h-k-1]*data[j+l-m->pvt_x][i+k-m->pvt_y];
						//printf("(%d,%d)x(%d,%d) ",m->msk_w-l-1, m->msk_h-k-1, j+l-m->pvt_x, i+k-m->pvt_y);
					}
				}
				out[j][i] = tmp > 0 ? 1 : 0;
				// if(out[j][i] > 0){
				// 	printf("%d,%d: 1\n", j, i);
				// }
			}
		}
	}
}