#include "processor.h"
#include <stdio.h>
#include <cmath>
#include <string.h>
#include <SDL2/SDL.h>
void zero_crossings(){};
void spectral_flux(){};
void bit_error_rate(){};
void bit_error_ratio(){};
void kl_divergence(){};
void bayesian_information_score(){};

void to_mel_scale(double* data, double* out, int fft_size, int mel_nb, double freq_max){
	double mel_max = 1127*log(1+freq_max/700);
	memset(out, 0, mel_nb);
	double mel_step = mel_max/mel_nb;
	double freq_step=freq_max/fft_size;
	int i_start=0, i_middle=0, i_stop=0;
	double f_min=0.;
	double f_middle=700*(pow(10, mel_step/2595)-1);
	i_middle=(int) f_middle/freq_step;
	double f_max;
	double i;
	for(int k=0;k<mel_nb-1;k++){
		f_max=700*(pow(10, (k+2)*mel_step/2595)-1);
		i_stop=(int) f_max/freq_step;
		// printf("start: %d, middle: %d, stop: %d, fmin: %lf, fmiddle: %lf, fmax:%lf\n", i_start, i_middle, i_stop, f_min, f_middle, f_max);
		for(i=i_start; i<i_stop; i++){
			if(i<=i_middle){
				out[k]+=data[(int)i]*(i-i_start)/(i_middle-i_start);
			}
			else{
				out[k]+=data[(int)i]*(i_stop-i)/(i_stop-i_middle);
			}
		}
		f_min=f_middle;
		i_start=i_middle;
		f_middle=f_max;		
		i_middle=i_stop;
	}
	//printf("\n");
}


void print_mel_filterbank(int fft_size, int mel_nb, double freq_max){
	SDL_Window* window = SDL_CreateWindow("Spectrum",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          fft_size, 200,
                          0);
  	if(!window){
  		printf("Could not create screen SDL: %s\n", SDL_GetError());
    	return;
  	}
  	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
  	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	// Clear window
    SDL_RenderClear(renderer);
  	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    SDL_Rect r;
    r.w=1;
    r.h=1;

    double mel_max = 1127*log(1+freq_max/700);
	double mel_step = mel_max/mel_nb;
	double freq_step=freq_max/fft_size;
	int i_start=0, i_middle=0, i_stop=0;
	double f_min=0.;
	double f_middle=700*(pow(10, mel_step/2595)-1);
	i_middle=(int) f_middle/freq_step;
	double f_max;
	double i;
	for(int k=0;k<mel_nb-1;k++){
		f_max=700*(pow(10, (k+2)*mel_step/2595)-1);
		i_stop=(int) f_max/freq_step;
		// printf("start: %d, middle: %d, stop: %d, fmin: %lf, fmiddle: %lf, fmax:%lf\n", i_start, i_middle, i_stop, f_min, f_middle, f_max);
		for(i=i_start; i<i_stop; i++){
			r.x=i;
			if(i<=i_middle){
				r.y=200-200*(i-i_start)/(i_middle-i_start);
			}
			else{
				r.y=200-200*(i_stop-i)/(i_stop-i_middle);
			}
			SDL_RenderFillRect(renderer, &r);
		}
		f_min=f_middle;
		i_start=i_middle;
		f_middle=f_max;		
		i_middle=i_stop;
	}

	SDL_RenderPresent(renderer);
	SDL_Delay(5000);
	SDL_DestroyRenderer(renderer);

	SDL_DestroyWindow(window);
	SDL_Quit();


}


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