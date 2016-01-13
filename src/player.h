#include <SDL2/SDL.h>
#include "ringbuffer.h"
#include <fftw3.h>

#define DATA_QUEUE_SIZE 10
#define FFT_SIZE 2048
#define SDL_AUDIO_BUFFER_SIZE 8192

typedef struct DataList{
	double* data;
	double pts;
	DataList* next=NULL;
} FrameList;


typedef struct DataQueue{
	DataList *first_frame, *last_frame;
	int nb_frames;
	int nb_frames_max;
	int frame_length;
	SDL_mutex* dataq_mutex;
} FrameQueue;


class Player{
public:
	Player(RingBuffer* b);
	void set_stream_reader_idx(int idx);
	void set_fft_reader_idx(int idx);
	void data_queue_init(int fl, int size);
	int open_with_wanted_specs(int sample_rate, Uint8 channel_nb);
	double get_audio_clock();
	void start_playback();
	void video_display(double* data, int l);
	void quit_all();
	DataQueue 		dataq;
	RingBuffer* 	data_source; // source from which to pull the data
	int				stream_reader_idx=-1; // index of the reader in the ring buffer
	int				fft_reader_idx=-1;
	int 			sample_rate; 
	SDL_Event       event;
	int				audio_hw_buf_size; // the buffer size given by the true specs
	double          frame_timer;
  	double          frame_last_pts; // pts of the last shown frame
  	double          last_queued_pts; // pts of the last queued frame
  	double          frame_last_delay;
  	double 			audio_frame_duration;
  	double 			video_frame_duration;
  	double          video_clock; // pts of last decoded frame / predicted pts of next decoded frame
  	double 			audio_clock; /* maintained in the audio thread */
  	bool			quit=false;
  	SDL_Window* 	window = NULL;
  	SDL_Surface* 	screen = NULL;
  	SDL_Renderer*	renderer = NULL;
  	SDL_mutex*      screen_mutex;
  	SDL_Thread 		*video_tid;
  	fftw_plan 		trans;
	fftw_complex 	*fft_in,*fft_out;
};