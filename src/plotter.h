#include <SDL2/SDL.h>
#include <string>


void plot_color_map(std::string filename, float** data, int d1, int d2);
void plot_lpc_data(std::string filename, double** data, int d1, int d2, float* lpc_errors, int l1);
