#include "spmanager.h"

SpectrumManager::SpectrumManager(){
	fft_size=0;
}

SpectrumManager::SpectrumManager(int size){
	fft_size=size;
	fft_in=fftw_alloc_complex(fft_size);
	fft_out=fftw_alloc_complex(fft_size);
	trans=fftw_plan_dft_1d(fft_size,fft_in,fft_out,FFTW_FORWARD,FFTW_MEASURE);
}

SpectrumManager::SpectrumManager(int size, int wtype){
	fft_size=size;
	fft_in=fftw_alloc_complex(fft_size);
	fft_out=fftw_alloc_complex(fft_size);
	trans=fftw_plan_dft_1d(fft_size,fft_in,fft_out,FFTW_FORWARD,FFTW_MEASURE);
	window_type=wtype;
}

SpectrumManager::~SpectrumManager(){
	fftw_free(fft_in);
	fftw_free(fft_out);
	fftw_destroy_plan(trans);
}


void SpectrumManager::set_window(int w){
	window_type=w;
}

void SpectrumManager::set_fft_size(int s){
	fft_size=s;
	fftw_free(fft_in);
	fftw_free(fft_out);
	fftw_destroy_plan(trans);
	fft_in=fftw_alloc_complex(fft_size);
	fft_out=fftw_alloc_complex(fft_size);
	trans=fftw_plan_dft_1d(fft_size,fft_in,fft_out,FFTW_FORWARD,FFTW_MEASURE);
}


void SpectrumManager::compute_spectrum(int16_t* in, double* out){
	int i;
	for(i=0;i<fft_size;i++){
		fft_in[0][i]=(double) in[i];
	}
	if(GET_TAKE_HALF(pipeline_plan)){
		apply_window(fft_in[0], fft_size, window_type);
	}
	
	fftw_execute(trans);
	
	int idx_max = GET_TAKE_HALF(pipeline_plan) ? fft_size/2 : fft_size;
	for(i=0;i<idx_max;i++){
		out[i]=sqrt(pow(fft_out[i][0],2)+pow(fft_out[i][1],2));
	}
}