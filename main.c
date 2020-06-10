#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <mqueue.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "common.h"


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define check printf("check\n")
#define BILLION 1000000000L;

double sobel_kernel[3*3] ={
	1.,0.,-1.,
	2.,0.,-2.,
	1.,0.,-1.,
};

double sobel_kernel2[3*3] ={
	1.,2.,1.,
	0.,0.,0.,
	-1.,-2.,-1.,
};

int *** readFile (char * filename, int * width, int * height, int * bpp, double * read_time);
int proceed ( char * read_name, char * save_name, char * read_timers_name, char * sobel_timers_name);

int ** thresholdImage(int ** array, int height, int width, int TH);
int ** sobelFilter (int *** array, int height, int width, double * K, double * sobel_time);

int saveFile (char * save, int ** array, int height, int width, int bpp);
void saveTimer ( char * read_timers_name, char * sobel_timers_name, double read_time, double sobel_time);
void startProcesses();


int main( int ** argc, char ** argv) {

	startProcesses();

	return 0;
}

int proceed ( char * read_name, char * save_name, char * read_timers_name, char * sobel_timers_name)
{
	double read_time, sobel_time;
	int height = 0, width = 0, bpp = 0;

	int *** file = readFile(read_name, &width, &height, &bpp, &read_time);

	int ** file_2D = sobelFilter(file, height, width, sobel_kernel2, &sobel_time);
	file_2D= thresholdImage(file_2D, height, width, 160);

	saveFile(save_name, file_2D, height, width, bpp);
	saveTimer(read_timers_name, sobel_timers_name, read_time, sobel_time);
}

void producerProcess(){
	printf("COZ");
	mqd_t mq;
	char buffer[MAX_SIZE];
	double read_time, sobel_time;
	int height = 0, width = 0, bpp = 0;
	int i=0,ix=0,iy=0;

	int *** array = readFile("images/pic.png", &width, &height, &bpp, &read_time);
	for ( ix = 0; ix < height; ix++) {
		for ( iy = 0; iy < width; iy++){
			b = (char*)array[ix][iy] + '0';
			buffer[ix + iy*height] = *b;
		}
	}


	/* open the mail queue */
	mq = mq_open(QUEUE_NAME, O_WRONLY);
	CHECK((mqd_t)-1 != mq);
	printf("Mq_otwarte producer");

	while ( i < 1000){
			//saveTimer("timers/read_timer.txt", "sobel_timers.txt", read_time, sobel_time);
			memset(buffer, 0, MAX_SIZE);
			fgets(buffer, MAX_SIZE, stdin);

			/* send the message */
			CHECK(0 <= mq_send(mq, buffer, MAX_SIZE, 0));
		i++;
	}
	/* cleanup */
	CHECK((mqd_t)-1 != mq_close(mq));
	printf("Koniec");
}
void clientProcess(){
	mqd_t mq;
  struct mq_attr attr;
  char buffer[MAX_SIZE + 1];
  int must_stop = 0;

  /* initialize the queue attributes */
  attr.mq_flags = 0;
  attr.mq_maxmsg = 10;
  attr.mq_msgsize = MAX_SIZE;
  attr.mq_curmsgs = 0;

  /* create the message queue */
  mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
	CHECK((mqd_t)-1 != mq);
	printf("Mq_otwarte client");

  do {
      ssize_t bytes_read;
			CHECK(bytes_read >= 0);
      /* receive the message */
      bytes_read = mq_receive(mq, buffer, MAX_SIZE, NULL);

      buffer[bytes_read] = '\0';
      if (! strncmp(buffer, MSG_STOP, strlen(MSG_STOP)))
      {
          must_stop = 1;
      }
      else
      {
          printf("Received: %s\n", buffer);
      }
  } while (!must_stop);

  /* cleanup */
	CHECK((mqd_t)-1 != mq_close(mq));
	CHECK((mqd_t)-1 != mq_unlink(QUEUE_NAME));

}
void archiverProcess(){
	printf("Hello from archiver process\n");
}

void startProcesses(){

	pid_t PID_A;
	PID_A = fork();
	pid_t PID_B;
	PID_B = fork();


	if (PID_A == 0 && PID_B == 0){
		producerProcess();
	}
	else if (PID_A > 0 && PID_B == 0){
		clientProcess();
	}
	else if (PID_A == 0 && PID_B > 0){
		//archiverProcess();
	}
	else if (PID_A < 0 || PID_B < 0){
		printf("Fork error!");
	}
}


int ** sobelFilter (int *** array, int height, int width, double * K, double * sobel_time){

	struct timespec start, stop;

	if ( clock_gettime (CLOCK_REALTIME, &start ) == -1 ){
		perror ("clock gettime");
		exit(EXIT_FAILURE);
	}

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

	if ( clock_gettime (CLOCK_REALTIME, &stop ) == -1 ){
		perror ("clock gettime");
		exit(EXIT_FAILURE);
	}

	*sobel_time = (stop.tv_sec - start.tv_sec ) + (double)(stop.tv_nsec - start.tv_nsec) / BILLION;

	return im_vertical;
}

int ** thresholdImage(int ** array, int height, int width, int TH){

	int ** im_binary = (int**) malloc (height*sizeof(int*) );
	for ( int i=0; i < height; i++){
		im_binary[i] = (int*) malloc (width*sizeof(int) );
	}
	if (im_binary != NULL) {
		for ( int i = 0; i < height; i++){
			for ( int j = 0; j < width; j++)
			{
				im_binary[i][j] = (array[i][j]>TH) ? 255.0 : 0;
			}
		}
	}
	return im_binary;
}

int *** readFile (char * filename, int * width, int * height, int * bpp, double * read_time)
{

	struct timespec start, stop;

	if ( clock_gettime (CLOCK_REALTIME, &start ) == -1 ){
		perror ("clock gettime");
		exit(EXIT_FAILURE);
	}
	*read_time = start.tv_sec  + (double)(start.tv_nsec) / BILLION;
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

	//*read_time = (stop.tv_sec - start.tv_sec ) + (double)(stop.tv_nsec - start.tv_nsec) / BILLION;

	return array;
}

int saveFile (char * save, int ** array, int height, int width, int bpp)
{
	system("ls results | wc -l > size.txt");
	FILE * size_file = fopen ("size.txt", "r");
	int size;
	fscanf (size_file, "%d", &size);
	fclose (size_file);

	char * temp;
	sprintf(temp, "%d", size);
	strcat(save, temp);

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

void saveTimer ( char * read_timers_name, char * sobel_timers_name, double read_time, double sobel_time)
{
	FILE * read_timers = fopen (read_timers_name, "a");
	if ( read_timers == NULL)
	{
		perror ("Error opening file.");
	}
	else
	{
		fprintf(read_timers, "%lf;\n", read_time);
		fclose (read_timers);
	}

	FILE * sobel_timers = fopen (sobel_timers_name, "a");

	if (sobel_timers == NULL)
	{
		perror ("Error opening file.");
	}
	else
	{
		fprintf(sobel_timers, "%lf\n", sobel_time);
		fclose (sobel_timers);
	}

}
