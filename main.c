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
#include <sys/mman.h>
#include <pthread.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <sys/mman.h>
#include <semaphore.h>


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define BUFFER_SIZE 300000
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
void* create_shared_memory(size_t size) {
  // Our memory buffer will be readable and writable:
  int protection = PROT_READ | PROT_WRITE;

  // The buffer will be shared (meaning other processes can access it), but
  // anonymous (meaning third-party processes cannot obtain an address for it),
  // so only this process and its children will be able to use it:
  int visibility = MAP_SHARED | MAP_ANONYMOUS;

  // The remaining parameters to `mmap()` are not important for this use case,
  // but the manpage for `mmap` explains their purpose.
  return mmap(NULL, size, protection, visibility, -1, 0);
}

int *** readFile (char * filename, int * width, int * height, int * bpp, double * read_time);
int proceed ( char * read_name, char * save_name, char * read_timers_name, char * sobel_timers_name);

int ** thresholdImage(int ** array, int height, int width, int TH);
int ** sobelFilter (int *** array, int height, int width, double * K, double * sobel_time);

int saveFile (char * save, int ** array, int height, int width, int bpp);
void saveTimer ( char * read_timers_name, char * sobel_timers_name, double read_time, double sobel_time);
void startProcesses();


void* shmem;
sem_t* mutex;
sem_t* shm_full;
sem_t* shm_empty;

int main( int ** argc, char ** argv) {


	shmem = create_shared_memory(BUFFER_SIZE);
	mutex = (sem_t *) create_shared_memory(sizeof(sem_t));
	shm_full = (sem_t *) create_shared_memory(sizeof(sem_t));
	shm_empty = (sem_t *) create_shared_memory(sizeof(sem_t));

	if( sem_init( mutex, 1, 1 ) != 0 )
		printf("sem_init: failed");
	if( sem_init( shm_full, 1, 1 ) != 0 )
		printf("sem_init: failed");
	if( sem_init( shm_empty, 1, 0 ) != 0 )
		printf("sem_init: failed");

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
	struct timespec start, stop;
	int height = 0, width = 0, bpp = 0;
	int i=0,ix=0,iy=0;

	int *** array = readFile("images/pic.png", &width, &height, &bpp, &read_time);
	char buffer[BUFFER_SIZE];
	char *b;
	for ( ix = 0; ix < height; ix++) {
		for ( iy = 0; iy < width; iy++){
			b = (char*)(array[ix][iy]);
			buffer[iy + ix*height] = *b;
		}
	}






	sem_wait( shm_full );
	printf("[PRODUCER]: shm_full down.\n");
	sem_wait( mutex );
	printf("[PRODUCER]: mutex down.\n");
	if ( clock_gettime (CLOCK_REALTIME, &start ) == -1 ){
		perror ("clock gettime");
		exit(EXIT_FAILURE);
	}
	memcpy(shmem, buffer, sizeof(buffer));
	read_time = start.tv_sec  + (double)(start.tv_nsec) / BILLION;
	saveTimer("timers/send_timer.txt", "sobel_timers.txt", read_time, sobel_time);
	printf("[PRODUCER]: Copied data to shared memory.\n");
	sem_post( shm_empty );
	printf("[PRODUCER]: shm_empty up.\n");
	sem_post( mutex );
	printf("[PRODUCER]: mutex up.\n");




}

void clientProcess(){
		double read_time, sobel_time;
		struct timespec start, stop;
		char buffer[BUFFER_SIZE];

		sem_wait( shm_empty );
		printf("[Client]: shm_empty down.\n");
		sem_wait( mutex );
		printf("[Client]: mutex down.\n");

		memcpy(buffer, shmem, sizeof(buffer));
		if ( clock_gettime (CLOCK_REALTIME, &start ) == -1 ){
			perror ("clock gettime");
			exit(EXIT_FAILURE);
		}
		read_time = start.tv_sec  + (double)(start.tv_nsec) / BILLION;
		saveTimer("timers/receive_timer.txt", "sobel_timers.txt", read_time, sobel_time);
		printf("[CLIENT]: Read data from shared memory.\n");


		sem_post( mutex );
		printf("[Client]: mutex up.\n");
		sem_post( shm_full );
		printf("[Client]: shm_full up.\n");

/*
		for (int ix = 0; ix < 480; ix++) {
			for (int iy = 0; iy < 640; iy++){
				printf("%d, ", (int*)buffer[iy + ix*480]);
			}
		}
*/
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
