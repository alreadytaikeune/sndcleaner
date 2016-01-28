#include <stdlib.h>     /* exit, EXIT_FAILURE, size_t */
#include <iostream>
#include "sndcleaner.h"
#include <assert.h>
#include "processor.h"
#include <boost/program_options.hpp>
#include "cleansound.h"
#include <bitset>
#include "plotter.h"
#include <sys/time.h>
#include "file_set.h"
#include "distribution.h"


namespace po = boost::program_options;

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



/*
*	TODO:
		- Add arg checking
		- finish the add spectrogram with copy pipeline
		- add max seconds argument
*/




typedef struct pthread_dump_arg{
	int len;
	SndCleaner* sc;
} pthread_dump_arg;


typedef struct lpc_thread_arg{
	SndCleaner* sc;
	float* errors;
	int nb_errors;
} lpc_thread_arg;


template<typename T> void print_array(T* arr, int offset, int len){
	for(T* it = arr; it <  arr + offset + len; ++it)
		std::cout << *it << " ";
	std::cout << std::endl;
}


void print_options(ProgramOptions* op){
	std::cout << "fft_size: " << op->fft_size << std::endl;
	std::cout << "playback: " << op->with_playback << std::endl;
	std::cout << "filename: " << op->filename << std::endl;
	std::cout << "half: " << op->take_half << std::endl;
	std::cout << "apply window: " << op->apply_window << std::endl;
	std::cout << "window: " << op->window << std::endl;
	std::cout << "mel: " << op->mel << std::endl;
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


SndCleaner::SndCleaner(ProgramOptions* op){
	if(!op)
		options = (ProgramOptions*) malloc(sizeof(ProgramOptions));
	else
		options = op;
	print_options(options);
	packet_queue_init(&audioq, PACKET_QUEUE_MAX_NB);
	audioStreamId=-1;
	conversion_out_format.sample_fmt = AV_SAMPLE_FMT_S16;
	conversion_out_format.sample_rate = OUT_SAMPLE_RATE;
	conversion_out_format.channel_layout = AV_CH_LAYOUT_MONO;
	swr = swr_alloc();
	int nb_readers = options->with_playback ? 2 : 1;
	int s = rb_create(data_buffer, DATA_BUFFER_SIZE, nb_readers);
	if(s < 0){
		std::cerr << "Error creating ring buffer" << std::endl;
		exit(EXIT_FAILURE);
	}
	/* Initialize mutex and condition variable objects */
  	pthread_mutex_init(&data_writable_mutex, NULL);
  	pthread_cond_init (&data_writable_cond, NULL);

  	// Parsing options

  	int pipeline=0;

  	if(op->apply_window){
  		spmanager=new SpectrumManager(2048, op->window);
  		pipeline |= APPLY_WINDOW; 
  	}
  	else{
  		spmanager=new SpectrumManager(2048);
  	}

  	if(op->take_half){
  		pipeline |= TAKE_HALF_SPEC; 
  	}

	if(op->mel>0){
		pipeline |= APPLY_MEL_RESCALING;
	}


	if(op->max_time>0){
		max_byte = op->max_time*conversion_out_format.sample_rate*av_get_bytes_per_sample(conversion_out_format.sample_fmt);
	}

	std::cout << "max byte: " << max_byte << std::endl;
  	spmanager->set_pipeline(pipeline);
  	spmanager->set_sampling(conversion_out_format.sample_rate);

	if(options->with_playback){
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


void SndCleaner::open_stream(){
	if(stream_opened){
		return;
	}
	av_register_all();
	char* filename = (char*) options->filename.c_str();
	// Open video file
	if(avformat_open_input(&pFormatCtx, filename, NULL, NULL)!=0){
		std::cerr << "Error opening input" << std::endl;
		exit(EXIT_FAILURE);
	}

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

	// Get a pointer to the codec context for the audio stream
	pCodecCtxOrig=pFormatCtx->streams[audioStreamId]->codec;


	// Now find the actual codec and open it

	AVCodec *pCodec = NULL;

	// Find the decoder for the audio stream
	pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);

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
	stream_opened=true;	
	//std::cout << "stream successfully opened" << std::endl;
}


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
  static int bytes_per_sample = av_get_bytes_per_sample(conversion_out_format.sample_fmt); 
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


bool SndCleaner::get_player_quit(){
	if(!options->with_playback)
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


int SndCleaner::fill_buffer(){
	pthread_mutex_lock(&data_writable_mutex);
	int to_write = rb_get_write_space(data_buffer);
	int lread=0, len_written=0;
	fill_packet_queue();
	if(to_write > max_byte)
		to_write=max_byte;
	while(to_write>0){
		if(max_byte<=0){
			break;
		}
		if(to_write > max_byte){
			to_write = max_byte;
		}
		lread=dump_queue(to_write);
		max_byte-=lread;
		len_written+=lread;
		if(lread==0){
			if(fill_packet_queue() == -1){
				// there is no more packet to read, we reached the end of the data
				break;
			} 
		}
	}
	pthread_mutex_unlock(&data_writable_mutex);
	//std::cout << "buffer filled with: " << len_written << std::endl;
	return len_written;
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
    sc->fill_buffer();
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
    	l = sc->fill_buffer();
    }while(l>0);
   
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
	if(options->max_time > 1){
		if(max_byte <=0)
			return true;
	}

	bool b = no_more_packets && is_empty(&audioq);
	//std::cout << "reached end " << b << std::endl;
	return b;
}

void SndCleaner::start_playback(){
	if(supports_playback())
		player->start_playback();
}

bool SndCleaner::supports_playback(){
	if(!options->with_playback)
		return false;
	return (player!=NULL);
}


int SndCleaner::get_mel_size(){
	return options->mel;
}

int SndCleaner::get_fft_size(){
	return options->fft_size;
}

int SndCleaner::get_sampling(){
	return conversion_out_format.sample_rate;
}

int SndCleaner::get_max_byte(){
	return max_byte;
}

int SndCleaner::get_time_in_bytes(float sec){
	return (int) (sec*conversion_out_format.sample_rate*av_get_bytes_per_sample(conversion_out_format.sample_fmt));
}

int SndCleaner::register_reader(){
	int i = add_reader(data_buffer);
	if(i==-1)
		exit(EXIT_FAILURE);
	return i;
}


void SndCleaner::compute_spectrogram(){
	compute_spectrogram(0);
}


void SndCleaner::compute_spectrogram(int reader){
	std::cout << "starting to compute spectrogram\n";
	Spectrogram* s = new Spectrogram(options->fft_size); // s is destroyed in the spmanager's destructor
	if(spmanager->register_spectrogram(s, OPEN_MODE_NORMAL)<0){
		std::cerr << "impossible to register spectrogram" << std::endl;
		exit(EXIT_FAILURE);
	}
	int len,lread=0;
	int flen=(options->fft_size)*sizeof(int16_t);
	int16_t* pulled_data = (int16_t *) malloc(flen);
	double* spectrum;
	int total_read=0;
	while((rb_get_read_space(data_buffer, 0)>0 | !reached_end())){
		spectrum=(double*) malloc(sizeof(double)*(options->fft_size));
		if(!spectrum)
			exit(1);
		pthread_mutex_lock(&data_writable_mutex);

		len=flen;
		lread=0;
		total_read=0;
		while(len>0){
			lread=rb_read(data_buffer, (uint8_t *) pulled_data+total_read, reader, (size_t) len);
			len-=lread;
			total_read+=lread;
			if(lread==0){
				break;
			}

		}
		pthread_cond_signal(&data_writable_cond);
		pthread_mutex_unlock(&data_writable_mutex); // Very important for the matching pthread_cond_wait() routine to complete
		
		spmanager->compute_spectrum(pulled_data, spectrum);
	}
	free(pulled_data);
}



void SndCleaner::compute_mel_spectrogram(){
	if(!spmanager->is_mel_flag_set()){
		std::cerr << "cannot compute spectrogram if flag mel unset in spmanager" << std::endl;
		exit(1);
	}
	Spectrogram* s = new Spectrogram(options->mel); // s is destroyed in the spmanager's destructor
	if(spmanager->register_spectrogram(s, OPEN_MODE_NORMAL)<0){
		std::cerr << "impossible to register spectrogram" << std::endl;
		exit(EXIT_FAILURE);
	}
	int len,lread=0;
	int flen=(options->fft_size)*sizeof(int16_t);
	int16_t* pulled_data = (int16_t *) malloc(flen);
	double* spectrum;
	int total_read=0;
	while((rb_get_read_space(data_buffer, 0)>0 | !reached_end())){
		spectrum=(double*) calloc(options->mel, sizeof(double)); // must be a calloc and not a malloc, because it is not zeroed out in the mel scale function, so the incrementation goes wrong
		if(!spectrum)
			exit(1);
		// std::cout << "spectrum allocated " << sizeof(double)*2048/2 << " bytes at address " << spectrum << std::endl;
		pthread_mutex_lock(&data_writable_mutex);
		// Open mode normal, only pointer is copied in the spectrogram
		// it is freed in the destructor, normally no memory leak
		len=flen;
		lread=0;
		total_read=0;
		while(len>0){
			lread=rb_read(data_buffer, (uint8_t *) pulled_data+total_read, 0, (size_t) len);
			len-=lread;
			total_read+=lread;
			if(lread==0 && reached_end()){
				//std::cout << "no more data" << std::endl;
				break;
			}

		}
		pthread_cond_signal(&data_writable_cond);
		pthread_mutex_unlock(&data_writable_mutex); // Very important for the matching pthread_cond_wait() routine to complete
		spmanager->compute_spectrum(pulled_data, spectrum);
	}

	free(pulled_data);
}


void* spectrogram_thread(void* arg){
	SndCleaner* sc = (SndCleaner*) arg;
	sc->compute_spectrogram();
}

void* lpc_thread(void* args){
	lpc_thread_arg* targs = (lpc_thread_arg*) args;
	SndCleaner* sc = (SndCleaner*) targs->sc;
	int reader = sc->register_reader();
	std::cout << "reader successfully allocated at: " << reader << std::endl;
	std::vector<float>* errors = sc->compute_lpc(reader);
	targs->errors=(float*)&(*errors)[0];
	targs->nb_errors=errors->size();
}


std::vector<float>* SndCleaner::compute_lpc(int reader){
	std::cout << "starting to compute lpc\n";
	float nb_milliseconds=10;
	float nb_millis_overlap=0; 
	float overlap=1-nb_millis_overlap/nb_milliseconds;
	int p = 10;
	int len,lread=0;
	int window_size = get_time_in_bytes(nb_milliseconds/1000);
	int overlap_size = get_time_in_bytes(nb_millis_overlap/1000);
	int flen=window_size;
	int16_t* pulled_data = (int16_t *) malloc(window_size);
	float coefs[p+1];
	std::vector<float>* errors = new std::vector<float>(0);
	fill_buffer();
	float error=0.;
	while(1){
		if(rb_get_read_space(data_buffer, reader)==0){
			if(fill_buffer()<=0)
				break;
		}
		len=flen;
		lread=0;
		while(len>0){
			lread=rb_read_overlap(data_buffer, (uint8_t *) pulled_data, reader, (size_t) len, overlap); 
			len-=lread;
			if(lread==0 && fill_buffer()<=0){
				break;
			}
		}
		lpc_filter_optimized(pulled_data, coefs, window_size/2, p, &error);
		errors->push_back(error);
	}
	free(pulled_data);
	return errors;
}

void spectrogram_with_lpc(SndCleaner* sc){
	lpc_thread_arg lpc_args;
	lpc_args.sc=sc;
	lpc_args.errors=NULL;
	lpc_args.nb_errors=0;
	sc->open_stream();

	pthread_t t_lpc;
	pthread_t t_spec;

	pthread_create(&t_lpc,
                   NULL,
                   lpc_thread,
                   (void *) &lpc_args);

	pthread_create(&t_spec,
                   NULL,
                   spectrogram_thread,
                   (void *) sc);

	pthread_join(t_lpc, NULL);
	pthread_join(t_spec, NULL);

	Spectrogram* s = sc->spmanager->get_spectrogram();
	s->initialize_for_rendering();
	//s->plot_up_to(8000, sc->get_sampling());
	int d1 = s->get_current_frame();
	int d2 = 371; // 8kHz cropping
	int l1 = lpc_args.nb_errors;
	double** data = s->get_data();
	plot_lpc_data("joint", data, d1, d2, lpc_args.errors, l1);

}


void test_bit_operations(){
	int i1 = 0b00101010110000110010101011000011;
	int i2 = 0b01001111010100110100111101010011;


	int diff = bit_error((void*) &i1, (void*) &i2, sizeof(int), 32, 1);

	std::cout << diff << std::endl;

	std::cout << bit_error((void*) &i1, (void*) &i2, sizeof(int), 10, 1) << std::endl;

	long li1 = 0b0010101011000011001010101100001100101010110000110010101011000011;
	long li2 = 0b0100111101010011010011110101001101001111010100110100111101010011;

	int ldiff = bit_error((void*) &li1, (void*) &li2, sizeof(long), 64, 1);

	std::cout << ldiff << std::endl;

	std::cout << bit_error((void*) &li1, (void*) &li2, sizeof(long), 33, 1) << std::endl;


	int* ai1 = (int*) malloc(2*sizeof(int));
	ai1[0]=i1;
	ai1[1]=i1;
	int* ai2 = (int*) malloc(2*sizeof(int));
	ai2[0]=i2;
	ai2[1]=i2;

	float adiff = bit_error_ratio((void*) ai1, (void*) ai2, sizeof(int), 32, 2);

	std::cout << adiff << std::endl;
}


void test_playback(SndCleaner* cleaner){
	cleaner->open_stream();

	pthread_t dump_thread;

	pthread_dump_arg dump_args;

	dump_args.len = STREAM_BUFFER_SIZE;
	dump_args.sc = cleaner;

	std::cout << "launching threads" << std::endl;

	pthread_create(&dump_thread,
                   NULL,
                   sc_dump_frames,
                   (void *) &dump_args);
	cleaner->player->start_playback();
}


void test_spectrogram(SndCleaner* cleaner){
	cleaner->open_stream();

	pthread_t dump_thread;

	pthread_dump_arg dump_args;

	dump_args.len = STREAM_BUFFER_SIZE;
	dump_args.sc = cleaner;

	std::cout << "launching threads" << std::endl;

	// pthread_create(&read_thread,
 //                   NULL,
 //                   sc_read_frames,
 //                   (void *) &cleaner);
	pthread_create(&dump_thread,
                   NULL,
                   sc_dump_frames,
                   (void *) &dump_args);
	//cleaner.player->start_playback();
	if(cleaner->get_mel_size()>0){
		cleaner->compute_mel_spectrogram();
		//cleaner->compute_spectrogram();
	}
	else{
		cleaner->compute_spectrogram();
	}
	
	pthread_join(dump_thread, NULL);
	std::cout << "dump thread joined" << std::endl;
	Spectrogram* s = cleaner->spmanager->get_spectrogram();
	s->initialize_for_rendering();

	s->plot_up_to(8000.f, cleaner->get_sampling());
	s->dump_in_bmp("spectrogram");
}

void test_mask(SndCleaner* cleaner){
	Spectrogram* s = cleaner->spmanager->get_spectrogram();
	int c = s->get_current_frame();
	uint8_t** bin = (uint8_t**) malloc(sizeof(uint8_t *)*c);
	for(int k=0; k<c; k++){
		bin[k] = (uint8_t*) malloc(sizeof(uint8_t)*1024);
		memset(bin[k], 0, sizeof(uint8_t)*1024);
	}
	Mask* m = alloc_mask(M1);
	apply_mask(s->get_data(), bin, c, 26, m, CROPPED);
	//print_mel_filterbank(1024, 26, 22050.);
	s->plot_binarized_spectrogram(bin);
	s->dump_in_bmp("maskspectrogram");

	free_msk(m);
	for(int k=0; k<c; k++){
		free(bin[k]);
	}
	free(bin);
}



void plot_byte_ratio(SndCleaner* sc1, SndCleaner* sc2){
	sc1->open_stream();
	sc2->open_stream();

	pthread_t dump_thread;

	pthread_dump_arg dump_args;

	dump_args.len = STREAM_BUFFER_SIZE;
	dump_args.sc = sc1;

	pthread_create(&dump_thread,
                   NULL,
                   sc_dump_frames,
                   (void *) &dump_args);
	sc1->compute_mel_spectrogram();
	pthread_join(dump_thread, NULL);
	dump_args.sc = sc2;
	pthread_create(&dump_thread,
                   NULL,
                   sc_dump_frames,
                   (void *) &dump_args);
	sc2->compute_mel_spectrogram();
	pthread_join(dump_thread, NULL);
	
	if(sc2->get_mel_size() != sc1->get_mel_size()){
		std::cerr << "mel size mismatch" <<std::endl;
	}
	int byte_size = 4*(sc1->get_mel_size()/32+1);
	const int nb_bits = sc1->get_mel_size();
	Spectrogram* s1 = sc1->spmanager->get_spectrogram();
	Spectrogram* s2 = sc2->spmanager->get_spectrogram();
	int c1 = s1->get_current_frame();
	int c2 = s2->get_current_frame();
	void* bin1 = calloc(c1, byte_size);
	void* bin2 = calloc(c2, byte_size);

	// s2->initialize_for_rendering();
	// s2->plot();


	float* ratios = (float*) calloc(c2-c1, sizeof(float));
	std::cout << "len of ratios: " << c2-c1 << std::endl;

	Mask* m = alloc_mask(M1);
	
	apply_mask_to_bit_value(s1->get_data(), bin1, byte_size, c1, nb_bits, m, CROPPED);
	apply_mask_to_bit_value(s2->get_data(), bin2, byte_size, c2, nb_bits, m, CROPPED);


	compute_bit_error_ratios((void*) bin1, (void*) bin2, ratios, byte_size, nb_bits, c1, c2);

	for(int i=0; i<c2-c1; i++){
		std::cout << ratios[i] << " ";
	}
	std::cout <<"\n";

	PLFLT* x= (PLFLT*) malloc(c2*sizeof(PLFLT));
	PLFLT* y= (PLFLT*) malloc(c2*sizeof(PLFLT));

    PLFLT xmin = 0., xmax = c2, ymin = 0., ymax = 1.0;

	int i;
	for(i=0;i<c2;i++){
		x[i]=i;
		y[i] = ratios[i];
	}

	plinit();

    // Create a labelled box to hold the plot.
    plenv( xmin, xmax, ymin, ymax, 0, 0 );
    //pllab( "x", "y=100 x#u2#d", "Simple PLplot demo of a 2D line plot" );

    // Plot the data that was prepared above.
    plline(c2-c1, x, y);

    // Close PLplot library
    plend();
	
	free_msk(m);

	free(bin1);
	free(bin2);
	free(ratios);
}


void find_in_stream(SndCleaner* sc1, SndCleaner* sc2){
	if(sc2->get_mel_size() != sc1->get_mel_size()){
		throw std::invalid_argument("Inconherent mel sizes");
	}

	if(sc2->get_fft_size() != sc1->get_fft_size()){
		throw std::invalid_argument("Inconherent fft sizes");
	}

	sc1->open_stream();
	sc2->open_stream();

	pthread_t dump_thread;

	pthread_dump_arg dump_args;

	dump_args.len = STREAM_BUFFER_SIZE;
	dump_args.sc = sc1;

	pthread_create(&dump_thread,
                   NULL,
                   sc_dump_frames,
                   (void *) &dump_args);
	sc1->compute_mel_spectrogram();
	pthread_join(dump_thread, NULL);
	dump_args.sc = sc2;
	pthread_create(&dump_thread,
                   NULL,
                   sc_dump_frames,
                   (void *) &dump_args);
	sc2->compute_mel_spectrogram();
	pthread_join(dump_thread, NULL);
	Spectrogram* s1 = sc1->spmanager->get_spectrogram();
	Spectrogram* s2 = sc2->spmanager->get_spectrogram();
	float* occurrences = (float*) malloc(4);
	Mask* m = alloc_mask(M3);
	int c1 = s1->get_current_frame();
	int c2 = s2->get_current_frame();

	int fft_size = sc1->get_fft_size();
	float sampling = (float) sc1->get_sampling();
	int d = find_time_occurences(&occurrences, s1->get_data(), s2->get_data(), sc2->get_mel_size(), 
		c1, c2, m, fft_size, sampling);

	if(d < -1){
		std::cerr << "An error happened" << std::endl;
	}
	std::cout << d << " occurrences found: " << std::endl;
	for(int i=0; i<d; i++){
		std::cout << occurrences[i] <<"s ";
	}
	std::cout << "\n";

	free(m);
	free(occurrences);
}

/*
	Test of a single threaded processing of the data. Applies the various audio processing 
	functions on the raw audio data.

*/
void test_processing_functions(SndCleaner* sc){
	sc->open_stream();
	float nb_milliseconds=50;
	int len,lread=0;
	int size=sc->get_max_byte(); // should be size_t really
	int window_size = sc->get_time_in_bytes(nb_milliseconds/1000);
	std::cout << "window size: " << window_size << std::endl;
	if(size=-1){
		size=sc->get_time_in_bytes(30);
	}
	int total_read=0;
	int flen=window_size;
	int16_t* pulled_data = (int16_t *) malloc(size);
	std::vector<double> rms_vector(0);
	std::vector<double> zc_vector(0);

	int nb_bands=10;
	int bands[nb_bands];
	memset((void *) bands, 0, nb_bands*sizeof(int));
	std::vector<float*> bands_vector(0);
	float* distrib;

	std::vector<double> centroids(0);
	RingBuffer* data_buffer = sc->data_buffer;
	sc->fill_buffer();

	while(1){
		if(rb_get_read_space(data_buffer, 0)==0){
			if(sc->fill_buffer() <=0)
				break;
		}
		//std::cout << rb_get_read_space(data_buffer, 0) << std::endl;
		len=flen;
		if(total_read+flen > size){
			distrib = (float*) calloc(nb_bands, sizeof(float)); // freed at the end
			
			compression_levels(pulled_data, total_read/2, bands, nb_bands);
			float s=(float) sum(bands, nb_bands);
			apply_coef(bands, distrib, 1/s, nb_bands);
			memset((void *) bands, 0, nb_bands*sizeof(int));
			bands_vector.push_back(distrib);
			total_read=0;
			centroids.push_back(compute_centroid(distrib, nb_bands, 2));
		}
		lread=0;
		while(len>0){
			// the cast only affects pulled_data so we can safely add lread without pointer arithmetic error
			lread=rb_read(data_buffer, (uint8_t *) pulled_data+total_read, 0, (size_t) len); 
			len-=lread;
			total_read+=lread;
			if(lread==0 && sc->fill_buffer()<=0){
				break;
			}
		}
		//rms_vector.push_back(root_mean_square(pulled_data+total_read/2-(flen-len)/2, (flen-len)/2));
		zc_vector.push_back(zero_crossing_rate(pulled_data+total_read/2-(flen-len)/2, (flen-len)/2));
	}
	std::cout << "out" << std::endl;
	//plot_color_map("radio", &bands_vector[0], bands_vector.size(), nb_bands);
	
	//plot_histogram(distrib, nb_bands);
	//plotData((double*)&rms_vector[0], rms_vector.size());
	plotData((double*)&zc_vector[0], zc_vector.size());
	//plotData((double*)&centroids[0], centroids.size());

	for(std::vector<float*>::iterator it = bands_vector.begin(); it != bands_vector.end(); ++it){
		free(*it);
	}

	free(pulled_data);
}



void test_lpc(SndCleaner* sc){
	sc->open_stream();
	float nb_milliseconds=10;
	float nb_millis_overlap=0; 
	float overlap=1-nb_millis_overlap/nb_milliseconds;
	int p = 10;
	int len,lread=0;
	int window_size = sc->get_time_in_bytes(nb_milliseconds/1000);
	int overlap_size = sc->get_time_in_bytes(nb_millis_overlap/1000);
	int flen=window_size;
	int16_t* pulled_data = (int16_t *) malloc(window_size);
	float coefs[p+1];
	std::vector<float> errors(0);
	RingBuffer* data_buffer = sc->data_buffer;
	sc->fill_buffer();
	std::cout << "window size is " << window_size << std::endl;
	float error=0.;
	while(1){
		if(rb_get_read_space(data_buffer, 0)==0){
			if(sc->fill_buffer() <=0)
				break;
		}
		len=flen;
		lread=0;
		while(len>0){
			lread=rb_read_overlap(data_buffer, (uint8_t *) pulled_data, 0, (size_t) len, overlap); 
			len-=lread;
			if(lread==0 && sc->fill_buffer()<=0){
				break;
			}
		}
		lpc_filter_optimized(pulled_data, coefs, window_size/2, p, &error);
		std::cout << error << std::endl;
		errors.push_back(error);
	}

	plotData((float*)&errors[0], errors.size());

	free(pulled_data);
}


void test_file_set(){
	FileSet f("./data", true);
	FileSetIterator it=f.begin();
	FileSetIterator end=f.end();
	for(;it!=end;++it){
		std::cout << *it << std::endl;
	}
}

/*
	For now only works with same local window sizes.
	TODO: allow for different local window sizes by declaring the 
	size of the pulled_data buffer as the smallest common multiple 
	of the window sizes, are dealing separately with each feature.


*/
void compute_features(SndCleaner* sc){
	sc->open_stream();
	float global_wdw_sz=2000;
	float local_wdw_sz=20; 
	float overlap=0;

	// number of local windows to make a global one
	int nb_frames_global=(int) global_wdw_sz/(local_wdw_sz*(1-overlap));
	// how many local windows have we stacked so far
	int nb_frames_current=0;

	std::cout << "nb frames global: " << nb_frames_global << std::endl;
	int len,lread=0;
	int p=10;
	int size=sc->get_time_in_bytes(local_wdw_sz/1000);
	int16_t* pulled_data = (int16_t *) malloc(size);

	int flen=size;
	float error=0.;
	float coefs[p+1];
	
	std::vector<double> rms_vector(0);
	std::vector<double> zc_vector(0);
	std::vector<float> lpc_vector(0);

	std::vector<distribution*> rms_distribs(0);
	std::vector<distribution*> zc_distribs(0);
	std::vector<distribution*> lpc_distribs(0);

	// pointers on the current distributions
	distribution* rms_distrib;
	distribution* zc_distrib;
	distribution* lpc_distrib;

	int rms_res=20;
	int zc_res=20;
	int lpc_res=20;
	RingBuffer* data_buffer = sc->data_buffer;
	sc->fill_buffer();

	while(1){
		//std::cout << rb_get_read_space(data_buffer, 0) << std::endl;
		
		if(nb_frames_current >= nb_frames_global){
			double rms_max=max_abs(&rms_vector[0], nb_frames_current);
			rms_distrib=dist_alloc(rms_res, rms_max);
			dist_incrementd(rms_distrib, &rms_vector[0], nb_frames_current);
			rms_distribs.push_back(rms_distrib);

			double zc_max=max_abs(&zc_vector[0], nb_frames_current);
			zc_distrib=dist_alloc(zc_res, zc_max);
			dist_incrementd(zc_distrib, &zc_vector[0], nb_frames_current);
			zc_distribs.push_back(zc_distrib);

			double lpc_max=(double) max_abs(&lpc_vector[0], nb_frames_current);
			lpc_distrib=dist_alloc(lpc_res, lpc_max);
			dist_incrementf(lpc_distrib, &lpc_vector[0], nb_frames_current);
			lpc_distribs.push_back(lpc_distrib);

			nb_frames_current=0;
			rms_vector.clear();
			zc_vector.clear();
			lpc_vector.clear();
		}
		if(rb_get_read_space(data_buffer, 0)==0){
			if(sc->fill_buffer() <=0)
				break;
		}

		len=flen;
		lread=0;
		while(len>0){
			// the cast only affects pulled_data so we can safely add lread without pointer arithmetic error
			//lread=rb_read_overlap(data_buffer, (uint8_t *) pulled_data, 0, (size_t) len, overlap); 
			lread=rb_read(data_buffer, (uint8_t *) pulled_data, 0, (size_t) len); 
			
			len-=lread;
			if(lread==0 && sc->fill_buffer()<=0){
				break;
			}
		}
		rms_vector.push_back(root_mean_square(pulled_data, (flen-len)/2));
		zc_vector.push_back(zero_crossing_rate(pulled_data, (flen-len)/2));
		lpc_filter_optimized(pulled_data, coefs, size/2, p, &error);
		lpc_vector.push_back(error);
		nb_frames_current++;
	}
	std::cout << "rms" << std::endl;
	for(std::vector<distribution*>::iterator it=rms_distribs.begin();
		it!=rms_distribs.end();++it){
		dist_print(*it);
		dist_free(*it);
	}
	std::cout << "zc" << std::endl;
	for(std::vector<distribution*>::iterator it=zc_distribs.begin();
		it!=zc_distribs.end();++it){
		dist_print(*it);
		dist_free(*it);
	}
	std::cout << "lpc" << std::endl;
	for(std::vector<distribution*>::iterator it=lpc_distribs.begin();
		it!=lpc_distribs.end();++it){
		dist_print(*it);
		dist_free(*it);
	}

	free(pulled_data);
}


int main(int argc, char *argv[]) {
	ProgramOptions poptions;
	bool test=false;
  	po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("with-playback", po::bool_switch(&poptions.with_playback)->default_value(false), "Should play back the input stream")
        ("size,s", po::value(&poptions.fft_size)->default_value(2048), "The size of the spectrum")
    	("input,i", po::value<std::vector<std::string>>(), "the names of the input files")
    	("take-half", po::bool_switch(&poptions.take_half)->default_value(true), "should we drop half of the spectrum")
    	("apply-window,w", po::bool_switch(&poptions.apply_window), "should an windowing function be applied")
    	("window-type", po::value(&poptions.window), "the windowing function to apply.\n\t1: rectangular\n\t2: blackmann\n\t4: hanning\n\t8: hamming")
     	("mel", po::value(&poptions.mel), "If set, the spectrum will be converted to mel scale with the specified number of windows")
        ("test", po::bool_switch(&test)->default_value(false), "run tests")
  		("find", po::bool_switch()->default_value(false), "tries to find arg1, in arg2")
  		("max", po::value(&poptions.max_time), "the maximum number of seconds to take into account")
    ;

	po::positional_options_description p;
	p.add("input", -1);

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).
	          options(desc).positional(p).run(), vm);
	po::notify(vm);

	if(vm.count("help")) {
		std::cout << desc << "\n";
		return 0;
    }

    if(test){
    	if(vm.count("input")){
	    	if(vm["input"].as<std::vector<std::string>>().size()==2){
		    	if(poptions.with_playback){
		    		std::cerr << "playback is not supported with two files" << std::endl;
		    		poptions.with_playback=false;
		    	}
		    	if(poptions.mel==-1)
		    		poptions.mel=26;
		    	poptions.filename=vm["input"].as<std::vector<std::string>>()[0];
		    	ProgramOptions poptions2;
		    	poptions2.fft_size = poptions.fft_size;
		    	poptions2.take_half=poptions.take_half;
		    	poptions2.mel=poptions.mel;
		    	poptions2.apply_window = poptions.apply_window;
		    	poptions2.window=poptions.window;
		    	poptions2.filename=vm["input"].as<std::vector<std::string>>()[1];
		    	SndCleaner sc1(&poptions);
		    	SndCleaner sc2(&poptions2);
		    	plot_byte_ratio(&sc1, &sc2);
		    	return 0;
		    }
		    poptions.filename=vm["input"].as<std::vector<std::string>>()[0];
    		SndCleaner sc(&poptions);
    		//test_spectrogram(&sc);
    		//spectrogram_with_lpc(&sc);
    		//test_mask(&sc);
    		//test_playback(&sc);
    		//test_processing_functions(&sc);
    		//test_lpc(&sc);
    		compute_features(&sc);
    		return 0;
    	}	
    	
    	//test_bit_operations();
    	
    	//test_solver();
    	//test_file_set();
    	return 0;
    }
    if(!vm.count("input")) {
    	std::cout << "YOU MUST SPECIFY AN INPUT FILE" << "\n";
		std::cout << desc << "\n";
		return 0;
    }

    if(vm.count("find")){
    	if(vm["input"].as<std::vector<std::string>>().size()==2){
	    	if(poptions.with_playback){
	    		std::cerr << "playback is not supported with two files" << std::endl;
	    		poptions.with_playback=false;
	    	}
	    	if(poptions.mel==-1)
	    		poptions.mel=26;
	    	poptions.filename=vm["input"].as<std::vector<std::string>>()[0];
	    	ProgramOptions poptions2;
	    	poptions2.fft_size = poptions.fft_size;
	    	poptions2.take_half=poptions.take_half;
	    	poptions2.mel=poptions.mel;
	    	poptions2.apply_window = poptions.apply_window;
	    	poptions2.window=poptions.window;
	    	poptions2.filename=vm["input"].as<std::vector<std::string>>()[1];
	    	SndCleaner sc1(&poptions);
	    	SndCleaner sc2(&poptions2);
	    	find_in_stream(&sc1, &sc2);
	    	return 0;
	    }
	    else{
	    	std::cout << "Not enough input arguments" << "\n";
			std::cout << desc << "\n";
			return 0;
	    }
    }
    std::cout << "inputs: " << vm.count("input") << std::endl;
   
	
	return 0;
}