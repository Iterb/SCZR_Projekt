#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>

#include <time.h>

#define check printf("check\n")

#define BILLION 1000000000L;

double sobel_kernel[3*3] ={
	1.,0.,-1.,
	2.,0.,-2.,
	1.,0.,-1.,
};

int saveFile (char * save, int ** array, int height, int width, int bpp)
{

	int gray_channels = 1;
	int gray_img_size = height * width * gray_channels;

	unsigned char * gray_img = (char*) malloc (gray_img_size);

	unsigned char * pg = gray_img;

	for ( int i = 0; i < height; i++){
		for ( int j = 0; j < width; j++, pg+=gray_channels){
		*pg = (array[i][j]);
		if (gray_channels == 3)
			{
				*(pg+1) = array[i][j];
				*(pg+2) = array[i][j];
			}
		}
	}

	stbi_write_png(save, width, height, gray_channels, gray_img, width*gray_channels);
	free (gray_img);
}

int ** fakeProceed ( int *** array, int height, int width)
{
	int ** im_vertical = (int**) malloc (height * sizeof(int*));
		for ( int i=0; i < height; i++){
			im_vertical[i] = (int*) malloc (width*sizeof(int) );
	}

	for ( int i = 0; i < height; i++){
		for ( int j = 0; j < width; j++)
		{
			im_vertical[i][j] = 0.3 * array[i][j][0] + 0.59 * array[i][j][1] + 0.11 * array[i][j][2];
		}
	}
	return im_vertical;
}

int ** sobelFilter (int *** array, int height, int width, double * K){
	unsigned int ix, iy, l;
	int kx, ky;
	double cp[3];

	int ** im_vertical = (int**) malloc (height*sizeof(int*) );
	for ( int i=0; i < height; i++){
		im_vertical[i] = (int*) malloc (width*sizeof(int) );
	}
	if (im_vertical != NULL) {
		for ( ix = 0; ix < height; ix++) {
			for ( iy = 0; iy < width; iy++){
				cp[0] = cp[1] = cp[2] = 0.0;
				for ( kx = -1; kx <= 1; kx++){
					for ( ky = -1; ky <= 1; ky++){
						for ( l = 0; l < 3; l++){
							if (!(((iy+ky)>width || (iy+ky)<1) || ((ix+kx)>height || (ix+kx)<1)))
								cp[l] += K[(kx+1)+(ky+1)*3] * array[ix+kx-1][iy+ky-1][l];
						}
					}
				}
				for ( l=0; l<3; l++){
					cp[l] = (cp[l]>255.0) ? 255.0 : ((cp[l]<0.0) ? 0.0 : cp[l]);
				}
				//grayscale conversion
				im_vertical[ix][iy]=0.3*cp[0] + 0.59*cp[1]+ 0.11*cp[2];
				//printf("%d,", im_vertical[ix][iy]);
			}
		}
	}
	return im_vertical;
}

int ** processFile (int *** array, int height, int width, int bpp, double * sobel_time)
{

	struct timespec start, stop;

	if ( clock_gettime (CLOCK_REALTIME, &start ) == -1 ){
		perror ("clock gettime");
		exit(EXIT_FAILURE);
	}

	int ** im_vertical = sobelFilter (array, height, width, sobel_kernel);

	if ( clock_gettime (CLOCK_REALTIME, &stop ) == -1 ){
		perror ("clock gettime");
		exit(EXIT_FAILURE);
	}

	*sobel_time = (stop.tv_sec - start.tv_sec ) + (double)(stop.tv_nsec - start.tv_nsec) / BILLION;

	return im_vertical;
}

int *** readFile (char * filename, int * width, int * height, int * bpp, double * read_time)
{

	struct timespec start, stop;

	if ( clock_gettime (CLOCK_REALTIME, &start ) == -1 ){
		perror ("clock gettime");
		exit(EXIT_FAILURE);
	}

	unsigned char * data = stbi_load(filename, width, height, bpp,3);

	if(data==NULL)
		{
		printf("blad wczytywania pliku");
		exit(1);
		}

	int *** array = (int***)malloc((*height) * sizeof(int**)); //array[height][width][color]
	int i, j;

	for (i = 0; i < (*height); i++) {

		array[i] = (int**)malloc((*width) * sizeof(int*));
		for (j = 0; j < (*width); j++) {
			array[i][j] = (int*)malloc(3 * sizeof(int));
		}
	}

	for (int i = 0; i < (*height); i++) {
		for (int j = 0; j < (*width); j++) {
			unsigned char* pixelOffset = data + (j + (*width) * i) * 3;
			array[i][j][0] = pixelOffset[0];
			array[i][j][1] = pixelOffset[1];
			array[i][j][2] = pixelOffset[2];
		}
	}

	if ( clock_gettime (CLOCK_REALTIME, &stop ) == -1 ){
		perror ("clock gettime");
		exit(EXIT_FAILURE);
	}

	*read_time = (stop.tv_sec - start.tv_sec ) + (double)(stop.tv_nsec - start.tv_nsec) / BILLION;

	return array;
}

void saveTimer ( char * read_timers_name, char * sobel_timers_name, double read_time, double sobel_time)
{
	FILE * read_timers = fopen (read_timers_name, "a");
	FILE * sobel_timers = fopen (sobel_timers_name, "a");

	if ( read_timers == NULL || sobel_timers == NULL)
	{
		perror ("Error opening file.");
	}
	else
	{
		fprintf(read_timers, "%lf\n", read_time);
		fprintf(sobel_timers, "%lf\n", sobel_time);	
	}

	fclose (read_timers);
	fclose (sobel_timers);
}

int proceed ( char * read_name, char * save_name, char * read_timers_name, char * sobel_timers_name)
{
	double read_time, sobel_time;
	int height = 0, width = 0, bpp = 0;
	int *** file = readFile(read_name, &width, &height, &bpp, &read_time);
	int ** file_2D = processFile(file, height, width, bpp, &sobel_time);
	//int ** file_2D = fakeProceed(file, height, width);
	saveFile(save_name, file_2D, height, width, bpp);
	saveTimer(read_timers_name, sobel_timers_name, read_time, sobel_time);
}

int main( int ** argc, char ** argv) {
	proceed(argv[1], argv[2], argv[3], argv[4]);
	return 0;
}
