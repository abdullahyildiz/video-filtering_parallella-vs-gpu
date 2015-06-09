/*
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <time.h>
#include "libfreenect/libfreenect.h"
#include <pthread.h>
#include <opencv2/highgui/highgui.hpp>
#include <cuda_runtime.h>
#include "structures.h"

pthread_t freenect_thread;
int terminate = 0;
uint8_t *rgb_display;
uint8_t *gray_display;
uint8_t *tmp,*tmp2;
freenect_context *f_ctx;
freenect_device *f_dev;
uint8_t* window;

/*
*	Grayscale conversion
*/

void gray(uint8_t *frame){ //Grayscale Conversion Function
	int row,col,layer;
	for(row=0;row<HEIGHT;row++){
		for(col=0;col<WIDTH;col++){
			for(layer=0;layer<3;layer++)
				window[layer]=frame[row*(WIDTH*3)+col*3+layer];
			tmp[row*WIDTH+col]=(window[0]+window[1]+window[2])/3;
		}
	}
}

/*
*	CUDA Section
*/

__device__ void sort(uint8_t* window){ // Sort Function for Device
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

__device__ void medianFilter(uint8_t *frame){ //Median Filter
	int blocknum = blockIdx.x;
	int framestart = blocknum*(SIZE/CORENUM);
	int rowcount=(HEIGHT/CORENUM-1);
	int row,col,wrow,wcol,s,flag=0,received_row;
	uint8_t temp,tmp[WIDTH];
	uint8_t window[9];
	/* Get number of rows received according to block number */
	if(blocknum==CORENUM-1 || blocknum==0){
		received_row=HEIGHT/CORENUM+1;
	}
	else{
		received_row=HEIGHT/CORENUM+2;
	}
	if(blocknum==0){
		for(row=0;row<received_row-1;row++){
			for(col=0;col<WIDTH;col++){
				//Fill Window
				for(wrow=-1;wrow<2;wrow++){
					if(wrow==-1 && row==0){
						wrow=0;
						flag=1;
					}
					for(wcol=-1;wcol<2;wcol++){
						if(col==0){
							if(wcol==-1){
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+(wcol+1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else if(col==WIDTH-1){
							if(wcol==2){
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+(wcol-1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else{
							window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
						}
					}
					if(flag){
						flag=0;
						wrow=-1;
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
				if(row==0){
					tmp[col]=window[4];
				}
				else{// Fix to not use changed bit in next iteration
					if(col==0){
						temp=tmp[col];
						tmp[col]=window[4];
					}
					else if(col==WIDTH-1){
						frame[framestart+(row-1)*WIDTH+col-1]=temp;
						frame[framestart+(row-1)*WIDTH+col]=tmp[col];
						tmp[col]=window[4];
					}
					else{
						frame[framestart+(row-1)*WIDTH+col-1]=temp;
						temp=tmp[col];
						tmp[col]=window[4];
					}
				}
			}
		}
	}
	else if(blocknum==CORENUM-1){
		for(row=1;row<received_row;row++){
			for(col=0;col<WIDTH;col++){
				//Fill Window
				for(wrow=-1;wrow<2;wrow++){
					if(wrow==1 && row==received_row-1){
						wrow=0;
						flag=1;
					}
					for(wcol=-1;wcol<2;wcol++){
						if(col==0){
							if(wcol==-1){
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+(wcol+1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else if(col==WIDTH-1){
							if(wcol==2){
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+(wcol-1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else{
							window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
						}
					}
					if(flag){
						flag=0;
						break;
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
				if(row==1){
					tmp[col]=window[4];
				}
				else{
					frame[framestart+(row-2)*WIDTH+col]=tmp[col];
					tmp[col]=window[4];
				}
			}
		}
	}
	else{
		for(row=1;row<received_row-1;row++){
			for(col=0;col<WIDTH;col++){
				//Fill Window
				for(wrow=-1;wrow<2;wrow++){
					for(wcol=-1;wcol<2;wcol++){
						if(col==0){
							if(wcol==-1){
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+(wcol+1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else if(col==WIDTH-1){
							if(wcol==2){
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+(wcol-1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else{
							window[(wrow+1)*3+(wcol+1)]=frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
						}
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
				if(row==1){
					tmp[col]=window[4];
				}
				else{
					frame[framestart+(row-2)*WIDTH+col]=tmp[col];
					tmp[col]=window[4];
				}
			}
		}
	}
	for (col=0;col<WIDTH;col++){ // Final row written from tmp to frame
		frame[framestart+WIDTH*rowcount+col]=tmp[col];
	}
}

__device__ void edgeDetection(uint8_t *frame){ // Laplacian Filter as an Edge Detector
	int blocknum = blockIdx.x;
	int rowcount=(HEIGHT/CORENUM-1);
	int framestart = blocknum*(SIZE/CORENUM);
	int row=0,col,value,wrow,wcol;
	int window[9] = {-1,-1,-1,-1,8,-1,-1,-1,-1}; // Laplacian Kernel
	uint8_t temp,tmp[WIDTH];
	for(col=0;col<WIDTH;col++){ //row 0
		if(col==0){
			value=0;
			for(wrow=0;wrow<2;wrow++){
				for(wcol=0;wcol<2;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
				}
			}
			tmp[col]=(uint8_t) (value/4<0 ? 0 : 255);
		}
		else if(col==WIDTH-1){
			value=0;
			for(wrow=0;wrow<2;wrow++){
				for(wcol=-1;wcol<1;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
				}
			}
			tmp[col]=(uint8_t) (value/4<0 ? 0 : 255);
		}
		else{
			value=0;
			for(wrow=0;wrow<2;wrow++){
				for(wcol=-1;wcol<2;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
					if(wrow==0){
						value+=window[(wrow)*3+(wcol+1)]*(int)frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
					}
				}
			}
			tmp[col]=(uint8_t) (value/9<0 ? 0 : 255);
		}
	}
	for(row=1;row<rowcount;row++){
		for(col=0;col<WIDTH;col++){
			if(col==0){
				value=0;
				for(wrow=-1;wrow<2;wrow++){
					for(wcol=0;wcol<2;wcol++){
						value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
					}
				}
				temp=tmp[col];
				tmp[col]=(uint8_t) (value/6<0 ? 0 : 255);
			}
			else if(col==WIDTH-1){
				value=0;
				for(wrow=-1;wrow<2;wrow++){
					for(wcol=-1;wcol<1;wcol++){
						value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
					}
				}
				frame[framestart+(row-1)*WIDTH+col-1]=temp;
				frame[framestart+(row-1)*WIDTH+col]=tmp[col];
				tmp[col]=(uint8_t) (value/6<0 ? 0 : 255);
			}
			else{
				value=0;
				for(wrow=-1;wrow<2;wrow++){
					for(wcol=-1;wcol<2;wcol++){
						value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
					}
				}
				frame[framestart+(row-1)*WIDTH+col-1]=temp;
				temp=tmp[col];
				tmp[col]=(uint8_t) (value/9<0 ? 0 : 255);
			}
		}
	}
	row=rowcount;
	for(col=0;col<WIDTH;col++){ //row equals rowcount
		if(col==0){
			value=0;
			for(wrow=-1;wrow<2;wrow++){
				for(wcol=0;wcol<2;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
				}
			}
			temp=tmp[col];
			tmp[col]=(uint8_t) (value/4<0 ? 0 : 255);
		}
		else if(col==WIDTH-1){
			value=0;
			for(wrow=-1;wrow<2;wrow++){
				for(wcol=-1;wcol<1;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
				}
			}
			frame[framestart+(row-1)*WIDTH+col-1]=temp;
			frame[framestart+(row-1)*WIDTH+col]=tmp[col];
			tmp[col]=(uint8_t) (value/4<0 ? 0 : 255);
		}
		else{
			value=0;
			for(wrow=-1;wrow<1;wrow++){
				for(wcol=-1;wcol<2;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
					if(wrow==0){
						value+=window[(wrow+2)*3+(wcol+1)]*(int)frame[framestart+row*WIDTH+col+wrow*WIDTH+wcol];
					}
				}
			}
			frame[framestart+(row-1)*WIDTH+col-1]=temp;
			temp=tmp[col];
			tmp[col]=(uint8_t) (value/9<0 ? 0 : 255);
		}
	}
	for(col=0;col<WIDTH;col++){ // Final row written from tmp to frame
		frame[framestart+rowcount*WIDTH+col]=tmp[col];
	}
}

__global__ void filters(uint8_t *frame){ // Function called from host to start filtering
	medianFilter(frame);
	edgeDetection(frame);
}

void* filterThread(void* d_frame){ // Activate devices
	int blocksPerGrid =CORENUM;
	uint8_t* frame=(uint8_t*)d_frame;
	filters<<<blocksPerGrid,1>>>(frame); 
	return 0;
}

/*
*	Kinect Section
*/

void rgb_cb(freenect_device *dev, void *rgb, uint32_t timestamp){
	rgb_display = (uint8_t*)rgb;
}

void *freenect_threadfunc(void *arg){
	freenect_set_tilt_degs(f_dev,0);
	freenect_set_led(f_dev,LED_RED);
	freenect_set_video_callback(f_dev, rgb_cb);
	freenect_set_video_mode(f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
	freenect_start_video(f_dev);
	while (!terminate && (freenect_process_events(f_ctx) >= 0)){}
	printf("\nshutting down streams...\n");
	//freenect_stop_depth(f_dev);
	freenect_stop_video(f_dev);
	freenect_close_device(f_dev);
	freenect_shutdown(f_ctx);
	printf("-- done!\n");
	return NULL;
}

/*
* Host main
*/

int main(int argc, char **argv){
	pthread_t thread;
    int res,count=0;
    cudaError_t err = cudaSuccess;
	IplImage* grayscale;
    size_t size = SIZE * sizeof(uint8_t);
	rgb_display = (uint8_t*)malloc(size*3);
	gray_display= (uint8_t*)malloc(size);
	tmp= (uint8_t*)malloc(size);
	tmp2= (uint8_t*)malloc(size);
	window=(uint8_t*)malloc(9);
	time_t start,end;
	double seconds,fps;

    uint8_t *d_frame = NULL;
    err = cudaMalloc((void **)&d_frame, size);
	
	/* Kinect in main */
		
	printf("Kinect camera test\n");
	if (freenect_init(&f_ctx, NULL) < 0) {
		printf("freenect_init() failed\n");
		return 1;
	}
	//freenect_set_log_level(f_ctx, FREENECT_LOG_DEBUG);
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

	/* OpenCV in main */

	cvNamedWindow( "CUDA Example" , CV_WINDOW_AUTOSIZE);
	grayscale = cvCreateImageHeader(cvSize(WIDTH,HEIGHT), IPL_DEPTH_8U, 1);
	time(&start);
	gray(rgb_display);

	/* Start */

	while(1){
		/* HOST */
		//gray(rgb_display);
		//medianFilter(tmp);
		//edgeDetection(tmp2);


		err = cudaMemcpy(d_frame, tmp, size, cudaMemcpyHostToDevice);
		if (err != cudaSuccess){
		    fprintf(stderr, "Failed to copy input frame from host to device (error code %s)!\n", cudaGetErrorString(err));
		    exit(EXIT_FAILURE);
		}

		pthread_create( &thread, NULL, filterThread, (void*) d_frame);
		gray(rgb_display);
		pthread_join( thread, NULL);


		err = cudaGetLastError();
		if (err != cudaSuccess){
		    fprintf(stderr, "Failed to launch filtering kernel (error code %s)!\n", cudaGetErrorString(err));
		    exit(EXIT_FAILURE);
		}

		err = cudaMemcpy(gray_display, d_frame, size, cudaMemcpyDeviceToHost);
		if (err != cudaSuccess){
		    fprintf(stderr, "Failed to copy output frame from device to host (error code %s)!\n", cudaGetErrorString(err));
		    exit(EXIT_FAILURE);
		}
		cvSetData(grayscale,gray_display,WIDTH);
		cvShowImage("CUDA Example", grayscale);
		++count;
		char c = cvWaitKey(33);
	    if (c == 27){
			time(&end);
			break;
	    }
	}

        seconds=difftime(end, start);
        fps=count/seconds;
        printf("FPS = %.2f\n",fps);
    // Free device global memory
    err = cudaFree(d_frame);

    // Free host memory
	free(rgb_display);
	free(gray_display);
	free(tmp);
	free(tmp2);
	free(window);
    err = cudaDeviceReset();
	
	// Free kinect
	terminate=1;

	//Free OpenCV window
	cvReleaseImage(&grayscale);
	cvDestroyWindow("CUDA Example");
    return 0;
}
