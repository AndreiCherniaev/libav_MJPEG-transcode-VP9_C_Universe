#!/bin/bash

#Based on https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu

#be in root of repo #cd FFMpeg_MJPEG-transcode-VP9_C_Universe/
if [ -d "FFMpeg_themself" ]; then
cd "FFMpeg_themself"
rm -Rf FFmpeg_build bin && mkdir FFmpeg_build bin
cd "FFmpeg_build"
PATH="../bin:$PATH" PKG_CONFIG_PATH="${PWD}/lib/pkgconfig" ../FFmpeg/configure \
  --prefix="${PWD}" \
  --pkg-config-flags="--static" \
  --extra-cflags="-I${PWD}/include" \
  --extra-ldflags="-L${PWD}/lib" \
  --extra-libs="-lpthread -lm" \
  --ld="g++" \
  --enable-gpl \
  --bindir="../bin" \
  --disable-x86asm \
  --enable-libopenh264 \
  --enable-shared \
  #--enable-static \
  --disable-ffplay \
  --disable-ffprobe \
  #--disable-ffmpeg \
  #--disable-swresample \
  #--disable-decoders \
  --disable-doc \
  --enable-swscale \
  --disable-encoders \
  --enable-encoder=mjpeg
PATH="../bin:$PATH" make -j16
make install
#If you plan use ffmpeg console utility then do
#export LD_LIBRARY_PATH="${PWD}/lib/"
else
  echo "err FFMpeg_themself not exists. You should be in root of repo, do cd FFMpeg_MJPEG-transcode-VP9_C_Universe"
fi
