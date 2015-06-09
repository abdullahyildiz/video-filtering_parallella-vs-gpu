#!/bin/bash

set -e

ELIBS=${EPIPHANY_HOME}/tools/host/lib
EHDF=${EPIPHANY_HDF}

sudo -E LD_LIBRARY_PATH=${ELIBS} EPIPHANY_HDF=${EHDF} ./kinect-OpenCV.elf
