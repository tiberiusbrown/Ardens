#!/bin/bash

cd "$(dirname "$0")"

rm -rf build
mkdir -p build/dist

docker run --rm -v `pwd`/..:/io ghcr.io/phusion/holy-build-box/hbb-64 bash /io/cmake/hbb_dist.sh

cd build

wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

cd dist

for f in $(ls *); do
    
    rm -rf AppDir
    mkdir AppDir
    
    mv -f "$f" AppDir
    cp ../../../src/ArdensPlayer.desktop AppDir/${f}.desktop
    sed -i "s/ArdensPlayer/${f}/" AppDir/${f}.desktop
    cp ../../../img/ardens.png AppDir
    ln -s "${f}" AppDir/AppRun

    cd ../../
    ./build/linuxdeploy-x86_64.AppImage --appdir build/dist/AppDir --output appimage
    cd build/dist

done
