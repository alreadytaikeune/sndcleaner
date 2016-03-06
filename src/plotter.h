#ifdef __cplusplus
extern "C" {
#endif
#include <plplot/plConfig.h>
#include <plplot/plplot.h>
#ifdef __cplusplus 
}
#endif

#include <SDL2/SDL.h>
#include <string>

void plot_color_map(std::string filename, double** data, int d1, int d2);
void plot_lpc_data(std::string filename, double** data, int d1, int d2, float* lpc_errors, int l1);
void plot_color_map2(SDL_Renderer* renderer, double** data, int d1, int d2, int w, int h);
template<typename T>
void plotData(T* data, int len){
	PLFLT x[len];
	PLFLT y[len];

	PLFLT max_value = (PLFLT) max_abs(data, len);
    PLFLT xmin = 0., xmax = len, ymin = 0., ymax = max_value;
    int i,j;
    int mean = 0;
    for (i = 0; i < len; i++)
    {
    	x[i] = (PLFLT) i;
        y[i] = (PLFLT) data[i];
    }

    // Initialize plplot
    plinit();

    // Create a labelled box to hold the plot.
    plenv( xmin, xmax, ymin, ymax, 0, 0 );

    // Plot the data that was prepared above.
    plline(len, x, y);

    // Close PLplot library
    plend();
}

void plfbox(PLFLT x0, PLFLT y0);

template <typename T>
void plot_histogram(T* data, int nb_bands){
	char string[20];
	plinit();
    pladv(0);
    plvsta();
    plwind(0, nb_bands, 0.0, 1.5);
    for(int i=0;i<nb_bands;i++){
    	plcol1(i / (PLFLT)nb_bands);
    	plpsty( 0 );
    	plfbox((PLFLT) i, (PLFLT) data[i] );
    	sprintf( string, "%.2f", data[i] );
    	plptex(i + .5, data[i] + 0.1, 1.0, 0.0, .5, string);
    }
    plend();
}


void vocoder_model_plotter(float** coefs, int n, int p, float freq_max, float sample_rate, int spec_size, double** spec);
