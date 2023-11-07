#!/bin/bash

cd "$(dirname "$0")"

rm -rf build
mkdir -p build
cmake -B build .. -DARDENS_LLVM=0 -DARDENS_DEBUGGER=0 -DARDENS_PLAYER=0 -DARDENS_DIST=1 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j $(sysctl -n hw.physicalcpu)
cp -a build/*.app .

