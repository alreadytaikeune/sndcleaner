#include <stdlib.h>     /* exit, EXIT_FAILURE, size_t */
#include <iostream>
#include "sndcleaner.h"
#include <assert.h>
#include <pthread.h>
#include "utils.h"


// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif
#endif

static int packet_queue_get(PacketQueue *q, AVPacket *pkt);
static int packet_queue_put(PacketQueue *q, AVPacket *pkt);


static bool buffer_full = false;
enum Endianness {BIG, LITTLE, UNDEDFINED};


Endianness endianness = UNDEDFINED;

typedef struct pthread_dump_arg{
	uint8_t *stream;
	int len;
	SndCleaner* sc;
} pthread_dump_arg;

void check_endianness(){
	int i = 1;
	char* it = (char *) &i;
	endianness = (*it == 1) ? LITTLE : BIG;
	std::cout << endianness << std::endl;
}

void SndCleaner::print_data_buffer(int offset, int len){
	for(int i = 0; i < len; i++)
		std::cout << data_buffer[offset+i] << " ";
	std::cout << std::endl;

}

template<typename T> void print_array(T* arr, int offset, int len){
	for(T* it = arr; it <  arr + offset + len; ++it)
		std::cout << (int) *it << " ";
	std::cout << std::endl;
}



//	PacketQueue related methods
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


void SndCleaner::packet_queue_init(PacketQueue *q, int nb_m) {
  memset(q, 0, sizeof(PacketQueue));
  q->nb_packets_max=nb_m;
}



static bool is_full(PacketQueue* q){
	if(!q){
		std::cerr << "Packets queue is null" << std::endl;
		exit(EXIT_FAILURE);
	}
	if(q->nb_packets > q->nb_packets_max){
		std::cerr << "Inconherent state in queue, more packets than allowed" << std::endl;
		exit(EXIT_FAILURE);
	}
	return q->nb_packets == q->nb_packets_max;
	
}


static bool is_empty(PacketQueue* q){
	if(!q){
		std::cerr << "Packets queue is null" << std::endl;
		exit(EXIT_FAILURE);
	}
	return q->first_pkt == NULL;
}


static void queue_flush(PacketQueue *q){
	AVPacketList* pt = q->first_pkt;
	q->queue_operation_mutex.lock();
	while(pt){
		q->first_pkt=pt->next;
		q->size-=q->first_pkt->pkt.size;
		q->nb_packets--;
		av_free(pt);
		pt=q->first_pkt;
	}
	q->queue_operation_mutex.unlock();
}


static int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
  if(is_full(q))
  	return -1;
  AVPacketList *pkt1;
  if(av_dup_packet(pkt) < 0) {
    return -1;
  }
  pkt1 = (AVPacketList*) av_malloc(sizeof(AVPacketList));
  if (!pkt1)
    return -1;
  pkt1->pkt = *pkt;
  pkt1->next = NULL;

  q->queue_operation_mutex.lock();
  
  if (!q->last_pkt)
    q->first_pkt = pkt1;
  else
    q->last_pkt->next = pkt1;
  q->last_pkt = pkt1;
  q->nb_packets++;
  q->size += pkt1->pkt.size;
  q->queue_operation_mutex.unlock();
  return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt) {
  AVPacketList *pkt1;
  int ret;
  q->queue_operation_mutex.lock();
  pkt1 = q->first_pkt;
  if (pkt1) {
    q->first_pkt = pkt1->next;
    if (!q->first_pkt)
	  q->last_pkt = NULL;
    q->nb_packets--;
    q->size -= pkt1->pkt.size;
    *pkt = pkt1->pkt;
    av_free(pkt1);
    ret = 1;
  } 
  else{
    ret = 0;
  }
  q->queue_operation_mutex.unlock();
  return ret;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


SndCleaner::SndCleaner(){
	packet_queue_init(&audioq, PACKET_QUEUE_MAX_NB);
	audioStreamId=-1;
	data_buffer_idx=0;
	conversion_out_format.sample_fmt = AV_SAMPLE_FMT_S16;
	conversion_out_format.sample_rate = OUT_SAMPLE_RATE;
	conversion_out_format.channel_layout = AV_CH_LAYOUT_MONO;
	swr = swr_alloc();
}

SndCleaner::~SndCleaner(){

}

void SndCleaner::acquire_data(char * filename){
	open_stream(filename);	
}

void SndCleaner::open_stream(char * filename){
	std::cout << "retrieving " << filename << std::endl;

	// Open video file
	if(avformat_open_input(&pFormatCtx, filename, NULL, NULL)!=0){
		std::cerr << "Error opening input" << std::endl;
		exit(EXIT_FAILURE);
	}

	std::cout << "finding stream info " << std::endl;

	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0){
		std::cerr << "Error finding stream information" << std::endl;
		exit(EXIT_FAILURE);
	}

	av_dump_format(pFormatCtx, 0, filename, 0);

	int i;
	AVCodecContext *pCodecCtxOrig = NULL;

	// Find the first audio stream
	for(i=0; i<pFormatCtx->nb_streams; i++)
	  if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO &&
     		audioStreamId < 0) {
	    audioStreamId=i;
	    break;
	  }
	if(audioStreamId==-1){
		std::cerr << "Didn't find audio stream" << std::endl;
		exit(EXIT_FAILURE);

	}

	std::cout << "audio stream is " << audioStreamId << std::endl;
	// Get a pointer to the codec context for the audio stream
	pCodecCtxOrig=pFormatCtx->streams[audioStreamId]->codec;

	std::cout << "pCodecOrig found" << std::endl;

	// Now find the actual codec and open it

	AVCodec *pCodec = NULL;

	// Find the decoder for the audio stream
	pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);


	std::cout << "pCodec found" << std::endl;

	if(pCodec==NULL) {
	  std::cerr << "Unsupported codec!" << std::endl;
	  exit(EXIT_FAILURE);
	}
	// Copy context
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
	  std::cerr << "Couldn't copy codec context" << std::endl;
	  exit(EXIT_FAILURE);
	}
	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec, NULL)<0){
	  std::cerr << " Could not open codec" << std::endl;
	  exit(EXIT_FAILURE);
	}

	conversion_in_format.sample_fmt = pCodecCtx->sample_fmt;
	conversion_in_format.sample_rate = pCodecCtx->sample_rate;
	conversion_in_format.channel_layout = pCodecCtx->channel_layout;

	std::cout << conversion_in_format.sample_fmt << std::endl;
	std::cout << conversion_in_format.sample_rate << std::endl;
	std::cout << conversion_in_format.channel_layout << std::endl;


	av_opt_set_int(swr, "in_channel_layout",  conversion_in_format.channel_layout, 0);
	av_opt_set_int(swr, "out_channel_layout", conversion_out_format.channel_layout,  0);
	av_opt_set_int(swr, "in_sample_rate",     conversion_in_format.sample_rate, 0);
	av_opt_set_int(swr, "out_sample_rate",    conversion_out_format.sample_rate, 0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt",  conversion_in_format.sample_fmt, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", conversion_out_format.sample_fmt,  0);
	swr_init(swr);

	std::cout << "stream successfully opened" << std::endl;
}



/*
* Read frames and process them before storing data in data_buffer
*/
void* SndCleaner::read_frames(){
	AVPacket packet;
	while(av_read_frame(pFormatCtx, &packet)>=0 && !buffer_full) {
		// Is this a packet from the video stream?
		if(packet.stream_index==audioStreamId) {
			if(is_full(&audioq)){
				std::cerr << "Blocking reading, queue is full..." << std::endl;
			}
			while(is_full(&audioq) && !buffer_full){
			}
			if(buffer_full){
				queue_flush(&audioq);
				break;
			}
	   		packet_queue_put(&audioq, &packet);
	  	}
		else {
	    	av_free_packet(&packet);
		}
	}
}



/*
* Pulls from the queue, and waits if not enough data in the queue.
* Decodes and dumps the packet queue in a stream buffer
*/
int SndCleaner::dump_queue_in_stream(uint8_t *stream, int len) {
  int len1, audio_size;
  static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
  static unsigned int audio_buf_size = 0;
  static unsigned int audio_buf_index = 0;
  int first_len = len; 
  uint8_t* first_stream = stream;
  // std::cout << "audio buf index " << audio_buf_index << std::endl;
  // std::cout << "audio buf size " << audio_buf_size << std::endl; 
  while(len > 0) {
    if(audio_buf_index >= audio_buf_size) {
      /* We have already sent all our data; get more */
      audio_size = audio_decode_frame(pCodecCtx, audio_buf, sizeof(audio_buf));
      if(audio_size < 0) { // EOF reached ?
	    /* If error, output silence */
	    //audio_buf_size = 1024; // arbitrary?
	    //memset(audio_buf, 0, audio_buf_size);
	    std::cout << "audio size < 0" << std::endl;
	    break; // exits the while loop and return the number of bytes read.
      } 
      else {
	    audio_buf_size = audio_size;
      }
      audio_buf_index = 0;
    }
    // std::cout << "audio size read: " << audio_size << std::endl;
    len1 = audio_buf_size - audio_buf_index;
    if(len1 > len)
      len1 = len;
 	//std::cout << "writing " << len1 << " bytes to stream" << std::endl;
    memcpy(stream, (uint8_t *) audio_buf + audio_buf_index, len1);
    len -= len1;
    stream += len1;
    audio_buf_index += len1;
  }
  //print_array((int16_t *)first_stream, 0, first_len/2);
  return first_len - len;
}

/*
* Returns the number of bytes written in the audio buf
*/
int SndCleaner::audio_decode_frame(AVCodecContext *pCodecCtx, uint8_t *audio_buf, int buf_size) {

  static AVPacket pkt;
  static uint8_t *audio_pkt_data = NULL;
  static int audio_pkt_size = 0;
  static AVFrame frame;
  static int count_in;
  static int bytes_per_sample = av_get_bytes_per_sample(pCodecCtx->sample_fmt); // can be moved for optimization

  int len1, data_size = 0;
  int nb_written = 0;
  for(;;) {
    while(audio_pkt_size > 0) {
      int got_frame = 0;
      len1 = avcodec_decode_audio4(pCodecCtx, &frame, &got_frame, &pkt);
      if(len1 < 0) {
	    /* if error, skip frame */
	    std::cerr << "error decoding frame" << std::endl;
	    audio_pkt_size = 0;
	    break;
      }
      audio_pkt_data += len1;
      audio_pkt_size -= len1;
      data_size = 0;
      nb_written = 0;
      if(got_frame) {
	    data_size = av_samples_get_buffer_size(NULL, 
					       pCodecCtx->channels,
					       frame.nb_samples,
					       pCodecCtx->sample_fmt,
					       1); // Total number of bytes pulled from the packet, not per channel!!
	    assert(data_size <= buf_size);
	    count_in = frame.nb_samples;
	    nb_written = swr_convert(swr, &audio_buf, buf_size, (const uint8_t**) frame.extended_data, count_in);
	    //print_array((int16_t *)audio_buf, 0, nb_written);
	    if(nb_written <= 0){
	    	std::cerr << "error converting data" << std::endl;
	    	exit(EXIT_FAILURE);
	    }
      }

      if(data_size <= 0) {
	    /* No data yet, get more frames */
	    continue;
      }
      /* We have data, return it and come back for more later */
      return nb_written*bytes_per_sample; 
    }
    if(pkt.data)
      av_free_packet(&pkt);


    if(packet_queue_get(&audioq, &pkt) == 0) {
      return -1;
    }
    audio_pkt_data = pkt.data;
    audio_pkt_size = pkt.size;
  }
}

/*
*  Processes and writes len number of bytes from the stream to the data buffer. 
*/
void SndCleaner::write_stream_to_data_buffer(int len){
	if(data_buffer_idx + len/2 >= DATA_BUFFER_SIZE)
		len = (DATA_BUFFER_SIZE - data_buffer_idx)*2;
	memcpy(data_buffer+data_buffer_idx, stream_buffer, len);
	// we only increment the pointer by len/2 because stream_buffer is uint8_t* and
	// data_buffer int16_t* 
	data_buffer_idx+=len/2; 

}


void* sc_read_frames(void* thread_arg){
	std::cout << "starting to read frames" << std::endl;
    SndCleaner* sc = (SndCleaner*) thread_arg;
	sc->read_frames();
}

void* sc_dump_frames(void* thread_arg){
	std::cout << "starting to dump frames" << std::endl;
	pthread_dump_arg* arg = (pthread_dump_arg*) thread_arg;
    uint8_t *stream = (uint8_t *) arg->stream;
    int len = arg->len;
    SndCleaner* sc = arg->sc;
    int l=0;
    while(sc->data_buffer_idx < DATA_BUFFER_SIZE){
    	l = sc->dump_queue_in_stream(stream, len);
		sc->write_stream_to_data_buffer(l);
    }
    buffer_full=true;
    swr_free(&(sc->swr));
}


void plotData(int16_t* data, int len, int nsize){
	PLFLT x[nsize];
	PLFLT y[nsize];
	if(x==NULL || y==NULL){
		std::cerr << "unable to allocate memory" << std::endl;
		exit(EXIT_FAILURE);
	}

	PLFLT max_value = (PLFLT) max_abs(data, len);
    PLFLT xmin = 0., xmax = len/((float) OUT_SAMPLE_RATE), ymin = 0., ymax = 1.0;
    int i,j;
    std::cout << "smax: " << xmax << std::endl;
    std::cout << "max value: " << max_value << std::endl;
    int step = len/nsize+1;
    std::cout << "step: " << step << std::endl;
    int mean = 0;
    // Prepare data to be plotted.
    for (i = 0; i < len; i+=step)
    {
    	x[i/step] = (PLFLT) ( i ) / (PLFLT) (OUT_SAMPLE_RATE);
    	mean = 0;
    	for(j=0; j < step && i+j < len; j++){
    		mean+=std::abs(data[i+j]);
    	}
        
        y[i/step] = mean / max_value /step;
    }

    // Parse and process command line arguments
    // plparseopts(&argc, argv, PL_PARSE_FULL);

    // Initialize plplot
    plinit();

    // Create a labelled box to hold the plot.
    plenv( xmin, xmax, ymin, ymax, 0, 0 );
    //pllab( "x", "y=100 x#u2#d", "Simple PLplot demo of a 2D line plot" );

    // Plot the data that was prepared above.
    plline(nsize, x, y);

    // Close PLplot library
    plend();
    std::cout << "ended" << std::endl;
}



int main(int argc, char *argv[]) {
	check_endianness();
	av_register_all();
	SndCleaner cleaner; 
	cleaner.open_stream(argv[1]);

	pthread_t read_thread;
	pthread_t dump_thread;

	pthread_dump_arg dump_args;

	dump_args.len = STREAM_BUFFER_SIZE;
	dump_args.stream = cleaner.stream_buffer;
	dump_args.sc = &cleaner;

	std::cout << "lauching threads" << std::endl;

	pthread_create(&read_thread,
                   NULL,
                   sc_read_frames,
                   (void *) &cleaner);
	pthread_create(&dump_thread,
                   NULL,
                   sc_dump_frames,
                   (void *) &dump_args);

	pthread_join(read_thread, NULL);
	pthread_join(dump_thread, NULL);
	plotData(cleaner.data_buffer, cleaner.data_buffer_idx, 10000);
	return 0;
}