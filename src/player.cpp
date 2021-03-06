#include "player.h"
#include <stdio.h>
#include "ringbuffer.h"
#include <stdint.h>
#include <math.h>
#include "utils.h"
extern "C" {
#include <libavutil/time.h>
}


#define FF_REFRESH_EVENT (SDL_USEREVENT)
#define FF_QUIT_EVENT (SDL_USEREVENT + 1)
#define DEBUG_LEVEL 1

static int dl_free(DataList* dl){
	free(dl->data);
	free(dl);
}


static bool is_full(DataQueue* q){
	if(!q){
		std::cerr << "Packets queue is null" << std::endl;
		exit(EXIT_FAILURE);
	}
	if(q->nb_frames > q->nb_frames_max){
		std::cerr << "Inconherent state in queue, more packets than allowed" << std::endl;
		exit(EXIT_FAILURE);
	}
	return q->nb_frames == q->nb_frames_max;
	
}

static int data_queue_put(DataQueue* q, double* data, double pts){
	if(DEBUG_LEVEL > 1)
		printf("putting data at %p in queue, size is %d \n", data, q->nb_frames);
	int ret=0;
	if(q->nb_frames==q->nb_frames_max){
		return -1;
	}
	SDL_LockMutex(q->dataq_mutex);
	DataList* dl = (DataList *) malloc(sizeof(DataList));
	dl->next=NULL;
	if(!q->last_frame){
		q->first_frame=dl;
	}
	else{
		q->last_frame->next=dl;
	}
	q->last_frame=dl;
	q->last_frame->data=data;
	q->last_frame->pts=pts;
	q->nb_frames++;
	if(DEBUG_LEVEL > 1)
		printf("queued data list located at %p\n", dl);
	SDL_UnlockMutex(q->dataq_mutex);
	return ret;
}

static void queue_flush(DataQueue *q){
	SDL_LockMutex(q->dataq_mutex);
	DataList* pt = q->first_frame;
	while(pt){
		q->first_frame=pt->next;
		q->nb_frames--;
		dl_free(pt);
		pt=q->first_frame;
	}
	SDL_CondSignal(q->dataq_cond);
	SDL_UnlockMutex(q->dataq_mutex);
}

static void print_queue(DataQueue* q){
	if(!q)
		return;
	DataList* l = q->first_frame;
	while(l){
		printf("Data at %p: \n", l);
		printf("\t -data at: %p\n", l->data);
		printf("\t -pts: %lf\n", l->pts);
		printf("\n");
		l=l->next;
	}
	printf("\n\n");
}


static int data_queue_get(DataQueue* q, DataList* datal_ptr){
	if(DEBUG_LEVEL > 1)
		printf("getting queue head into %p queue size is %d\n", datal_ptr, q->nb_frames);
	int ret;
	SDL_LockMutex(q->dataq_mutex);
	DataList* fframe = q->first_frame;
	if(fframe){
		q->first_frame=fframe->next;
		if(!q->first_frame)
			q->last_frame=NULL;
		q->nb_frames--;
		*datal_ptr=*fframe;
		free(fframe);
		datal_ptr->next=NULL;
		ret=1;
	}
	else{
		ret=0;
	}
	SDL_CondSignal(q->dataq_cond);
	SDL_UnlockMutex(q->dataq_mutex);
	return ret;
}


/*
*	SDL's audio callback, a player instance is passed as userdata
*/
void audio_callback(void *userdata, Uint8 *stream, int len) {
	Player* pl = (Player*)userdata;
	int lread=0;
	pthread_mutex_lock(pl->data_mutex);
	while(len > 0 && !pl->quit) {
		lread=rb_read(pl->data_source, (uint8_t *) stream+lread, pl->stream_reader_idx, (size_t) len);
		len-=lread;
		if(lread==0 && pl->no_more_data){
  			(pl->event).type = SDL_QUIT;
  			pthread_mutex_unlock(pl->data_mutex);
  			SDL_PushEvent(&pl->event);
			return;
		}
	}
	pthread_cond_signal(pl->data_available_cond);
	pthread_mutex_unlock(pl->data_mutex); // Very important for the matching pthread_cond_wait() routine to complete	
	pl->audio_clock+=pl->audio_frame_duration;
}

static Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque) {
  SDL_Event event;
  event.type = FF_REFRESH_EVENT;
  event.user.data1 = opaque;
  SDL_PushEvent(&event);
  return 0; /* 0 means stop timer */
}


/* schedule a video refresh in 'delay' ms */
static void schedule_refresh(Player *pl, int delay) {
	SDL_AddTimer(delay, sdl_refresh_timer_cb, pl);
}

void video_refresh_timer(void *userdata) {
	Player* pl = (Player *) userdata;
	if(pl->quit)
		return;
	DataList* datal=(DataList*) malloc(sizeof(DataList));
	double actual_delay, delay, ref_clock, diff;
	if(!pl->data_source){
		printf("data source null\n");
		schedule_refresh(pl, 100);
		free(datal);
		return;
	}

	int s = data_queue_get(&(pl->dataq), datal);
	if(s>0){
		delay = datal->pts - pl->frame_last_pts;
		pl->frame_last_delay = delay;
      	pl->frame_last_pts = datal->pts;
      	/* update delay to sync to audio */
      	ref_clock = pl->get_audio_clock();
      	diff = datal->pts - ref_clock;
      	pl->frame_timer += delay;

      	/* computer the REAL delay */
      	actual_delay = pl->frame_timer - (av_gettime() / 1000000.0);

      	schedule_refresh(pl, (int)(actual_delay * 1000 + 0.5));
      	if(!datal->data){
      		printf("data null \n");
      	}
      	else{
        	pl->video_display(datal->data, (pl->dataq).frame_length);	
      	}

      	//printf("Freeing the data at %p\n", datal->data);
		free(datal->data);
	}
	else{
		schedule_refresh(pl, 1);
	}

	free(datal);
}


static int video_thread(void *arg) {
	printf("Starting video thread... \n");
	Player* pl = (Player*) arg;
	int16_t* pulled_data; // intermediary data for int16_t to double conversion in fft input
	double* spectrum;
	int len, lread=0;
	int ratio = sizeof(int16_t) / sizeof(uint8_t);
	int flen=FFT_SIZE*ratio;
	int i;
	pulled_data=(int16_t *) malloc(flen);
	printf("Entering video thread loop... \n");
	if(!(pl->data_source)){
		printf("null\n");
	}
	bool out=false;
	for(;;){
		spectrum=(double *) malloc(sizeof(double)*FFT_SIZE/2); // freed in the video thread
		len=flen;
		lread=0;

		pthread_mutex_lock(pl->data_mutex);
		while(len>0 && !pl->quit){
			lread=rb_read(pl->data_source, (uint8_t *) pulled_data+lread, pl->fft_reader_idx, (size_t) len);
			len-=lread;
			if(lread==0 && pl->no_more_data){
				std::cout << "no more data" << std::endl;
				out=true;
				break;
			}

		}
		pthread_cond_signal(pl->data_available_cond);
		pthread_mutex_unlock(pl->data_mutex); // Very important for the matching pthread_cond_wait() routine to complete
	
		if(out)
			break;
		if(pl->quit){
			printf("quiting...\n");
			break;
		}
		
		pl->spmanager->compute_spectrum(pulled_data, spectrum);

		double pts = pl->last_queued_pts+pl->video_frame_duration;
		SDL_LockMutex(pl->dataq.dataq_mutex);
		while(is_full(&pl->dataq) && !pl->quit){
			SDL_CondWait(pl->dataq.dataq_cond, pl->dataq.dataq_mutex);
		}
		SDL_UnlockMutex(pl->dataq.dataq_mutex);
		data_queue_put(&pl->dataq, spectrum, pts);
		pl->last_queued_pts = pts;

		//print_buffer_stats(pl->data_source);
	}
	free(pulled_data);
	printf("Out the video thread \n");
	return 0;
}


void Player::video_display(double* data, int l){
	SDL_Rect r;
	int w, h;
	int bottom_padding=0;
	int i;
	if(quit)
		return;
	SDL_GetWindowSize(window, &w, &h);
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255 );
	// Clear window
    SDL_RenderClear( renderer );

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255 );

    r.y=h-bottom_padding;
    r.w=1;
    int idx_max=l;
    if(w < l)
    	idx_max=w;

    // double spec_max = max(data, idx_max);
    double spec_max = 500000; /// arbitrary
    SDL_LockMutex(screen_mutex);
    double rh;
    for(i=0;i<idx_max;i++){
    	r.x=i;
    	rh = -data[i]*(h-bottom_padding)/spec_max;
    	if(rh < bottom_padding-h)
    		rh=bottom_padding-h;
    	r.h=rh;
    	SDL_RenderFillRect(renderer, &r );
    }

    SDL_RenderPresent(renderer);
    SDL_UnlockMutex(screen_mutex);
    video_clock+=video_frame_duration;
}

Player::Player(RingBuffer* b){
	data_source = b;
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    	fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    	exit(1);
  	}
  	window = SDL_CreateWindow("Spectrum",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          1025, 480,
                          0);
  	if(!window){
  		fprintf(stderr, "Could not create screen SDL - %s\n", SDL_GetError());
    	exit(1);
  	}

	renderer = SDL_CreateRenderer(window, -1, 0);
	screen_mutex = SDL_CreateMutex();
	if(renderer == NULL)
    {
        /* Handle problem */
        fprintf(stderr, "%s\n", SDL_GetError());
        SDL_Quit();
    }

  	data_queue_init(FFT_SIZE/2, DATA_QUEUE_SIZE); // divided by 2 we don't want the symetric part
	pthread_mutex_init(&quit_mutex, NULL);
  	pthread_cond_init (&quit_cond, NULL);

}


Player::~Player(){
	pthread_mutex_destroy(&quit_mutex);
	pthread_cond_destroy(&quit_cond);
}


void Player::data_queue_init(int fl, int size){
	memset(&dataq, 0, sizeof(DataQueue));
  	dataq.nb_frames_max=size;
  	dataq.dataq_mutex=SDL_CreateMutex();
  	dataq.dataq_cond=SDL_CreateCond();
  	dataq.frame_length=fl;
  	dataq.nb_frames=0;
}

void Player::set_stream_reader_idx(int idx){
	stream_reader_idx=idx;
}

void Player::set_fft_reader_idx(int idx){
	fft_reader_idx=idx;
}


double Player::get_audio_clock(){
	return audio_clock;
}

void Player::quit_all(){
	printf("quiting all \n");
	quit=1;
  	event.type = SDL_QUIT;
  	SDL_PushEvent(&event);
}


void Player::_quit_all(){
	int status=0;
	quit = 1;
	printf("joining thread\n");
	queue_flush(&dataq);
	SDL_WaitThread(video_tid, &status);
	printf("joined with status %d\n", status);
	pthread_mutex_lock(&quit_mutex);
	pthread_cond_signal(&quit_cond);
	pthread_mutex_unlock(&quit_mutex);
	pthread_mutex_lock(data_mutex);
	pthread_cond_signal(data_available_cond);
	pthread_mutex_unlock(data_mutex);
	SDL_Quit();
	std::cout << "quit all ok" << std::endl;
}


void Player::start_playback(){
	printf("Starting playback...");
	frame_timer = (double)av_gettime() / 1000000.0;
	frame_last_delay = ((double) FFT_SIZE)/sample_rate;
	schedule_refresh(this, 40);
	video_tid = SDL_CreateThread(video_thread, "video", this);
	SDL_PauseAudio(0); // start audio playback
	for(;;) {
		SDL_WaitEvent(&event);
    	switch(event.type) {
    	case FF_QUIT_EVENT:
    	case SDL_QUIT:
    		std::cout << "quit event received" << std::endl;
      		_quit_all();
      		return;
      		break;
    	case FF_REFRESH_EVENT:
      		video_refresh_timer(event.user.data1);
      		break;
    	default:
      		break;
    }
  }
  printf("quiting playback\n");
}


/*
*	Opens audio stream with wanted specs, the ringbuffer is written in the SndCleaner class with
	the format AV_SAMPLE_FMT_S16, which corresponds to the SDL AUDIO_S16SYS format
*/
int Player::open_with_wanted_specs(int sr, Uint8 channel_nb){
	SDL_AudioSpec wanted_spec, spec;
	wanted_spec.freq = sr;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = channel_nb;
    wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE; // defined in player.h
    wanted_spec.callback = audio_callback; // defined above
    wanted_spec.userdata = (void*) this;
    sample_rate=sr;
    audio_frame_duration = SDL_AUDIO_BUFFER_SIZE/((double) sample_rate);
   	video_frame_duration = FFT_SIZE/((double) sample_rate);
    if(SDL_OpenAudio(&wanted_spec, &spec) < 0) {
      fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
      exit(EXIT_FAILURE);
      return -1;
    }
    audio_hw_buf_size = spec.size;
    printf("SDL audio has been opened and is ready to play\n");
    return 0;

}


void Player::set_cond_wait_write_parameters(pthread_mutex_t* m, pthread_cond_t* c){
	data_mutex=m;
	data_available_cond=c;
}

void Player::set_spectrum_manager(SpectrumManager* sm){
	spmanager=sm;
	spmanager->set_fft_size(FFT_SIZE);
}

void Player::register_for_quit_callback(pthread_mutex_t* mut, pthread_cond_t* cond){
	mut=&quit_mutex;
	cond=&quit_cond;
}