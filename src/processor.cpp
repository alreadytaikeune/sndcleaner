#include "processor.h"
#include <stdio.h>
#include <cmath>
#include <string.h>
#include <SDL2/SDL.h>


/*

	bit_error_ratio uses the function __builtin_popcount which is a GCC specific function. If to compile
	with other compiler, may not work.















*/

enum Endianness {BIG, LITTLE, UNDEFINED};


static Endianness endianness = UNDEFINED;


void check_endianness(){
	int i = 1;
	char* it = (char *) &i;
	endianness = (*it == 1) ? LITTLE : BIG;
}




void zero_crossings(){};
void spectral_flux(){};



double bit_error_rate(void* in1, void* in2){


};


float bit_error_ratio(void* in1, void* in2, int byte_size, int nb_bits, int len){
	float b = (float) bit_error(in1, in2, byte_size, nb_bits, len);
	if(b==-1)
		return -1;
	return b/(nb_bits*len);
}


int bit_error(void* in1, void* in2, int byte_size, int nb_bits, int len){
	if(nb_bits > byte_size*8){
		printf("Error: mismatch dimension. Byte size: %d vs %d bits requested\n", byte_size, nb_bits);
		return -1;
	}

	if(endianness==UNDEFINED){
		check_endianness();
	}
	int offset = byte_size*8-nb_bits;
	unsigned int* uin1 = (unsigned int*) in1;
	unsigned int* uin2 = (unsigned int*) in2;
	int bit_err=0;
	unsigned int x;
	int byte_ratio = byte_size/sizeof(unsigned int);
	//printf("byte ratio is: %d \n", byte_ratio);
	if(byte_ratio==0 || byte_ratio > 2){
		printf("Unsupported byte ratio\n");
		return -1;
	}
	for(int i=0; i < len*byte_ratio;i++){
		if(byte_ratio==2){ 
			// void points to a 64 bits integer, the half to offset is the most significant part
			// which depends on the endianness
			if((endianness==LITTLE && i%2==1) || (endianness==BIG && i%2==0)){ 
				// We are dealing with the most significant part
				x = (*(uin1+i) << offset) ^ (*(uin2+i) << offset);
			}
			else{
				x = (*(uin1+i)) ^ (*(uin2+i));
			}
		}
		else{
			x = (*(uin1+i) << offset) ^ (*(uin2+i) << offset);
		}
		
		bit_err += __builtin_popcount(x);
	}
	
	return bit_err;
}

int bit_error_nb(void* in1, void* in2, int byte_size, int nb_bits, int len){

}



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
		//printf("start: %d, middle: %d, stop: %d, fmin: %lf, fmiddle: %lf, fmax:%lf\n", i_start, i_middle, i_stop, f_min, f_middle, f_max);
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


int apply_mask_to_bit_value(double** data, void* out, int out_byte_size, int w, int h, Mask* m, int mask_op){
	int needed_bytes = h/8;
	if(needed_bytes > out_byte_size){
		printf("Error: need %d bytes to map to int, but only %d pointed to \n", needed_bytes, out_byte_size);
		return -1;
	}

	memset(out, 0, out_byte_size*w);
	int i,j,k,l;
	double tmp;
	printf("w: %d, h: %d \n", w, h);
	int32_t* p32 = (int32_t*) out;
	int64_t* p64 = (int64_t*) out;
	if(mask_op==CROPPED){
		int start_i = m->pvt_y;
		int end_i = h-(m->msk_h-m->pvt_y-1);
		int start_j = m->pvt_x;
		int end_j = w-(m->msk_w-m->pvt_x-1);
		printf("start_i: %d, end_i: %d, start_j: %d, end_j %d \n", start_i, end_i, start_j, end_j);
		for(j=start_j;j<end_j;j++){
			p32 = (int32_t*) out+j*out_byte_size;
			p64 = (int64_t*) out+j*out_byte_size;
			for(i=start_i;i<end_i;i++){
				tmp=0.;
				for(k=0;k<m->msk_h;k++){
					for(l=0;l<m->msk_w;l++){
						tmp+=m->msk[m->msk_w-l-1][m->msk_h-k-1]*data[j+l-m->pvt_x][i+k-m->pvt_y];
					}
				}
				if(tmp > 0){
					if(out_byte_size==4){
						*p32 = *p32 | (1 << i);
					}
					else if(out_byte_size==8){
						*p64 = *p64 | (1 << i);
					}
				}	
			}
		}
	}
	return 0;
}