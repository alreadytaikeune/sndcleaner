#include "spectrogram.h"
#include <stdlib.h>
#include <iostream> 
#include <string.h>

#define RESCALE_FACTOR 1.5

Spectrogram::Spectrogram(int fsize){
	data_size=fsize;
	data = (double**) malloc(sizeof(double*)*SPECTROGRAM_BASE_SIZE);
	for(int i=0; i < temp_frames_nb; i++){
		data[i]=NULL;
	}
}


int Spectrogram::_realloc(){
	std::cout << "reallocating..." << std::endl;
	data = (double**) realloc(data, (size_t) sizeof(double)*temp_frames_nb*RESCALE_FACTOR);
	if(data){
		temp_frames_nb = (int)temp_frames_nb*RESCALE_FACTOR;
		return 0;
	}
	return -1;
}


int Spectrogram::add_spectrum(double* spec){
	// std::cout << "adding spectrum at address " << spec << std::endl;
	if(current_frame==temp_frames_nb){
		if(_realloc() < 0)
			return -1;
	}
	data[current_frame]=spec;
	current_frame++;
	return 0;
}

void Spectrogram::set_fft_size(int size){
	data_size = size;
}

void Spectrogram::get_plot_dimensions(int w, int h, int* x_step, int* y_step, int* rect_w, int* rect_h){
	*x_step = current_frame/w;
	if(*x_step==0)
		*x_step=1;
	*rect_w=w/current_frame;
	if(*rect_w==0)
		*rect_w=1;

	*y_step=data_size/h;
	if(*y_step==0)
		*y_step=1;
	*rect_h=h/data_size;
	if(*rect_h==0)
		*rect_h=1;
}


void Spectrogram::plot(){
	int w, h;
	int x_step, y_step;
	int rect_w, rect_h;
	int i,j;
	SDL_Rect r;
	if(!initialized){
		std::cerr << "not initialized for rendering" << std::endl;
	}
	SDL_GetWindowSize(window, &w, &h);
	if(current_frame==0)
		return;
	x_step = current_frame/w;
	if(x_step==0)
		x_step=1;
	rect_w=w/current_frame;
	if(rect_w==0)
		rect_w=1;

	y_step=data_size/h;
	if(y_step==0)
		y_step=1;
	rect_h=h/data_size;
	if(rect_h==0)
		rect_h=1;
	r.w=rect_w;
	r.h=rect_h;
	std::cout << "current frame: " << current_frame << std::endl;
	std::cout << "data_size: " << data_size << std::endl;
	std::cout << "rect_w: " << rect_w << std::endl;
	std::cout << "rect_h: " << rect_h << std::endl;
	std::cout << "x_step: " << x_step << std::endl;
	std::cout << "y_step: " << y_step << std::endl;
	std::cout << "w: " << w << std::endl;
	std::cout << "h: " << h << std::endl;
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	// Clear window
    SDL_RenderClear(renderer);
    int grey=0;
	for(i=0; i < current_frame; i+=x_step){
		r.x=(i/x_step)*rect_w;
		for(j=0; j < data_size; j+=y_step){
			r.y=(j/y_step)*rect_h;
			grey = (int) ((data[i][j]/150000.)*255);
			if(grey > 255)
				grey=255;
			SDL_SetRenderDrawColor(renderer, grey, grey, grey, 255);
			SDL_RenderFillRect(renderer, &r);
		}
	}
	SDL_RenderPresent(renderer);
	SDL_Delay(5000);
}


void Spectrogram::plot_binarized_spectrogram(uint8_t** bin){
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	// Clear window
    SDL_RenderClear(renderer);

    int w, h;
	int x_step, y_step;
	int rect_w, rect_h;
	int i,j;
	SDL_Rect r;

	SDL_GetWindowSize(window, &w, &h);
	if(current_frame==0)
		return;

	get_plot_dimensions(w, h, &x_step, &y_step, &rect_w, &rect_h);
	r.w=rect_w;
	r.h=rect_h;
	int grey=0;
	for(i=0; i < current_frame; i+=x_step){
		r.x=(i/x_step)*rect_w;
		for(j=0; j < data_size; j+=y_step){
			r.y=(j/y_step)*rect_h;
			grey = bin[i][j]*255;
			SDL_SetRenderDrawColor(renderer, grey, 0, 255-grey, 255);
			SDL_RenderFillRect(renderer, &r);
		}
	}
	SDL_RenderPresent(renderer);
	SDL_Delay(5000);

}



int Spectrogram::initialize_for_rendering(){
	if(initialized){
		std::cout << "already initialized" << std::endl;
		return 0;
	}
	window = SDL_CreateWindow("Spectrum",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          1000, 1000,
                          0);
  	if(!window){
  		std::cerr << "Could not create screen SDL: " << SDL_GetError() << std::endl;
    	return -1;
  	}
  	renderer = SDL_CreateRenderer(window, -1, 0);
  	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	// Clear window
    SDL_RenderClear(renderer);
    initialized=true;
    return 0;
}

int Spectrogram::add_spectrum_with_copy(double* spec){
	double* new_spec = (double*) malloc(data_size*sizeof(double));
	if(!new_spec){
		std::cerr << "unable to allocate memory for spectrum" << std::cout;
		return -1;
	}
		
	if(current_frame==temp_frames_nb){
		if(_realloc() < 0)
			return -1;
	}
	if(!memcpy(new_spec, spec, data_size*sizeof(double)))
		return -1;
	data[current_frame]=new_spec;
	current_frame++;
	return 0;

}

Spectrogram::~Spectrogram(){
	std::cout << "destructor called spectrogram" << std::endl;

	for(int i=0; i < current_frame; i++){
		free(data[i]);
	}
	free(data);
	if(initialized){
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}


}

SDL_Window* Spectrogram::get_window(){
	return window;
}

void Spectrogram::dump_in_bmp(std::string filename){

	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	SDL_Surface *sshot = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, sshot->pixels, sshot->pitch);
	SDL_SaveBMP(sshot, (const char *) (filename+".bmp").c_str());
	SDL_FreeSurface(sshot);
}

int Spectrogram::get_current_frame(){
	return current_frame;
}

double** Spectrogram::get_data(){
	return data;
}

