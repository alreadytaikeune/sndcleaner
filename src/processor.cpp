#include "processor.h"
#include <stdio.h>
#include <cmath>
#include <string.h>
#include <SDL2/SDL.h>
#include "errors.h"
#include <iostream>
#include "window.h"


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




int zero_crossings(int16_t* data, int len){
	int out=0;
	int prev = data[0] > 0 ? 1 : -1;
	for(int i=1; i<len; i++){
		if(data[i]*prev < 0){
			out++;
			prev=-prev;
		}
	}
	return out;
}

double zero_crossing_rate(int16_t* data, int len){
	return zero_crossings(data, len)/((double) len);
}


void spectral_flux(){};

double root_mean_square(int16_t* data, int len){
	double out=0.;
	for(int i=0; i<len;i++){
		out+= (double) pow(data[i], 2);
	}
	return sqrt(out/len);
}

float bit_error_ratio(void* in1, void* in2, int byte_size, int nb_bits, int len){
	float b = (float) bit_error(in1, in2, byte_size, nb_bits, len);
	if(b==-1)
		return -1;
	return b/((nb_bits-1)*len);
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


void compression_levels(int16_t* data, int len, int* bands, int nb_bands){
	float max = (float) max_abs(data, len)+1;
	int band;
	for(int i=0;i<len;i++){
		band = (int)(nb_bands*std::abs(data[i])/max);
		bands[band]++;
	}
}

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
			p32++;
			p64++;	
		}
	}
	else if(mask_op==WRAPPED){
		throw NotImplementedException("");
	}
	else if(mask_op==EXTENDED){
		throw NotImplementedException("");
	}
	return 0;
}

double lpc_filter(int16_t* data, int len, int p, double* coefs){
	double* matrix = (double*) malloc(p*p*sizeof(double));
	int i,j,k;
	double s=0;

	double* c = (double*) malloc(p*sizeof(double));
	for(k=0;k<p;k++){
		s=0.;
		for(i=p;i<len;i++){
			s+=(double) data[i]*data[i-k-1];
		}
		c[k]=s;
		//std::cout << s/1.0e8 << "\n";
	}
	for(j=0;j<p;j++){
		for(k=0;k<p;k++){
			s=0.; // s is matrix's k,j coefficient 
			for(i=p;i<len;i++){
				s+=(double) data[i-k-1]*data[i-j-1];
			}
			matrix[j*p+k]=s;
			//std::cout << s/1.0e8 << "\t";
		}
		//std::cout << "\n";
	}
	double error = solve_linear(matrix, coefs, c, p);
	free(matrix);
	free(c);
	return error;
}


float lpc_filterf(int16_t* data, int len, int p, float* coefs){
	float* matrix = (float*) malloc(p*p*sizeof(float));
	int i,j,k;
	float s=0;

	float* c = (float*) malloc(p*sizeof(float));
	for(k=0;k<p;k++){
		s=0.;
		for(i=p;i<len;i++){
			s+=(float) data[i]*data[i-k-1];
		}
		c[k]=s;
		//std::cout << s/1.0e8 << "\n";
	}
	for(j=0;j<p;j++){
		for(k=0;k<p;k++){
			s=0.; // s is matrix's k,j coefficient 
			for(i=p;i<len;i++){
				s+=(float) data[i-k-1]*data[i-j-1];
			}
			matrix[j*p+k]=s;
			//std::cout << s/1.0e8 << "\t";
		}
		//std::cout << "\n";
	}
	float error = solve_linearf(matrix, coefs, c, p);
	free(matrix);
	free(c);
	return error;
}


/*---------------------------------------------------------------------------*\

  lpc_filter_optimize()

  This function takes a frame of samples, and determines the linear
  prediction coefficients for that frame of samples.

\*---------------------------------------------------------------------------*/

void lpc_filter_optimized(
  int16_t Sn[],	/* Nsam samples with order sample memory */
  float a[],	/* order+1 LPCs with first coeff 1.0 */
  int Nsam,	/* number of input speech samples */
  int order,	/* order of the LPC analysis */
  float *E	/* residual energy */
)
{
  float Wn[Nsam];	/* windowed frame of Nsam speech samples */
  float R[order+1];	/* order+1 autocorrelation values of Sn[] */
  int i;
  for(int k=0; k<Nsam; k++){
  	Wn[k]=(float) Sn[k]/32578;
  }
  autocorrelate(Wn,R,Nsam,order);

  levinson_durbin(R,a,order);

  *E = 0.0;
  for(i=0; i<=order; i++)
    *E += a[i]*R[i];
  if (*E < 0.0)
    *E = 1E-12;
}

/*---------------------------------------------------------------------------*\

  autocorrelate()

  Finds the first P autocorrelation values of an array of windowed speech
  samples Sn[].

\*---------------------------------------------------------------------------*/

void autocorrelate(
  float Sn[],	/* frame of Nsam windowed speech samples */
  float Rn[],	/* array of P+1 autocorrelation coefficients */
  int Nsam,	/* number of windowed samples to use */
  int order	/* order of LPC analysis */
)
{
  int i,j;	/* loop variables */

  for(j=0; j<order+1; j++) {
    Rn[j] = 0.0;
    for(i=0; i<Nsam-j; i++)
      Rn[j] += Sn[i]*Sn[i+j];
  }
}

/*---------------------------------------------------------------------------*\

  levinson_durbin()

  Given P+1 autocorrelation coefficients, finds P Linear Prediction Coeff.
  (LPCs) where P is the order of the LPC all-pole model. The Levinson-Durbin
  algorithm is used, and is described in:

    J. Makhoul
    "Linear prediction, a tutorial review"
    Proceedings of the IEEE
    Vol-63, No. 4, April 1975

\*---------------------------------------------------------------------------*/

void levinson_durbin(
  float R[],		/* order+1 autocorrelation coeff */
  float lpcs[],		/* order+1 LPC's */
  int order		/* order of the LPC analysis */
)
{
  float E[order+1];
  float k[order+1];
  float a[order+1][order+1];
  float sum;
  int i,j;				/* loop variables */

  E[0] = R[0];				/* Equation 38a, Makhoul */

  for(i=1; i<=order; i++) {
    sum = 0.0;
    for(j=1; j<=i-1; j++)
      sum += a[i-1][j]*R[i-j];
    k[i] = -1.0*(R[i] + sum)/E[i-1];	/* Equation 38b, Makhoul */
    // if (fabsf(k[i]) > 1.0)
    //   k[i] = 0.0;

    a[i][i] = k[i];

    for(j=1; j<=i-1; j++)
      a[i][j] = a[i-1][j] + k[i]*a[i-1][i-j];	/* Equation 38c, Makhoul */

    E[i] = (1-k[i]*k[i])*E[i-1];		/* Equation 38d, Makhoul */
  }

  for(i=1; i<=order; i++)
    lpcs[i] = a[order][i];
  lpcs[0] = 1.0;
}

/*---------------------------------------------------------------------------*\

  inverse_filter()

  Inverse Filter, A(z).  Produces an array of residual samples from an array
  of input samples and linear prediction coefficients.

  The filter memory is stored in the first order samples of the input array.

\*---------------------------------------------------------------------------*/

void inverse_filter(
  float Sn[],	/* Nsam input samples */
  float a[],	/* LPCs for this frame of samples */
  int Nsam,	/* number of samples */
  float res[],	/* Nsam residual samples */
  int order	/* order of LPC */
)
{
  int i,j;	/* loop variables */

  for(i=0; i<Nsam; i++) {
    res[i] = 0.0;
    for(j=0; j<=order; j++)
      res[i] += Sn[i-j]*a[j];
  }
}

/*---------------------------------------------------------------------------*\

 synthesis_filter()

 C version of the Speech Synthesis Filter, 1/A(z).  Given an array of
 residual or excitation samples, and the the LP filter coefficients, this
 function will produce an array of speech samples.  This filter structure is
 IIR.

 The synthesis filter has memory as well, this is treated in the same way
 as the memory for the inverse filter (see inverse_filter() notes above).
 The difference is that the memory for the synthesis filter is stored in
 the output array, wheras the memory of the inverse filter is stored in the
 input array.

 Note: the calling function must update the filter memory.

\*---------------------------------------------------------------------------*/

void synthesis_filter(
  float res[],	/* Nsam input residual (excitation) samples */
  float a[],	/* LPCs for this frame of speech samples */
  int Nsam,	/* number of speech samples */
  int order,	/* LPC order */
  float Sn_[]	/* Nsam output synthesised speech samples */
)
{
  int i,j;	/* loop variables */

  /* Filter Nsam samples */

  for(i=0; i<Nsam; i++) {
    Sn_[i] = res[i]*a[0];
    for(j=1; j<=order; j++)
      Sn_[i] -= Sn_[i-j]*a[j];
  }
}

float lpc_from_data(float *data,float *lpci,int n,int m){
  double *aut= (double *) malloc(sizeof(double)*(m+1));
  double *lpc= (double *) malloc(sizeof(double)*(m));
  double error;
  double epsilon;
  int i,j;

  /* autocorrelation, p+1 lag coefficients */
  j=m+1;
  while(j--){
    double d=0; /* double needed for accumulator depth */
    for(i=j;i<n;i++)d+=(double)data[i]*data[i-j];
    aut[j]=d;
  }

  /* Generate lpc coefficients from autocorr values */

  /* set our noise floor to about -100dB */
  error=aut[0] * (1. + 1e-10);
  epsilon=1e-9*aut[0]+1e-10;

  for(i=0;i<m;i++){
    double r= -aut[i+1];

    if(error<epsilon){
      memset(lpc+i,0,(m-i)*sizeof(*lpc));
      goto done;
    }

    for(j=0;j<i;j++)r-=lpc[j]*aut[i-j];
    r/=error;

    lpc[i]=r;
    for(j=0;j<i/2;j++){
      double tmp=lpc[j];

      lpc[j]+=r*lpc[i-1-j];
      lpc[i-1-j]+=r*tmp;
    }
    if(i&1)lpc[j]+=lpc[j]*r;

    error*=1.-r*r;

  }

 done:

  /* slightly damp the filter */
  {
    double g = .99;
    double damp = g;
    for(j=0;j<m;j++){
      lpc[j]*=damp;
      damp*=g;
    }
  }

  for(j=0;j<m;j++)lpci[j]=(float)lpc[j];

  free(aut);
  free(lpc);

  return error;
}