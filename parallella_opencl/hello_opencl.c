#define DEVICE_TYPE	CL_DEVICE_TYPE_ACCELERATOR

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <CL/cl.h>

#include <opencv2/highgui/highgui.hpp>

#define WIDTH 320
#define HEIGHT 240
#define SIZE WIDTH*HEIGHT

using namespace cv;
using namespace std;

int main(int argc, char **argv)
{ 

  /******************OpenCL*******************/

  int i,j;
  int err;
  char buffer[256];

  unsigned int n = 16;

  cl_uint nplatforms;
  cl_platform_id* platforms;
  cl_platform_id platform;

  clGetPlatformIDs(0,0,&nplatforms);
  platforms = (cl_platform_id*)malloc(nplatforms*sizeof(cl_platform_id));
  clGetPlatformIDs(nplatforms, platforms, 0);

  for(i=0; i<nplatforms; i++) {
    platform = platforms[i];
    clGetPlatformInfo(platforms[i],CL_PLATFORM_NAME,256,buffer,0);
    if (!strcmp(buffer,"coprthr")) break;
  }

  if (i<nplatforms) platform = platforms[i];
  else exit(1);

  cl_uint ndevices;
  cl_device_id* devices;
  cl_device_id dev;

  clGetDeviceIDs(platform,DEVICE_TYPE,0,0,&ndevices);
  devices = (cl_device_id*)malloc(ndevices*sizeof(cl_device_id));
  clGetDeviceIDs(platform, DEVICE_TYPE,ndevices,devices,0);

  if (ndevices) dev = devices[0];
  else exit(1);

  cl_context_properties ctxprop[3] = {
    (cl_context_properties)CL_CONTEXT_PLATFORM,
    (cl_context_properties)platform,
    (cl_context_properties)0
  };
  
  cl_context ctx = clCreateContext(ctxprop,1,&dev,0,0,&err);
  cl_command_queue cmdq = clCreateCommandQueue(ctx,dev,0,&err);

  size_t tmp_display_sz  = SIZE*sizeof(uint8_t);
  size_t gray_display_sz = SIZE*sizeof(uint8_t);
   
  uint8_t* tmp_display  = (uint8_t*)malloc(SIZE);
  uint8_t* gray_display = (uint8_t*)malloc(SIZE);
  
  Mat image;
  image = imread("Desert.jpg", CV_LOAD_IMAGE_GRAYSCALE);   // Read the file

  uchar* p;

  for(i=0;i<image.rows;++i){
    p = image.ptr<uchar>(i);
    for (j = 0; j < image.cols; ++j) {
      tmp_display[i * image.cols + j] = p[j];
      gray_display[i * image.cols + j] = 0;
    }
  }

  cl_mem tmp_display_buf = clCreateBuffer(ctx,CL_MEM_USE_HOST_PTR,tmp_display_sz,tmp_display,&err);
  cl_mem gray_display_buf = clCreateBuffer(ctx,CL_MEM_USE_HOST_PTR,gray_display_sz,gray_display,&err);
   
  const char kernel_code[]=
    "__kernel void dataParallel(\n"
    "__global unsigned char* tmp_display,__global unsigned char* gray_display)\n"
    "{\n"
    "   int i;\n"
    "for(i=0; i<320*240; i++){\n"
    "gray_display[i] = tmp_display[i];\n"
    "}\n"
    "}\n";

  const char* src[1] = { kernel_code };
  size_t src_sz = sizeof(kernel_code);

  cl_program prg = clCreateProgramWithSource(ctx,1,(const char**)&src,&src_sz,&err);

  printf("Build has started.\n");
  fflush(stdout);
   
  clBuildProgram(prg,1,&dev,0,0,0);
  printf("Build has finished.\n");
  fflush(stdout);   
  cl_kernel krn = clCreateKernel(prg,"dataParallel",&err);
  
  clSetKernelArg(krn,0,sizeof(cl_mem),&tmp_display_buf);
  clSetKernelArg(krn,1,sizeof(cl_mem),&gray_display_buf); 

  size_t gtdsz[] = { 16 };
  size_t ltdsz[] = { 16 };

  cl_event ev[10];
 
  clEnqueueNDRangeKernel(cmdq,krn,1,0,gtdsz,ltdsz,0,0,&ev[0]);
  clEnqueueReadBuffer(cmdq,gray_display_buf,CL_TRUE,0,gray_display_sz,gray_display,0,0,&ev[1]);
  err = clWaitForEvents(2,ev);
   
  /******************OpenCL*******************/

  /******************OpenCV*******************/

  IplImage* grayscale;

  cvNamedWindow( "Output" , CV_WINDOW_AUTOSIZE);
  grayscale = cvCreateImageHeader(cvSize(WIDTH,HEIGHT), IPL_DEPTH_8U, 1);
  
  cvSetData(grayscale,gray_display,WIDTH);
  cvShowImage("Output", grayscale);
  waitKey(0);

  cvReleaseImage(&grayscale);
  cvDestroyWindow("Output");

  /******************OpenCV*******************/

  clReleaseEvent(ev[1]);
  clReleaseEvent(ev[0]);
  clReleaseKernel(krn);
  clReleaseProgram(prg);
  clReleaseMemObject(tmp_display_buf);
  clReleaseMemObject(gray_display_buf);
  clReleaseCommandQueue(cmdq);
  clReleaseContext(ctx);

  free(tmp_display);
  free(gray_display);

  return 0;

}