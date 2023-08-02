#!/bin/bash

rm -rf build/
mkdir build

docker run -t -i --rm -v `pwd`:/io ghcr.io/phusion/holy-build-box/hbb-64 bash /io/cmake/hbb.sh

cd build
wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage
mkdir AppDir AppDirPlayer
mv -f Ardens AppDir
mv -f ArdensPlayer AppDirPlayer
cp ../src/Ardens.desktop AppDir
cp ../src/ArdensPlayer.desktop AppDirPlayer
cp ../img/ardens.png AppDir
cp ../img/ardens.png AppDirPlayer
ln -s Ardens AppDir/AppRun
ln -s ArdensPlayer AppDirPlayer/AppRun
./linuxdeploy-x86_64.AppImage --appdir AppDir --output appimage
./linuxdeploy-x86_64.AppImage --appdir AppDirPlayer --output appimage
zip Ardens_linux_x64.zip Ardens*.AppImage

