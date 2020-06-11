#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <mqueue.h>
#include <string.h>
#include <fcntl.h>    /* For O_* constants. */
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define QUEUE_NAME  "/test_queue" /* Queue name. */
#define QUEUE_PERMS ((int)(0644))
#define QUEUE_MAXMSG  2 /* Maximum number of messages. */
#define QUEUE_MSGSIZE 300000 /* Length of message. */
#define QUEUE_ATTR_INITIALIZER ((struct mq_attr){0, QUEUE_MAXMSG, QUEUE_MSGSIZE, 0, {0}})

/* The consumer is faster than the publisher. */
#define QUEUE_POLL_CONSUMER ((struct timespec){2, 500000000})
#define QUEUE_POLL_PUBLISHER ((struct timespec){5, 0})

#define QUEUE_MAX_PRIO ((int)(9))

static bool th_consumer_running = true;
static bool th_publisher_running = true;


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
	double read_time, sobel_time;
	int height = 0, width = 0, bpp = 0;
	int i=0,ix=0,iy=0;

	int *** array = readFile("images/pic.png", &width, &height, &bpp, &read_time);
	char buffer[QUEUE_MSGSIZE];
	char *b;
	printf("%d %d\n",height, width );
	for ( ix = 0; ix < height; ix++) {
		for ( iy = 0; iy < width; iy++){
			b = (char*)(array[ix][iy]);
			buffer[iy + ix*height] = *b;
		}
	}


	mqd_t mq;
	struct timespec poll_sleep;
	struct timespec start, stop;
	do {
		mq = mq_open(QUEUE_NAME, O_WRONLY);
		if(mq < 0) {
			printf("[PUBLISHER]: The queue is not created yet. Waiting...\n");

			poll_sleep = QUEUE_POLL_PUBLISHER;
			nanosleep(&poll_sleep, NULL);
		}
	} while(mq == -1);

	printf("[PUBLISHER]: Queue opened, queue descriptor: %d.\n", mq);

	/* Intializes random number generator. */
	srand((unsigned)time(NULL));

	unsigned int prio = 0;
	int count = 1;

	while(th_publisher_running) {
		/* Send a burst of three messages */
		prio = QUEUE_MAX_PRIO;
		//snprintf(buffer, sizeof(buffer), "MESSAGE NUMBER %d, PRIORITY %d", count, prio);


		printf("[PUBLISHER]: Sending message %d with priority %d...\n", count, prio);
		if ( clock_gettime (CLOCK_REALTIME, &start ) == -1 ){
			perror ("clock gettime");
			exit(EXIT_FAILURE);
		}
		mq_send(mq, buffer, QUEUE_MSGSIZE, prio);
		read_time = start.tv_sec  + (double)(start.tv_nsec) / BILLION;
		saveTimer("timers/send_timer.txt", "sobel_timers.txt", read_time, sobel_time);
		count++;

		poll_sleep = QUEUE_POLL_PUBLISHER;
		nanosleep(&poll_sleep, NULL);

		fflush(stdout);
		break;
	}

	/* Cleanup */
	printf("[PUBLISHER]: Cleanup...\n");
	mq_close(mq);


}



/*
	while ( i < 1){
			//saveTimer("timers/read_timer.txt", "sobel_timers.txt", read_time, sobel_time);

		i++;
	}
	*/
	/* cleanup */
void clientProcess(){
		double read_time, sobel_time;
		struct timespec start, stop;
		struct mq_attr attr = QUEUE_ATTR_INITIALIZER;

		/* Create the message queue. The queue reader is NONBLOCK. */
		mqd_t mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY | O_NONBLOCK, QUEUE_PERMS, &attr);
		if(mq < 0) {
			fprintf(stderr, "[CONSUMER]: Error, cannot open the queue: %s.\n", strerror(errno));
			exit(1);
		}

		printf("[CONSUMER]: Queue opened, queue descriptor: %d.\n", mq);

		unsigned int prio;
		ssize_t bytes_read= 0;
		char buffer[QUEUE_MSGSIZE + 1];
		struct timespec poll_sleep;
		while(th_consumer_running) {
			memset(buffer, 0x00, sizeof(buffer));
			bytes_read = mq_receive(mq, buffer, QUEUE_MSGSIZE, &prio);
			if(bytes_read >= 0) {
				if ( clock_gettime (CLOCK_REALTIME, &start ) == -1 ){
					perror ("clock gettime");
					exit(EXIT_FAILURE);
				}
				read_time = start.tv_sec  + (double)(start.tv_nsec) / BILLION;
				saveTimer("timers/receive_timer.txt", "sobel_timers.txt", read_time, sobel_time);
				printf("[CONSUMER]: Received message: \"%ld\"\n", sizeof(buffer));
				break;
			} else {
				printf("[CONSUMER]: No messages yet.\n");
				poll_sleep = QUEUE_POLL_CONSUMER;
				nanosleep(&poll_sleep, NULL);
			}

			fflush(stdout);
		}

		/* Cleanup */
		printf("[CONSUMER]: Cleanup...\n");
		mq_close(mq);
		mq_unlink(QUEUE_NAME);



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
