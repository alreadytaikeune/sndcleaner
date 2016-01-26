#include "plotter.h"
#include "utils.h"
#include <iostream>

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


void plot_color_map(std::string filename, float** data, int d1, int d2){
	int w, h;
	int x_step, y_step;
	int rect_w, rect_h;
	int i,j;
	int grey=0;
	SDL_Rect r;

	SDL_Window* window = SDL_CreateWindow("Spectrum",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          1000, 500,
                          0);

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
  	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	// Clear window
    SDL_RenderClear(renderer);
    SDL_GetWindowSize(window, &w, &h);
    get_plot_dimensions(d1, d2, w, h, &x_step, &y_step, &rect_w, &rect_h);
	r.w=rect_w;
	r.h=rect_h;

	for(i=0; i < d1; i+=x_step){
		r.x=(i/x_step)*rect_w;
		for(j=0; j < d2; j+=y_step){
			r.y=(j/y_step)*rect_h;
			grey = data[i][j]*255;
			SDL_SetRenderDrawColor(renderer, 255-grey, 255-grey, 255-grey, 255);
			SDL_RenderFillRect(renderer, &r);
		}
	}
	SDL_RenderPresent(renderer);
	SDL_Delay(5000);
	dump_in_bmp(filename, window, renderer);
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
	int xnext;
	int ynext;
	for(i=0; i < d1; i+=x_step){
		r.x=(int) (i/x_step)*rect_wf;
		xnext=(int) (i/x_step+1)*rect_wf;
		r.w=xnext-r.x;
		for(j=0; j < d2-1; j+=y_step){
			r.y=(int) (j/y_step)*rect_h;
			//ynext = (int) (j/y_step+1)*rect_hf;
			//r.h=ynext-r.y;
			grey = (int) ((data[i][j]/150000.)*255);
			if(grey > 255)
				grey=255;
			SDL_SetRenderDrawColor(renderer, grey, grey, grey, 255);
			SDL_RenderFillRect(renderer, &r);
		}
	}
	SDL_RenderPresent(renderer);
	SDL_Delay(1000);
}


void plot_vector(SDL_Window* window, SDL_Renderer* renderer, 
	float* data, int d1, int w, int h, float p){

	int x_step=d1/w;
	int this_h = (int) h*p;
	if(x_step==0)
		x_step=0;
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