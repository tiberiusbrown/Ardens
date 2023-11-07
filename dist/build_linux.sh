#!/bin/bash

cd "$(dirname "$0")"

rm -rf build
mkdir build

docker run --rm -v `pwd`/..:/io ghcr.io/phusion/holy-build-box/hbb-64 bash /io/cmake/hbb_dist.sh

cd build

for f in $(ls *); do
	[[ -x "$f" ]] && file="$f"
done

wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

mkdir AppDir
mv -f "$file" AppDir
cp ../../src/ArdensPlayer.desktop AppDir/${file}.desktop
sed -i "s/ArdensPlayer/${file}/" AppDir/${file}.desktop
cp ../../img/ardens.png AppDir
ln -s "${file}" AppDir/AppRun

cd ..
./build/linuxdeploy-x86_64.AppImage --appdir build/AppDir --output appimage

#rm -rf build
#mkdir -p build
#cmake -B build .. -DARDENS_LLVM=0 -DARDENS_DEBUGGER=0 -DARDENS_PLAYER=0 -DARDENS_DIST=1 -DCMAKE_BUILD_TYPE=Release
#cmake --build build -j $(nproc)
#for f in $(ls build/*); do
#	[[ -x "$f" ]] && cp "$f" .
#done

