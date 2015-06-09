/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * Andrew Miller <amiller@dappervision.com>
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include "libfreenect/libfreenect.h"
#include <pthread.h>
#include <opencv2/highgui/highgui.hpp>
#include <e-hal.h>

#include "structures.h"

unsigned int IMAGE_ADDRESS = 0x2400;
unsigned int MESSAGE_ADDRESS = 0x7A00;

typedef struct{
 	e_platform_t platform;
	e_epiphany_t epiphany;
} epiphany_structure;

pthread_t freenect_thread;
int terminate = 0;
uint8_t *rgb_display;
uint8_t *gray_display;
uint8_t *tmp,*tmp2;
freenect_context *f_ctx;
freenect_device *f_dev;
uint8_t* window;
volatile message msg;

void sort(uint8_t* window){ // Sort Function
	uint8_t temp;
	int j,i;
	for(i=1;i<9;i++){
		for(j=0;j<9-i;j++){
			if(window[j] > window[j+1]){
				temp=window[j];
				window[j]=window[j+1];
				window[j+1]=temp;
			}
		}
	}
}

_Bool medianFilter(uint8_t* frame){ // Host Median Filter
	int row,col,layer,wrow,wcol,s;
	for(row=1;row<HEIGHT;row++){
		for(col=1;col<WIDTH;col++){
			//Fill Window
			for(wrow=-1;wrow<2;wrow++){
				for(wcol=-1;wcol<2;wcol++){
					window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+wcol];
				}
			}
			//Sort Window
			sort(window);
			s=1;
			if (window[4]==(uint8_t) 0){
				while(window[4+s]==(uint8_t) 0 && s!=5){
					s=s+1;
				}
				window[4]=window[4+s+(10-(5+s))/2];
			}
			if (window[4]==(uint8_t) 255){
				while(window[4-s]==(uint8_t) 255 && s!=5){
					s=s+1;
				}
				window[4]=window[(4-s)/2];
			}
			tmp2[row*WIDTH+col]=window[4];
		}
	}
	return 1;
}

_Bool edgeDetection(uint8_t *frame){ // Host Edge Detection
	int row,col,element,value,wrow,wcol;
	int window[9] = {-1,-1,-1,-1,8,-1,-1,-1,-1};
	for(row=1;row<HEIGHT;row++){
		for(col=1;col<WIDTH;col++){
			value=0;
			for(wrow=-1;wrow<2;wrow++){
				for(wcol=-1;wcol<2;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
				}
			}
			gray_display[row*WIDTH+col]=(uint8_t) (value/9<0 ? 0 : 255);
		}
	}
}

_Bool gray(uint8_t *frame){ // Grayscale Conversion Function
	int row,col,layer;
	for(row=0;row<HEIGHT;row++){
		for(col=0;col<WIDTH;col++){
			for(layer=0;layer<3;layer++)
				window[layer]=frame[row*(WIDTH*3)+col*3+layer];
			tmp[row*WIDTH+col]=(window[0]+window[1]+window[2])/3;
		}
	}
	return 1;
}

/*
*	Epiphany Section
*/

void waitCoreReady(epiphany_structure *e){ // Function to wait READY signal
	int row,col;
    	for (row=0; row<(int) e->platform.rows; row++)
        	for (col=0; col<(int) e->platform.cols;){
			e_read(&e->epiphany, row, col, MESSAGE_ADDRESS + offsetof(message, core_ready), (void *) &(msg.core_ready), sizeof(msg.core_ready));
            		if(msg.core_ready==1){
				msg.core_ready=0;
				col++;
			}
		}
        printf("!!EPIPHANY SYSTEM IS READY!!\n");
        fflush(stdout);
}

void waitCoreComplete(epiphany_structure *e){ // Function to wait COMPLETE signal
	int row,col;
	for (row=0; row<(int) e->platform.rows; row++)
        	for (col=0; col<(int) e->platform.cols;){
			e_read(&e->epiphany, row, col, MESSAGE_ADDRESS + offsetof(message, core_complete),(void *) &(msg.core_complete) , sizeof(msg.core_complete));
        		if(msg.core_complete==1){
        			msg.core_complete=0;
				/* Reset COMPLETE signal in cores local memory */
               			e_write(&e->epiphany, row, col, MESSAGE_ADDRESS + offsetof(message, core_complete), (void *) &(msg.core_complete), sizeof(msg.core_complete));
				col++;
			}
		}
}

int init_epiphany(epiphany_structure *e){
	int i,row,col;
  	e_init(NULL);
  	e_reset_system();
	e_get_platform_info(&e->platform);
	e_open(&e->epiphany, 0, 0, e->platform.rows, e->platform.cols);
	e_reset_group(&e->epiphany);
	msg.signal_terminate=0;
	msg.signal_go=0;
  	msg.core_ready=0;
	msg.core_complete=0;
	for (row=0; row<(int) e->platform.rows; row++)
		for (col=0; col<(int) e->platform.cols; col++)
			e_write(&e->epiphany, row, col, MESSAGE_ADDRESS, (void *)(&msg), sizeof(msg));
	if (e_load_group("core.srec", &e->epiphany, 0, 0, e->platform.rows, e->platform.cols, E_TRUE) == E_ERR){
    		perror("Epiphany Load Failed");
    		return -1;
  	}
  	return 0;
}

int close_epiphany(epiphany_structure *e){
	e_close(&e->epiphany);
	e_finalize();
}

/*
*	Kinect Section
*/

void rgb_cb(freenect_device *dev, void *rgb, uint32_t timestamp){
	int row,col;
	rgb_display = (uint8_t*)rgb;
}

void *freenect_threadfunc(void *arg){
	int accelCount = 0;
	freenect_set_tilt_degs(f_dev,0);
	freenect_set_led(f_dev,LED_RED);
	freenect_set_video_callback(f_dev, rgb_cb);
	freenect_set_video_mode(f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
	freenect_start_video(f_dev);
	while (!terminate && (freenect_process_events(f_ctx) >= 0)){}
	printf("\nshutting down streams...\n");
	freenect_stop_video(f_dev);
	freenect_close_device(f_dev);
	freenect_shutdown(f_ctx);
	printf("-- done!\n");
	return NULL;
}

int main(int argc, char **argv){
	int res,row,col,cnum,count=0,cornercores=HEIGHT/16+1,middlecores=HEIGHT/16+2,framesize=HEIGHT/16;
	unsigned int addr;
	uint8_t host_signal=1;
	time_t start,end;
	double seconds,fps;
	epiphany_structure epiphany;
	IplImage* grayscale;
	rgb_display = (uint8_t*)malloc(WIDTH*HEIGHT*3);
	gray_display= (uint8_t*)malloc(WIDTH*HEIGHT);
	tmp= (uint8_t*)malloc(WIDTH*HEIGHT);
	tmp2= (uint8_t*)malloc(WIDTH*HEIGHT);
	window=(uint8_t*)malloc(9);
	printf("Kinect camera test\n");
	if (freenect_init(&f_ctx, NULL) < 0) {
		printf("freenect_init() failed\n");
		return 1;
	}
	freenect_select_subdevices(f_ctx, (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));
	int nr_devices = freenect_num_devices (f_ctx);
	printf ("Number of devices found: %d\n", nr_devices);
	int user_device_number = 0;
	if (argc > 1)
		user_device_number = atoi(argv[1]);
	if (nr_devices < 1) {
		freenect_shutdown(f_ctx);
		return 1;
	}
	if (freenect_open_device(f_ctx, &f_dev, user_device_number) < 0) {
		printf("Could not open device\n");
		freenect_shutdown(f_ctx);
		return 1;
	}
	res = pthread_create(&freenect_thread, NULL, freenect_threadfunc, NULL);
	if (res) {
		printf("pthread_create failed\n");
		freenect_shutdown(f_ctx);
		return 1;
	}
	if(init_epiphany(&epiphany)){
		printf("init_epiphany failed");
		return 1;
	}
	waitCoreReady(&epiphany);// Wait core_ready

	cvNamedWindow( "Output" , CV_WINDOW_AUTOSIZE);
	grayscale = cvCreateImageHeader(cvSize(WIDTH,HEIGHT), IPL_DEPTH_8U, 1);
	addr = IMAGE_ADDRESS;
	time(&start);
	gray(rgb_display);
	while(1){
		/* Host */
		// gray(rgb_display);
		// medianFilter(tmp);
		// edgeDetection(tmp2);


		/* Epiphany */
		// Copy Image to cores local memory
		for (row=0; row<(int) epiphany.platform.rows; row++)
			for (col=0; col<(int) epiphany.platform.cols; col++){
				cnum = e_get_num_from_coords(&epiphany.epiphany, row, col);
				if(row==0 && col==0){
					e_write(&epiphany.epiphany, row, col, addr, (void *) &tmp[0], WIDTH*cornercores);
				}
				else if(row==((int) epiphany.platform.rows)-1 && col==((int) epiphany.platform.cols)-1){
					e_write(&epiphany.epiphany, row, col, addr, (void *) &tmp[WIDTH * (HEIGHT - cornercores)], WIDTH*cornercores);
				}
				else{
					e_write(&epiphany.epiphany, row, col, addr, (void *) &tmp[WIDTH * (cnum * framesize) - WIDTH], WIDTH*middlecores);
				}
			}

		msg.signal_go=1;
		// Signal GO to each core
		for (row=0; row<(int) epiphany.platform.rows; row++)
	        	for (col=0; col<(int) epiphany.platform.cols; col++)
				e_write(&epiphany.epiphany, row, col, MESSAGE_ADDRESS + offsetof(message, signal_go), (void *)&msg.signal_go, sizeof(msg.signal_go));
		gray(rgb_display);

		waitCoreComplete(&epiphany);// Wait core_complete

		//Read Image from Cores Memory
		for (row=0; row<(int) epiphany.platform.rows; row++)
			for (col=0; col<(int) epiphany.platform.cols; col++){
				cnum = e_get_num_from_coords(&epiphany.epiphany, row, col);
				e_read(&epiphany.epiphany, row, col, addr, (void *) &gray_display[WIDTH * (cnum*framesize)], WIDTH*framesize);
			}

		cvSetData(grayscale,gray_display,WIDTH);
		cvShowImage("Output", grayscale);
		++count;
		char c = cvWaitKey(10);
	    if (c == 27){
		time(&end);
		msg.signal_go=1;
		msg.signal_terminate=1;
		// Signal terminate
		for (row=0; row<(int) epiphany.platform.rows; row++)
	        	for (col=0; col<(int) epiphany.platform.cols; col++)
				e_write(&epiphany.epiphany, row, col, MESSAGE_ADDRESS, (void *)&msg, sizeof(msg));
		terminate = 1; // Stop Kinect Device
		break;
	    }
	}
        
    seconds=difftime(end, start);
    fps=count/seconds;
    printf("FPS = %.2f\n",fps);
	cvReleaseImage(&grayscale);
	cvDestroyWindow("Output");
	close_epiphany(&epiphany);
	return 0;
}