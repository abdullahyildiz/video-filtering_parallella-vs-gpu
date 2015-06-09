#!/bin/bash

ESDK=${EPIPHANY_HOME}
ELIBS=${ESDK}/tools/host/lib
EINCS=${ESDK}/tools/host/include
INCDIR=/usr/local/include
LIBDIR=/usr/local/lib


gcc kinect-OpenCV.c `pkg-config --cflags --libs opencv` -o kinect-OpenCV.elf -I${INCDIR} -I${EINCS} -L${LIBDIR} -L${ELIBS} -le-hal -lpthread -lfreenect

e-gcc -T fast_modified.ldf core.c -o core.elf -le-lib

e-objcopy --srec-forceS3 --output-target srec core.elf core.srec
