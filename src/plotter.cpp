#include "plotter.h"
#include "utils.h"
#include <iostream>
#include <complex>
#include <math.h>
#include <stdio.h>

void dump_in_bmp(std::string filename, SDL_Window* window, SDL_Renderer* renderer){
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	SDL_Surface *sshot = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, sshot->pixels, sshot->pitch);
	SDL_SaveBMP(sshot, (const char *) (filename+".bmp").c_str());
	SDL_FreeSurface(sshot);
}

void get_plot_dimensions(int d1, int d2, int w, int h, int* x_step, int* y_step, int* rect_w, int* rect_h){
	*x_step = d1/w;
	if(*x_step==0)
		*x_step=1;
	*rect_w=w/d1;
	if(*rect_w==0)
		*rect_w=1;

	*y_step=d2/h;
	if(*y_step==0)
		*y_step=1;
	*rect_h=h/d2;
	if(*rect_h==0)
		*rect_h=1;
}


void plot_color_map(std::string filename, double** data, int d1, int d2){
	int w, h;
	int x_step, y_step;
	int rect_w, rect_h;
	int i,j;
	int grey=0;
	SDL_Rect r;

	SDL_Window* window = SDL_CreateWindow("Spectrum",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          2*d1, d2,
                          0);

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
  	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	// Clear window
    SDL_RenderClear(renderer);
    SDL_GetWindowSize(window, &w, &h);
   	plot_color_map2(renderer, data, d1, d2, w, h);
   	SDL_Delay(5000);
	dump_in_bmp(filename, window, renderer);
	SDL_DestroyRenderer(renderer);
  	SDL_DestroyWindow(window);
	SDL_Quit();

}

void plot_color_map2(SDL_Renderer* renderer, double** data, int d1, int d2, int w, int h){
	int x_step, y_step;
	int rect_w, rect_h;
	int i,j;
	int grey=0;
	SDL_Rect r;

  	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	// Clear window
    SDL_RenderClear(renderer);
    get_plot_dimensions(d1, d2, w, h, &x_step, &y_step, &rect_w, &rect_h);
	r.w=rect_w;
	r.h=rect_h;
	float rect_wf = (float) w/d1;
	float rect_hf = (float) h/d2;
	std::cout << "rect_w: " << rect_w << std::endl;
	std::cout << "rect_h: " << rect_h << std::endl;
	std::cout << "x_step: " << x_step << std::endl;
	std::cout << "y_step: " << y_step << std::endl;
	std::cout << "w: " << w << std::endl;
	std::cout << "h: " << h << std::endl;
	r.y=0;
	r.x=0;
	int xnext=0;
	int ynext=0;
	for(i=0; i < d1; i+=x_step){
		r.x=(int) (i/x_step)*rect_wf;
		xnext=(int) ceil((i/x_step+1)*rect_wf);
		r.w=xnext-r.x;
		ynext=0;
		for(j=0; j < d2-1; j+=y_step){
			r.y=(int) (j/y_step)*rect_hf;
			ynext=(int) ceil((j/y_step+1)*rect_hf);
			r.h=ynext-r.y;
			//ynext = (int) (j/y_step+1)*rect_hf;
			//r.h=ynext-r.y;
			grey = (int) ((data[i][j]/300000.)*255);
			if(grey > 255)
				grey=255;
			SDL_SetRenderDrawColor(renderer, grey, grey, grey, 255);
			SDL_RenderFillRect(renderer, &r);
		}
	}
	SDL_RenderPresent(renderer);
}


void plot_color_map3(SDL_Renderer* renderer, double* data, int d1, int d2, int w, int h, int k, int nk){
	int x_step, y_step;
	int rect_w, rect_h;
	int j;
	int grey=0;
	SDL_Rect r;
    get_plot_dimensions(d1, d2, w, h, &x_step, &y_step, &rect_w, &rect_h);
	r.w=rect_w;
	r.h=rect_h;
	float rect_wf = (float) w/d1;
	float rect_hf = (float) h/d2;
	r.y=0;
	r.x=0;
	int xnext=0;
	int ynext=0;
	r.x = (int) ((float(k)/nk)*w);;
	xnext=(int) ((float(k+1)/nk)*w);
	r.w=xnext-r.x;
	for(j=0; j < d2-1; j+=y_step){
		r.y=(int) (j/y_step)*rect_hf;
		ynext=(int) (j/y_step+1)*rect_hf;
		r.h=ynext-r.y;
		//ynext = (int) (j/y_step+1)*rect_hf;
		//r.h=ynext-r.y;
		grey = (int) ((data[j]/200000.)*255);
		if(grey > 255)
			grey=255;
		SDL_SetRenderDrawColor(renderer, grey, grey, grey, 255);
		SDL_RenderFillRect(renderer, &r);
	}
	SDL_RenderPresent(renderer);
}


void plot_vector(SDL_Window* window, SDL_Renderer* renderer, 
	float* data, int d1, int w, int h, float p){

	int x_step=d1/w;
	int this_h = (int) h*p;
	if(x_step==0)
		x_step=1;
	float m = max(data, d1);
	std::cout << "max is: " << m << std::endl;
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_Rect r;
	r.h=1;
	r.w=1;
	for(int i=0;i<d1-1;i+=x_step){
		r.x= i/x_step;
		r.y= h-this_h*data[i]/m;
		SDL_RenderFillRect(renderer, &r);
		SDL_RenderDrawLine(renderer, i/x_step, 
			h-this_h*data[i]/m, i/x_step+1, 
			h-this_h*data[i+1]/m);
	}
	SDL_RenderPresent(renderer);
}


void plot_lpc_data(std::string filename, double** data, int d1, int d2, float* lpc_errors, int l1){
	SDL_Window* window = SDL_CreateWindow("Spectrum",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          1000, 625,
                          0);

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
  	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	// Clear window
    SDL_RenderClear(renderer);
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
    SDL_Rect specViewport; 
    specViewport.x = 0; 
    specViewport.y = 0; 
    specViewport.w = w; 
    specViewport.h = h*0.8;
    SDL_Rect lpcViewport; 
    specViewport.x = 0; 
    specViewport.y = h*0.8; 
    specViewport.w = w; 
    specViewport.h = h*0.2; 
    //SDL_RenderSetViewport(renderer, &specViewport);
    plot_color_map2(renderer, data, d1, d2, w, (int)h*0.8);
    //SDL_RenderSetViewport(renderer, &lpcViewport);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderFillRect(renderer, &lpcViewport);
    plot_vector(window, renderer, lpc_errors, l1, w, h, 0.2);

    SDL_Delay(5000);
	dump_in_bmp(filename, window, renderer);
	SDL_DestroyRenderer(renderer);
  	SDL_DestroyWindow(window);

	SDL_Quit();
}

void plfbox(PLFLT x0, PLFLT y0){
    PLFLT x[4], y[4];

    x[0] = x0;
    y[0] = 0.;
    x[1] = x0;
    y[1] = y0;
    x[2] = x0 + 1.;
    y[2] = y0;
    x[3] = x0 + 1.;
    y[3] = 0.;
    plfill( 4, x, y );
    plcol0( 1 );
    pllsty( 1 );
    plline( 4, x, y );
}



/*

	H(z) =  G*(1+\sum_{k}^{p}a_{k}*z^{-k})^{-1}
*/
void plot_filter_data(float* coefs, int p, float* filt_out, int w, int h, int step, float freq_step, SDL_Renderer* renderer, float sample_rate){
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	// Clear window
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

	for(int k=0; k<w; k++){
		std::complex<float> denom(1.f, 0.f);
		for(int l=1; l<p; l++){
			//G=1
			denom+=std::complex<float>((float) cos(k*l*freq_step*2*M_PI/sample_rate), (float) -sin(k*l*freq_step*2*M_PI/sample_rate))*coefs[l];
		}
		filt_out[k] = (float) 20*log(norm(pow(denom,-1)));
	}

	SDL_Rect r;
	r.h=1;
	r.w=1;
	r.x=0;
	r.y=h;
	for(int k=0; k<w; k++){
		int x=k*step;
		int y=(int) (h-filt_out[k]*h/150);
		SDL_RenderDrawLine(renderer, r.x, r.y, x, y);
		r.x=x;
		r.y=y;
		SDL_RenderFillRect(renderer, &r);
	}
	SDL_RenderPresent(renderer);

}

/*
	Spectrum's size should be the one of the window
*/
void plot_vocoder_spectrum(double* spec, int n, int w, int h, int step, SDL_Renderer* renderer){
	SDL_Rect r;
	r.h=1;
	r.w=1;
	r.x=0;
	r.y=h;
	SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
	for(int k=0; k<n; k++){
		int x=k*step;
		int y=(int) (h-10*log(spec[k])*h/150);
		SDL_RenderDrawLine(renderer, r.x, r.y, x, y);
		r.x=x;
		r.y=y;
		SDL_RenderFillRect(renderer, &r);
	}
	SDL_RenderPresent(renderer);
}


void update_cursor(int k, int nk, int w, int h, SDL_Renderer* renderer){
	SDL_Rect r;
	r.w=1;
	r.h=h;
	r.x = (int) ((float(k)/nk)*w);
	r.y=0;
	SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
	SDL_RenderFillRect(renderer, &r);
	SDL_RenderPresent(renderer);
}


void vocoder_model_plotter(float** coefs, int n, int p, float freq_max, float sample_rate, int spec_size, double** spec){
	int w=4*spec_size;
	int h=4*spec_size;
	int w2=2*n;
	int h2=2*spec_size;
	SDL_Window* window2 = SDL_CreateWindow("Vocoder",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          w2, h2,
                          0);

	SDL_Renderer* renderer2 = SDL_CreateRenderer(window2, -1, 0);

	SDL_Window* window = SDL_CreateWindow("Vocoder",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          w, h,
                          0);

	if(window==NULL){
		std::cerr << "error allocating window" << std::endl;
		exit(1);
	}
	
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
	printf("clearing screen\n");
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
	SDL_SetRenderDrawColor(renderer2, 255, 255, 255, 255);
    SDL_RenderClear(renderer2);
    SDL_RenderPresent(renderer);
    SDL_RenderPresent(renderer2);

	plot_color_map2(renderer2, spec, n, spec_size/2, w2, h2);
	update_cursor(0, n, w2, h2, renderer2);
	

	float* filt_out = (float*) malloc(w*sizeof(float));
	float freq_step = freq_max/w;
	char title[100];
	int k=0, quit=0;
	plot_filter_data(coefs[0], p, filt_out, w, h, 2,freq_step, renderer, sample_rate);
	SDL_RenderPresent(renderer);


	while (!quit) {
	    SDL_Event event;
	    int old_k=k;
	    while (SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_QUIT:
					quit=1;
					break;
				case SDL_WINDOWEVENT_CLOSE:
					quit=1;
					break;
				case SDL_KEYDOWN:
					if(event.key.keysym.sym == SDLK_RIGHT){
						k+=1;		
					}
					else if(event.key.keysym.sym == SDLK_LEFT){
						k-=1;		
					}
					if(k>=n){
						k=n-1;
					}
					if(k<0){
						k=0;
					}
					if(k!=old_k){
						snprintf(title, 100, "transfer n°%d", k);
						SDL_SetWindowTitle(window, (const char*) title);

						plot_filter_data(coefs[k], p, filt_out, w, h, w/spec_size, freq_step, renderer, sample_rate);
						plot_vocoder_spectrum(spec[k], spec_size, w, h, w/spec_size, renderer);
						plot_color_map3(renderer2, spec[old_k], n, spec_size/2, w2, h2, old_k, n);
						update_cursor(k, n, w2, h2, renderer2);
					}
					break;
				default:
				break;
			}
	    }
	}



	free(filt_out);
	SDL_DestroyRenderer(renderer);
  	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer2);
  	SDL_DestroyWindow(window2);
	SDL_Quit();
}