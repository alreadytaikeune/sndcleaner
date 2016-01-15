#ifndef _SPECTROGRAM_H_
#define _SPECTROGRAM_H_
#include <SDL2/SDL.h>
#define SPECTROGRAM_BASE_SIZE 100

class Spectrogram{
public:
	Spectrogram(int size);
	~Spectrogram();
	int _realloc();
	int shrink();
	int add_spectrum(double* spec);
	int add_spectrum_with_copy(double* spec);
	int initialize_for_rendering();
	SDL_Window* get_window();
	void plot();
	void dump_in_bmp(const char*);
	void set_fft_size(int size);
protected:
	int temp_frames_nb=SPECTROGRAM_BASE_SIZE;
	int current_frame=0;
	double** data;
	int fft_size=0;
	SDL_Window* window;
	SDL_Renderer* renderer;
};

#endif