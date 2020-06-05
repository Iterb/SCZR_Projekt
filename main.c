#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>

#define check printf("check\n")

double sobel_kernel[3*3] ={
	1.,0.,-1.,
	2.,0.,-2.,
	1.,0.,-1.,
};
void check3DArray (int *** array, int height, int width, int bpp)
{

	for ( int i = 0; i < width; i++){
		for ( int j = 0; j < height; j++){
			printf ("%d ", 1+i*height+ j);
			printf ("[");
			for ( int k = 0; k < bpp; k++)
				printf ("%d ", array[i][j][k]);
			printf ("\b] ");
			printf ("\n");
		}
	}
}

void check2DArray (int ** array, int height, int width)
{

	for ( int i = 0; i < width; i++){
		printf ("[");
		for ( int j = 0; j < height; j++){
			printf ("%d ", array[i][j]);
		}
		printf ("\b]\n");
	}
}

int saveFile (char * save, int ** array, int height, int width)
{
	char * gray_img = (char*) malloc (width*height*sizeof(char));
	int gray_channels = 1;

	int k = 0;

	for ( int i = 0; i < width; i++){
		for ( int j = 0; j < height; j++){
			*(gray_img+k) = array[i][j];
			k++;
		}
	}

	stbi_write_png(save, width, height, gray_channels, gray_img, width*gray_channels);
	free (gray_img);
}


int ** sobelFilter (int *** array, int height, int width, double * K){
	unsigned int ix, iy, l;
	int kx, ky;
	double cp[3];

	int ** im_vertical = (int**) malloc (width*sizeof(int*) );
	for ( int i=0; i < width; i++){
		im_vertical[i] = (int*) malloc (height*sizeof(int) );
	}
	if (im_vertical != NULL) {
		for ( ix = 0; ix < width; ix++) {
			for ( iy = 0; iy < height; iy++){
				cp[0] = cp[1] = cp[2] = 0.0;
				for ( kx = -1; kx <= 1; kx++){
					for ( ky = -1; ky <= 1; ky++){
						for ( l = 0; l < 3; l++){
							if (!(((iy+ky)>height || (iy+ky)<1) || ((ix+kx)>width || (ix+kx)<1)))
								cp[l] += K[(kx+1)+(ky+1)*3] * array[ix+kx-1][iy+ky-1][l];
						}
					}
				}
				for ( l=0; l<3; l++){
					cp[l] = (cp[l]>255.0) ? 255.0 : ((cp[l]<0.0) ? 0.0 : cp[l]);
				}
				//grayscale conversion
				im_vertical[ix][iy]=0.299*cp[0] + 0.587*cp[1]+ 0.144*cp[2];
				//printf("%d,", im_vertical[ix][iy]);
			}
		}
	}
	return im_vertical;
}
int ** processFile (int *** array, int height, int width, int bpp)
{
	int ** im_vertical = sobelFilter (array, height, width, sobel_kernel);
	return im_vertical;
}

int *** readFile (char * filename, int * width, int * height, int * bpp)
{
	char * data = stbi_load(filename, width, height, bpp,3);

	if(data==NULL)
		{
		printf("blad wczytywania pliku");
		exit(1);
		}

	int ***array = (int***)malloc((*width) * sizeof(int**));			//array[width][height][color]
	int i, j;

	for (i = 0; i < (*width); i++) {

		array[i] = (int**)malloc((*height) * sizeof(int*));
		for (j = 0; j < (*height); j++) {
			array[i][j] = (int*)malloc(3 * sizeof(int));
		}
	}
	for (int i = 0; i < (*width); i++) {
		for (int j = 0; j < (*height); j++) {
			unsigned char* pixelOffset = data + (i + (*height) * j) * 3;
			array[i][j][0] = pixelOffset[0];
			array[i][j][1] = pixelOffset[1];
			array[i][j][2] = pixelOffset[2];
		}
	}
	return array;
}

int proceed ()
{
	int height = 0, width = 0, bpp = 0;
	int *** file = readFile("pic.png", &width, &height, &bpp);
	//check3DArray(file, height, width, bpp);
	int ** file_2D = processFile(file, height, width, bpp);
	//check2DArray(file_2D, height, width);
	saveFile("save.png", file_2D, height, width);
}

int main() {

	proceed();

	return 0;
}
