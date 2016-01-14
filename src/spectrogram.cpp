#include "spectrogram.h"
#include <stdlib.h>   
#define RESCALE_FACTOR 1.5

Spectrogram::Spectrogram(){

}


int Spectrogram::_realloc(){
	data = (double**) realloc(data, (size_t) sizeof(double)*temp_frames_nb*RESCALE_FACTOR);
	if(data){
		temp_frames_nb = (int)temp_frames_nb*RESCALE_FACTOR;
		return 0;
	}
	return -1;
}


int Spectrogram::add_spectrum(double* spec){
	if(current_frame==temp_frames_nb){
		if(_realloc() < 0)
			return -1;
	}
	data[current_frame]=spec;
	current_frame++;
	return 0;

}

Spectrogram::~Spectrogram(){
	for(int i=0; i < temp_frames_nb; i++){
		free(data[i]);
	}
	free(data);
}