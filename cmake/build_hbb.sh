#!/usr/bin/env bash

set -euxo pipefail

rm -rf build
mkdir build

export ARCH=x86_64

docker run --rm \
    -v "$(pwd):/io" \
    ghcr.io/phusion/holy-build-box/hbb-64 \
    bash /io/cmake/hbb.sh

cd build

# Do not silently publish incomplete builds.
for file in Ardens ArdensPlayer ardens_libretro.so; do
    if [[ ! -s "$file" ]]; then
        echo "ERROR: HBB build did not produce $file" >&2
        exit 1
    fi
done

wget -q \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

rm -rf AppDir AppDirPlayer
mkdir AppDir AppDirPlayer

./linuxdeploy-x86_64.AppImage \
    --appdir AppDir \
    --executable ./Ardens \
    --desktop-file ../src/Ardens.desktop \
    --icon-file ../img/ardens.png \
    --output appimage

./linuxdeploy-x86_64.AppImage \
    --appdir AppDirPlayer \
    --executable ./ArdensPlayer \
    --desktop-file ../src/ArdensPlayer.desktop \
    --icon-file ../img/ardens.png \
    --output appimage

# Catch dangling AppRun links or missing payloads before publishing.
test -x AppDir/AppRun
test -x AppDir/usr/bin/Ardens
test -x AppDirPlayer/AppRun
test -x AppDirPlayer/usr/bin/ArdensPlayer

test -s Ardens-x86_64.AppImage
test -s ArdensPlayer-x86_64.AppImage
test -s ardens_libretro.so

rm -f Ardens_linux_x64.zip
zip Ardens_linux_x64.zip \
    Ardens-x86_64.AppImage \
    ArdensPlayer-x86_64.AppImage \
    ardens_libretro.so
