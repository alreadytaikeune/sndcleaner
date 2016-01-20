#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <plplot/plConfig.h>
#include <plplot/plplot.h>
#ifdef __cplusplus 
}

#endif
#include <stdint.h>
#include <mutex>
#include "ringbuffer.h"
#include "player.h"
#include <pthread.h>
#include "utils.h"


#define MAX_AUDIO_FRAME_SIZE 192000
#define MINUTES_IN_BUFFER 1
#define SECONDS_IN_BUFFER 20
#define OUT_SAMPLE_RATE  44100
#define DATA_BUFFER_SIZE SECONDS_IN_BUFFER*OUT_SAMPLE_RATE
#define STREAM_BUFFER_SIZE 1152*2*4
#define PACKET_QUEUE_MAX_NB 5000

typedef struct PacketQueue {
  AVPacketList *first_pkt, *last_pkt;
  int nb_packets;
  int nb_packets_max;
  int size;
  std::mutex queue_operation_mutex;
} PacketQueue;


typedef struct ConversionFormat{
	AVSampleFormat sample_fmt;
	int sample_rate;
	int64_t channel_layout;
} ConversionFormat;

typedef struct ProgramOptions{
	int fft_size=2048;
	bool with_playback=false;
	std::string filename;
	bool take_half=true;
	bool apply_window=true;
	int window=WINDOW_BLACKMAN;
	int mel=-1;

} ProgramOptions;


class SndCleaner{
public:
	void open_stream();
	void packet_queue_init(PacketQueue *q, int nb);
	SndCleaner(ProgramOptions* op);
	~SndCleaner();
	void* read_frames();
	int fill_packet_queue();
	// void dump_queue_in_stream(uint8_t *stream, int len);
	int dump_queue(size_t len);
	int audio_decode_frame(AVCodecContext *pCodecCtx, uint8_t *audio_buf, int buf_size);
	void audio_callback(void *userdata, Uint8 *stream, int len); // Callback called by the SDL
	bool reached_end();
	bool get_player_quit();
	void start_playback();
	bool supports_playback();
	void compute_spectrogram();
	void compute_mel_spectrogram();
	int get_mel_size();
	int get_fft_size();
	int get_sampling();
	//void write_stream_to_data_buffer(int len);

	// This stream buffer may not appear very useful for now but might become when we'll be doing further 
	// processing berfore storing in the data buffer. It induces a performance hit but may become handy.
	//int16_t *data_buffer = (int16_t *) malloc(DATA_BUFFER_SIZE*sizeof(int16_t)); // Allocate in the heap
	RingBuffer* data_buffer = (RingBuffer *) malloc(sizeof(RingBuffer));
	SwrContext *swr; // the resampling context for outputting with standard format in data_buffer
	Player* player=NULL;
	pthread_mutex_t data_writable_mutex;
	// Condition for write availability in the data buffer not to do busy wait
	// in the dump_queue method
	pthread_cond_t data_writable_cond;
	// Condition for write availability in the data buffer not to do busy wait
	// in the dump_queue method
	SpectrumManager* spmanager=NULL;
private:
	AVFormatContext *pFormatCtx = NULL;
	AVCodecContext *pCodecCtx = NULL; // the audio codec
	
	PacketQueue audioq;
	// IO formats of the conversion held in a structure, a bit redondant but more elegant
	ConversionFormat conversion_out_format;
	ConversionFormat conversion_in_format;
	int audioStreamId;
	int stored_samples;
	bool received_packets=false;
	bool no_more_packets=false;
	ProgramOptions* options;
};