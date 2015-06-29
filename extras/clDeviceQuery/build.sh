#!/bin/sh

set -e

g++ -o clDeviceQuery clDeviceQuery.cpp -I/usr/local/browndeer/include -L/usr/local/browndeer/lib -lcoprthr_opencl
