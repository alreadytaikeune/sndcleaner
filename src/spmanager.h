#ifndef _SPMANAGER_H_
#define _SPMANAGER_H_
#include <fftw3.h>
#ifndef _WINDOW_H_
#include "window.h"
#endif
#include "spectrogram.h"

#define APPLY_WINDOW           0x01
#define APPLY_DENOISING        0x02
#define TAKE_HALF_SPEC         0x04
#define OPEN_NO_SPEC		   0x10
#define OPEN_MODE_NORMAL	   0x20
#define OPEN_MODE_COPY		   0x40

#define OPT_FLAG_ISSET(flags, opt)	    ((flags & opt) > 0)
// #define OPT_FLAG_SET(flags, opt)	    (flags =| opt)
#define OPT_FLAG_SET(flags, opt, val)	    (val ? (flags |= opt) : (flags &= (~opt)))

#define GET_DO_WINDOWING(flags)		(OPT_FLAG_ISSET(flags, APPLY_WINDOW))
#define GET_DO_DENOISING(flags)		(OPT_FLAG_ISSET(flags, APPLY_DENOISING))
#define GET_TAKE_HALF(flags)		(OPT_FLAG_ISSET(flags, TAKE_HALF_SPEC))
#define GET_OPEN_MODE(flags)		(flags & (OPEN_NO_SPEC | OPEN_MODE_NORMAL | OPEN_MODE_COPY))
class SpectrumManager{
public:
	SpectrumManager();
	SpectrumManager(int fft_size);
	SpectrumManager(int fft_size, int w);
	~SpectrumManager();
	void compute_spectrum(int16_t* in, double* out);
	void set_window(int w);
	void set_fft_size(int s);
	int register_spectrogram(Spectrogram* s, int open_mode);
	int set_open_mode(int open_mode);

private:
	int 			pipeline_plan=APPLY_WINDOW | TAKE_HALF_SPEC | OPEN_NO_SPEC;
	fftw_plan 		trans;
	fftw_complex 	*fft_in,*fft_out;
	int 			fft_size=0;
	int 			window_type=WINDOW_RECT;
	Spectrogram*	spectrogram=NULL;

};


#endif // _SPMANAGER_H_