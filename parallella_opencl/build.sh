#!/bin/sh

set -e

g++ hello_opencl.c `pkg-config --cflags --libs opencv` -o hello_opencl.elf -I/usr/local/browndeer/include -L/usr/local/browndeer/lib -I/usr/local/include -L/usr/local/lib -lcoprthr_opencl -lpthread -lfreenect -lm

