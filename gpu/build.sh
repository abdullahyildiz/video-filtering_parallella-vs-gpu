#!/bin/bash

INCDIR=/usr/local/include
LIBDIR=/usr/local/lib

nvcc kinect-OpenCV.cu -o kinect-OpenCV `pkg-config --cflags --libs opencv` -I${INCDIR} -L${LIBDIR} -lpthread -lfreenect
