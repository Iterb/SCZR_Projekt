#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>

#define check printf("check\n")

unsigned char * data;

void checkArray (int *** array, int height, int width, int bpp)
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

int saveFile (char * save, int *** array, int height, int width, int bpp)
{
	size_t img_size = width * height * bpp;
	int gray_channels = bpp == 4 ? 2 : 1;
	size_t gray_img_size = width * height * gray_channels;

	unsigned char * gray_img = malloc ( gray_img_size);
	if ( gray_img == NULL)
	{
		printf ("Blad alokacjin");
		exit(-2);
	}

	check;

	for ( unsigned char * p = data, *pg = gray_img; p != data + img_size; p+= bpp, pg += gray_channels){
		*pg = (uint8_t)((*p + *(p+1) + *(p+2))/3.0);
		if (bpp == 4){
			*(pg+1) = *(p+3);
		}
	}

	check;
	
	stbi_write_png(save, width, height, gray_channels, gray_img, width*gray_channels);

	free(gray_img);
}

int *** processFile (int *** array, int height, int width, int bpp)
{
	return array;
}

int *** readFile (char * filename, int * width, int * height, int * bpp)
{
	data = stbi_load(filename, width, height, bpp,3);

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
	//checkArray(file, height, width, bpp);
	processFile(file, height, width, bpp);
	saveFile("save.png", file, height, width, bpp);
}

int main() {
	
	proceed();

	return 0;
}
