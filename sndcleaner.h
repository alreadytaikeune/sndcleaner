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

#define MAX_AUDIO_FRAME_SIZE 192000
#define MINUTES_IN_BUFFER 1
#define SECONDS_IN_BUFFER 20
#define OUT_SAMPLE_RATE  44100
#define DATA_BUFFER_SIZE SECONDS_IN_BUFFER*OUT_SAMPLE_RATE
#define STREAM_BUFFER_SIZE 1152*2*4

typedef struct PacketQueue {
  AVPacketList *first_pkt, *last_pkt;
  int nb_packets;
  int size;
  std::mutex queue_operation_mutex;
} PacketQueue;


typedef struct ConversionFormat{
	AVSampleFormat sample_fmt;
	int sample_rate;
	int64_t channel_layout;
} ConversionFormat;


class SndCleaner{
public:
	void open_stream(char * filename);
	void acquire_data(char * filename);
	void packet_queue_init(PacketQueue *q);
	SndCleaner();
	~SndCleaner();
	void* read_frames();
	bool is_queue_full();
	bool is_queue_empty();
	// void dump_queue_in_stream(uint8_t *stream, int len);
	int dump_queue_in_stream(uint8_t *stream, int len);
	int audio_decode_frame(AVCodecContext *pCodecCtx, uint8_t *audio_buf, int buf_size);
	void write_stream_to_data_buffer(int len);
	void print_data_buffer(int offset, int len);

	// This stream buffer may not appear very useful for now but might become when we'll be doing further 
	// processing berfore storing in the data buffer. It induces a performance hit but may become handy.
	uint8_t *stream_buffer = (uint8_t *) malloc(STREAM_BUFFER_SIZE); 
	int16_t *data_buffer = (int16_t *) malloc(DATA_BUFFER_SIZE*sizeof(int16_t)); // Allocate in the heap
	SwrContext *swr; // the resampling context for outputting with standard format in data_buffer
	int data_buffer_idx;
private:
	AVFormatContext *pFormatCtx = NULL;
	AVCodecContext *pCodecCtx = NULL; // the audio codec
	
	PacketQueue audioq;

	// IO formats of the conversion held in a structure, a bit redondant but more elegant
	ConversionFormat conversion_out_format;
	ConversionFormat conversion_in_format;
	int audioStreamId;
	int stored_samples;

};