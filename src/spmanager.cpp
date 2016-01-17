#include "spmanager.h"
#include <iostream>
#include "processor.h"

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
	if(s!=fft_size){
		std::cout << "setting new fft size " << s << " vs " << fft_size << std::endl;
		fft_size=s;
		fftw_free(fft_in);
		fftw_free(fft_out);
		fftw_destroy_plan(trans);
		fft_in=fftw_alloc_complex(fft_size);
		fft_out=fftw_alloc_complex(fft_size);
		trans=fftw_plan_dft_1d(fft_size,fft_in,fft_out,FFTW_FORWARD,FFTW_MEASURE);
		for(int i=0;i<fft_size;i++){
			fft_in[1][i]=0.;	
		}
	}
	
}


void SpectrumManager::compute_spectrum(int16_t* in, double* out){
	int i;
	for(i=0;i<fft_size;i++){
		fft_in[0][i]=(double) in[i];
	}
	// std::cout << "reading in at " << in << " up to address " << &in[fft_size-1] << std::endl;
	if(GET_DO_WINDOWING(pipeline_plan)){
		// std::cout << "applying window" << std::endl;
		apply_window(fft_in[0], fft_size, window_type);
		// std::cout << "applied" << std::endl;
	}
	
	fftw_execute(trans);
	
	int idx_max = GET_TAKE_HALF(pipeline_plan) ? fft_size/2 : fft_size;
	double* tmp;
	if(OPT_FLAG_ISSET(pipeline_plan, APPLY_MEL_RESCALING)){
		tmp = (double*) malloc(sizeof(double)*fft_size);
	}
	else{
		tmp=out;
	}
	for(i=0;i<idx_max;i++){
		out[i]=sqrt(pow(fft_out[i][0],2)+pow(fft_out[i][1],2));
	}
	// std::cout << "written out up to address " << &out[idx_max-1] << std::endl;

	if(GET_OPEN_MODE(pipeline_plan) == OPEN_MODE_NORMAL){
		if(!spectrogram){
			std::cerr << "no spectrogram set" << std::endl;
			return;
		}
		if(OPT_FLAG_ISSET(pipeline_plan, APPLY_MEL_RESCALING)){
			double* mel = (double*) malloc(sizeof(double)*MEL_SIZE);
			to_mel_scale(tmp, out, idx_max, MEL_SIZE, sampling/2);
			free(tmp);
		}
		spectrogram->add_spectrum(out);

	}
	else if(GET_OPEN_MODE(pipeline_plan) == OPEN_MODE_COPY){
		if(!spectrogram){
			std::cerr << "no spectrogram set" << std::endl;
			return;
		}
		spectrogram->add_spectrum_with_copy(out);
	}
	// std::cout << "done" << std::endl;
}

int SpectrumManager::register_spectrogram(Spectrogram* s, int open_mode){
	if(set_open_mode(open_mode)<0)
		return -1;
	spectrogram=s;
	if(OPT_FLAG_ISSET(pipeline_plan, APPLY_MEL_RESCALING)){
		spectrogram->set_fft_size(MEL_SIZE);
		return 0;
	}
	if(GET_TAKE_HALF(pipeline_plan))
		spectrogram->set_fft_size(fft_size/2);		
	else
		spectrogram->set_fft_size(fft_size);
	return 0;
}

int SpectrumManager::set_open_mode(int open_mode){
	std::cout << "open mode is " << GET_OPEN_MODE(pipeline_plan) << std::endl;
	if(open_mode != OPEN_MODE_NORMAL && open_mode != OPEN_MODE_COPY)
		return -1;
	if(GET_OPEN_MODE(pipeline_plan) != open_mode){
		OPT_FLAG_SET(pipeline_plan, GET_OPEN_MODE(pipeline_plan), 0);
		OPT_FLAG_SET(pipeline_plan, open_mode, 1);
	}
	std::cout << "pipeline plan is " << pipeline_plan << std::endl;
	std::cout << "open mode is " << GET_OPEN_MODE(pipeline_plan) << std::endl;
	return 0;	 
}

void SpectrumManager::set_pipeline(int pipeline){
	pipeline_plan=pipeline;
}