#include <stdlib.h>     /* exit, EXIT_FAILURE, size_t */
#include <iostream>
#include "sndcleaner.h"
#include <assert.h>
#include "utils.h"
#include "processor.h"

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
	int len;
	SndCleaner* sc;
} pthread_dump_arg;

void check_endianness(){
	int i = 1;
	char* it = (char *) &i;
	endianness = (*it == 1) ? LITTLE : BIG;
	std::cout << endianness << std::endl;
}

template<typename T> void print_array(T* arr, int offset, int len){
	for(T* it = arr; it <  arr + offset + len; ++it)
		std::cout << *it << " ";
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
		q->size-=q->first_pkt->pkt.size;
		q->first_pkt=pt->next;
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
	conversion_out_format.sample_fmt = AV_SAMPLE_FMT_S16;
	conversion_out_format.sample_rate = OUT_SAMPLE_RATE;
	conversion_out_format.channel_layout = AV_CH_LAYOUT_MONO;
	swr = swr_alloc();
	std::cerr << "trying to create ring buffer" << std::endl;
	int nb_readers = with_playback ? 2 : 1;
	int s = rb_create(data_buffer, DATA_BUFFER_SIZE, nb_readers);
	if(s < 0){
		std::cerr << "Error creating ring buffer" << std::endl;
		exit(EXIT_FAILURE);
	}
	/* Initialize mutex and condition variable objects */
  	pthread_mutex_init(&data_writable_mutex, NULL);
  	pthread_cond_init (&data_writable_cond, NULL);

  	spmanager=new SpectrumManager(2048, WINDOW_BLACKMAN);

	if(with_playback){
		player = new Player(data_buffer);
		player->set_stream_reader_idx(0);
		player->set_fft_reader_idx(1);
		int p_status = player->open_with_wanted_specs(OUT_SAMPLE_RATE, 1);
		if(p_status<0){
			exit(EXIT_FAILURE);
		}
		player->set_cond_wait_write_parameters(&data_writable_mutex, &data_writable_cond);
		player->set_spectrum_manager(spmanager);
	}
	std::cerr << "ok" << std::endl;
}

SndCleaner::~SndCleaner(){
	std::cout << "destructor called sndcleaner" << std::endl;
	rb_free(data_buffer);
	delete player;
	delete spmanager;
	pthread_mutex_destroy(&data_writable_mutex);
	pthread_cond_destroy(&data_writable_cond);
	avformat_free_context(pFormatCtx);
	avcodec_free_context(&pCodecCtx);
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

	// std::cout << conversion_in_format.sample_fmt << std::endl;
	// std::cout << conversion_in_format.sample_rate << std::endl;
	// std::cout << conversion_in_format.channel_layout << std::endl;


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
*	Returns the number of packets added or -1 if we reached the end of the available packets
*/
int SndCleaner::fill_packet_queue(){
	int nb=0;
	AVPacket packet;
	while(!is_full(&audioq)){
		int s = av_read_frame(pFormatCtx, &packet);
		while(s>=0) {
			if(packet.stream_index==audioStreamId) {
				received_packets=true;
				if(packet_queue_put(&audioq, &packet)>=0){
					nb++;
				}
				break;
			}
			else {
	    		av_free_packet(&packet);
			}
			s=av_read_frame(pFormatCtx, &packet);
		}
		if(s<0){
			no_more_packets=true;
			return -1;
		}
			
	}
	return nb;
}


void* SndCleaner::read_frames(){
	AVPacket packet;
	while(av_read_frame(pFormatCtx, &packet)>=0) {
		// Is this a packet from the video stream?
		if(packet.stream_index==audioStreamId) {
			if(is_full(&audioq)){
				std::cerr << "Blocking reading, queue is full..." << std::endl;
			}
			while(is_full(&audioq)){
			}
			if(buffer_full){
				queue_flush(&audioq);
				break;
			}
	   		packet_queue_put(&audioq, &packet);
	   		received_packets=true;
	  	}
		else {
	    	av_free_packet(&packet);
		}
	}
	no_more_packets=true;
}



/*
* Pulls from the queue, and waits if not enough data in the queue.
* Decodes and dumps the packet queue in a stream buffer
*/
int SndCleaner::dump_queue(size_t len) {
  //std::cout << "dumping queue" << std::endl;
  int len1, audio_size;
  static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
  static unsigned int audio_buf_size = 0;
  static unsigned int audio_buf_index = 0;
  int first_len = len; 
  int nb_written=0;
  // std::cout << "audio buf index " << audio_buf_index << std::endl;
  // std::cout << "audio buf size " << audio_buf_size << std::endl; 
  while(len > 0) {
    if(audio_buf_index >= audio_buf_size) {
      /* We have already sent all our data; get more */
      audio_size = audio_decode_frame(pCodecCtx, audio_buf, sizeof(audio_buf));
      if(audio_size < 0) { // EOF reached ?
	    /* If error, output silence */
	    if(no_more_packets)
	    	return 0;
	    audio_buf_size = 1024; // arbitrary?
	    memset(audio_buf, 0, audio_buf_size);
	    std::cout << "audio size < 0" << std::endl;
	    std::cout << audioq.nb_packets << std::endl;
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
    //memcpy(stream, (uint8_t *) audio_buf + audio_buf_index, len1);
    nb_written=rb_write(data_buffer, (uint8_t *) audio_buf + audio_buf_index, len1);
    if(nb_written==0){
    	//std::cerr << "WARNING: no more room in the buffer" << std::endl;
    	break;
    }
    len -= nb_written;
    audio_buf_index += nb_written;
  }
  //print_array((int16_t *)first_stream, 0, first_len/2);
  //std::cout << "dumped" << std::endl;
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
  // static int bytes_per_sample = av_get_bytes_per_sample(pCodecCtx->sample_fmt); // can be moved for optimization
  static int bytes_per_sample = 2; // can be moved for optimization

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
      if(got_frame){
	    data_size = av_samples_get_buffer_size(frame.linesize, 
					       pCodecCtx->channels,
					       frame.nb_samples,
					       pCodecCtx->sample_fmt,
					       1); // Total number of bytes pulled from the packet, not per channel!!
	    assert(data_size <= buf_size);

	    count_in = frame.nb_samples;
	    nb_written = swr_convert(swr, &audio_buf, buf_size, (const uint8_t**) frame.extended_data, count_in);
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
// void SndCleaner::write_stream_to_data_buffer(int len){
// 	if(data_buffer_idx + len/2 >= DATA_BUFFER_SIZE)
// 		len = (DATA_BUFFER_SIZE - data_buffer_idx)*2;
// 	memcpy(data_buffer+data_buffer_idx, stream_buffer, len);
// 	// we only increment the pointer by len/2 because stream_buffer is uint8_t* and
// 	// data_buffer int16_t* 
// 	data_buffer_idx+=len/2; 

// }

bool SndCleaner::get_player_quit(){
	if(!with_playback)
		return false;
	else if(!player)
		return false;
	else
		return player->quit;
}



void* sc_read_frames(void* thread_arg){
	std::cout << "starting to read frames" << std::endl;
    SndCleaner* sc = (SndCleaner*) thread_arg;
	sc->read_frames();
}


/*
*	Continually dump frames until no more available, or quit event from the player
*/
void* sc_dump_frames(void* thread_arg){
	std::cout << "starting to dump frames" << std::endl;
	pthread_dump_arg* arg = (pthread_dump_arg*) thread_arg;
    int len = arg->len;
    SndCleaner* sc = arg->sc;
    int l=0;
    int i=0;
    do{
    	// std::cout << "writing data to ringbuffer..." << std::endl;
    	if(rb_get_write_space(sc->data_buffer) < 3*len){
    		// std::cout << "not enough write space: " << rb_get_write_space(sc->data_buffer) << std::endl;
    		pthread_mutex_lock(&sc->data_writable_mutex);
    		while (rb_get_write_space(sc->data_buffer) < len){
    			pthread_cond_wait(&sc->data_writable_cond, &sc->data_writable_mutex);
    			if(sc->get_player_quit())
    				break;
    		}
    		pthread_mutex_unlock(&sc->data_writable_mutex);
    	}

    	if(sc->get_player_quit())
    		break;
    	sc->fill_packet_queue();
    	l = sc->dump_queue(len);
    	//std::cout << "done." << std::endl;
    }while((!sc->reached_end() | l>0) && !sc->get_player_quit());
   
    if(sc->supports_playback()){
    	std::cout << "support playback true" << std::endl;
    	sc->player->no_more_data=true;
    	pthread_mutex_t quit_mutex;
		// Condition for write availability in the data buffer not to do busy wait
		// in the dump_queue method
		pthread_cond_t quit_cond;

		sc->player->register_for_quit_callback(&quit_mutex, &quit_cond);
		std::cout << "trying to lock quit mutex" << std::endl;
	    pthread_mutex_lock(&quit_mutex);
		std::cout << "locked" << std::endl;
	    while(rb_get_max_read_space(sc->data_buffer)>0 && !sc->get_player_quit()){
	    	//print_buffer_stats(sc->data_buffer);
	    	std::cout << "waiting quit signal" << std::endl;
			pthread_cond_wait(&quit_cond, &quit_mutex);
	    }
	    pthread_mutex_unlock(&quit_mutex);
    }
    else{
    	std::cout << "waiting end of read" << std::endl;
    	// while(rb_get_max_read_space(sc->data_buffer)>0){
    	// 	// Should do better an implement a kind of worker thread waiter capability
    	// }
    }

    
    //sc->player->quit_all();
    swr_free(&(sc->swr));
}


bool SndCleaner::reached_end(){
	if(!received_packets)
		return false;
	bool b = no_more_packets && is_empty(&audioq);
	//std::cout << "reached end " << b << std::endl;
	return b;
}

void SndCleaner::start_playback(){
	if(supports_playback())
		player->start_playback();
}

bool SndCleaner::supports_playback(){
	if(!with_playback)
		return false;
	return (player!=NULL);
}

void plotData(int16_t* data, int len, int nsize){
	PLFLT x[nsize];
	PLFLT y[nsize];
	if(x==NULL || y==NULL){
		std::cerr << "unable to allocate memory" << std::endl;
		exit(EXIT_FAILURE);
	}

	std::cout << "len is " << len << std::endl;

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



void SndCleaner::compute_spectrogram(){
	Spectrogram* s = new Spectrogram(1024);
	std::cout << "allocated spectrogram at " << s << std::endl;
	spmanager->register_spectrogram(s, OPEN_MODE_NORMAL);
	s->initialize_for_rendering();
	int len,lread=0;
	int flen=2048*sizeof(int16_t);
	int16_t* pulled_data = (int16_t *) malloc(flen);
	double* spectrum;
	std::cout << "starting to compute spectrogram" << std::endl;
	int k=0;
	while((rb_get_read_space(data_buffer, 0)>0 | !reached_end()) && k < 1000){
		//k++;
		// print_buffer_stats(data_buffer);
		spectrum=(double*) malloc(sizeof(double)*2048/2);
		if(!spectrum)
			exit(1);
		// std::cout << "spectrum allocated " << sizeof(double)*2048/2 << " bytes at address " << spectrum << std::endl;
		pthread_mutex_lock(&data_writable_mutex);
		// Open mode normal, only pointer is copied in the spectrogram
		// it is freed in the destructor, normally no memory leak
		len=flen;
		lread=0;
		while(len>0){
			lread=rb_read(data_buffer, (uint8_t *) pulled_data+lread, 0, (size_t) len);
			len-=lread;
			if(lread==0 && reached_end()){
				std::cout << "no more data" << std::endl;
				break;
			}

		}
		pthread_cond_signal(&data_writable_cond);
		pthread_mutex_unlock(&data_writable_mutex); // Very important for the matching pthread_cond_wait() routine to complete
		
		spmanager->compute_spectrum(pulled_data, spectrum);
	}
	int c = s->get_current_frame();
	uint8_t** bin = (uint8_t**) malloc(sizeof(uint8_t *)*c);
	for(int k=0; k<c; k++){
		bin[k] = (uint8_t*) malloc(sizeof(uint8_t)*1024);
		memset(bin[k], 0, sizeof(uint8_t)*1024);
	}
	Mask* m = alloc_mask(M1);
	apply_mask(s->get_data(), bin, c, 1024, m, CROPPED);
	//s->plot();
	s->plot_binarized_spectrogram(bin);
	s->dump_in_bmp("spectrogram");
	delete s;
	free(pulled_data);
	free_msk(m);
	for(int k=0; k<c; k++){
		free(bin[k]);
	}
	free(bin);
	std::cout << "finished to compute spectrogram" << std::endl;
}





int main(int argc, char *argv[]) {
	if(argc < 2) {
    	std::cerr << "Usage: sndcleaner <file>\n" << std::endl;
    	exit(1);
  	}
	check_endianness();
	av_register_all();
	SndCleaner cleaner; 
	cleaner.open_stream(argv[1]);

	pthread_t read_thread;
	pthread_t dump_thread;

	pthread_dump_arg dump_args;

	dump_args.len = STREAM_BUFFER_SIZE;
	dump_args.sc = &cleaner;

	std::cout << "lauching threads" << std::endl;

	// pthread_create(&read_thread,
 //                   NULL,
 //                   sc_read_frames,
 //                   (void *) &cleaner);
	pthread_create(&dump_thread,
                   NULL,
                   sc_dump_frames,
                   (void *) &dump_args);
	cleaner.player->start_playback();
	//cleaner.compute_spectrogram();
	pthread_join(dump_thread, NULL);
	std::cout << "dump thread joined" << std::endl;
	return 0;
}