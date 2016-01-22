#include "plotter.h"



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