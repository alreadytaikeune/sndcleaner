#ifndef _SPECTROGRAM_H_
#define _SPECTROGRAM_H_

#define SPECTROGRAM_BASE_SIZE 100

class Spectrogram{
public:
	Spectrogram();
	~Spectrogram();
	int _realloc();
	int shrink();
	int add_spectrum(double* spec);

protected:
	int temp_frames_nb=SPECTROGRAM_BASE_SIZE;
	int current_frame=0;
	double** data;
	int fft_size=0;
};

#endif