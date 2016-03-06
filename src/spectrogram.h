#ifndef _SPECTROGRAM_H_
#define _SPECTROGRAM_H_
#include <SDL2/SDL.h>
#define SPECTROGRAM_BASE_SIZE 5000
#include <string>
class Spectrogram{
public:
	Spectrogram(int size);
	~Spectrogram();
	int _realloc();
	int shrink();
	int add_spectrum(double* spec);
	int add_spectrum_with_copy(double* spec);
	SDL_Window* get_window();
	void plot();
	void plot_up_to(float max_hz, float sampling);
	void plot_binarized_spectrogram(uint8_t** bin);
	void dump_in_bmp(std::string filename);
	void set_fft_size(int size);
	int get_current_frame();
	double** get_data();
	void get_plot_dimensions(int w, int h, int* x_step, int* y_step, int* rect_w, int* rect_h);

protected:
	int temp_frames_nb=SPECTROGRAM_BASE_SIZE;
	int current_frame=0;
	double** data;
	int data_size=0;
	SDL_Window* window;
	SDL_Renderer* renderer;
	bool initialized=false;
};

#endif