#!/bin/bash

# Use Holy Build Box to run:
#     docker run -t -i --rm -v `pwd`:/io ghcr.io/phusion/holy-build-box/hbb-64 bash /io/cmake/hbb.sh

set -e

source /hbb_exe/activate

set -x

yum -y install python3 libX11-devel libXi-devel libXcursor-devel pulseaudio-libs-devel alsa-lib-devel mesa-libGL-devel

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DARDENS_LLVM=1 -DARDENS_DEBUGGER=1 -DARDENS_PLAYER=1 -DARDENS_LIBRETRO=0 /io

nj=$(nproc)
if [[ $nj > 4 ]]; then
	nj=4
fi

make -j$nj

strip -x Ardens ArdensPlayer

cp Ardens ArdensPlayer /io/build

