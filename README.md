This is C project that using libav (FFmpeg), build system is cmake. Tested on Ubuntu 23 server.

## Prerequisites
```bash
sudo apt install cmake gcc git ninja-build pkg-config
sudo apt-get install libvpx-dev libvorbis-dev libx265-dev libnuma-dev
```
```bash
git clone --recurse-submodules -j8 --remote-submodules https://github.com/AndreiCherniaev/libav_MJPEG-transcode-VP9_C_Universe.git && cd libav_MJPEG-transcode-VP9_C_Universe
```
## Build FFmpeg
Next step is about how [build](https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu#FFmpeg) FFmpeg
```bash
libav_MJPEG-transcode-VP9_C_Universe$ FFMpeg_themself/build_FFMpeg.sh
```
## Build C example
Build example
```bash
libav_MJPEG-transcode-VP9_C_Universe$ cmake -G Ninja -DCMAKE_BUILD_TYPE:STRING=Debug -S myExample/src/ -B myExample/build-host/ --fresh
libav_MJPEG-transcode-VP9_C_Universe$ ninja -j16 -C myExample/build-host/
```
Before run example we need copy input video to build folder
```bash
libav_MJPEG-transcode-VP9_C_Universe$ cp myExample/small_bunny_1080p_60fps.mp4 myExample/build-host
```
Before start we should be in folder with input.yuvj422p video file. If no then "Cannot open input file Segmentation fault (core dumped)". LD_LIBRARY_PATH should be `/home/username/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib` where "username" is current user's name. This is universal code to run example
```bash
libav_MJPEG-transcode-VP9_C_Universe$ cd myExample/build-host/
libav_MJPEG-transcode-VP9_C_Universe/myExample/build-host$ LD_LIBRARY_PATH=${PWD}/../../FFMpeg_themself/FFmpeg_build/lib ./myExample
```
## Run without LD_LIBRARY_PATH
This step is optional. If you want to run example without LD_LIBRARY_PATH then you should tell to the operating system about new locations of shared libraries.
```bash
libav_MJPEG-transcode-VP9_C_Universe$ sudo bash -c 'cat <<EOF > /etc/ld.so.conf.d/myFFMpeg.conf
#My path to my FFMpeg build
#see details https://github.com/AndreiCherniaev/libav_MJPEG-transcode-VP9_C_Universe
${PWD}/FFMpeg_themself/FFmpeg_build/lib
EOF'
sudo ldconfig -v
```
Now we can run example as usual
```bash
./myExample
```
## Synthesize MJPEG video
input.yuvj422p video I plan use as input to test my C example. This step is optional, you can use myExample/input.yuvj422p This step is if you want to synthesize video again.
```bash
libav_MJPEG-transcode-VP9_C_Universe$ FFMpeg_themself/bin/ffmpeg -y -f lavfi -i testsrc=size=1280x720:rate=1:duration=10 -vcodec mjpeg -pix_fmt yuvj422p -f mjpeg myExample/input.yuvj422p
```
If you get error
"FFMpeg_themself/bin/ffmpeg: error while loading shared libraries: libavdevice.so.60: cannot open shared object file: No such file or directory" then be in root of repo and do
```bash
libav_MJPEG-transcode-VP9_C_Universe$ export LD_LIBRARY_PATH="${PWD}/FFMpeg_themself/lib/"
```
And try again Synthesize MJPEG video.

## In case of error 
Check [dependency](https://habr.com/ru/articles/220961/).
```bash
chrpath -l myExample/build-host/myExample
```
You do the same and compare my log with yours:
```bash
libav_MJPEG-transcode-VP9_C_Universe$ objdump -p myExample/build-host/myExample | grep NEEDE
  NEEDED               libavcodec.so.60
  NEEDED               libavutil.so.58
  NEEDED               libavformat.so.60
  NEEDED               libavfilter.so.9
  NEEDED               libc.so.6
  
ldd myExample/build-host/myExample
	linux-vdso.so.1 (0x00007ffc8c9f9000)
        libavcodec.so.60 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavcodec.so.60 (0x00007ff1cfe00000)
        libavutil.so.58 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavutil.so.58 (0x00007ff1cec00000)
        libavformat.so.60 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavformat.so.60 (0x00007ff1ce800000)
        libavfilter.so.9 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavfilter.so.9 (0x00007ff1ce400000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007ff1ce000000)
        libswresample.so.4 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswresample.so.4 (0x00007ff1d0fc7000)
	libvpx.so.7 => /lib/x86_64-linux-gnu/libvpx.so.7 (0x00007ff1cdc00000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007ff1cfd17000)
        libswscale.so.7 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswscale.so.7 (0x00007ff1ceb89000)
	/lib64/ld-linux-x86-64.so.2 (0x00007ff1d0ff1000)
	
  
objdump -p ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavcodec.so | grep NEEDE
  NEEDED               libswresample.so.4
  NEEDED               libavutil.so.58
  NEEDED               libvpx.so.7
  NEEDED               libm.so.6
  NEEDED               libc.so.6

ldd ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavcodec.so
	linux-vdso.so.1 (0x00007ffc6612f000)
        libswresample.so.4 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswresample.so.4 (0x00007f6a3eee0000)
        libavutil.so.58 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavutil.so.58 (0x00007f6a3ca00000)
	libvpx.so.7 => /lib/x86_64-linux-gnu/libvpx.so.7 (0x00007f6a3c600000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f6a3edf7000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f6a3c200000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f6a3ef00000)


objdump -p ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavdevice.so | grep NEEDE
  NEEDED               libavfilter.so.9
  NEEDED               libavformat.so.60
  NEEDED               libavcodec.so.60
  NEEDED               libavutil.so.58
  NEEDED               libc.so.6
  
ldd ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavdevice.so
	linux-vdso.so.1 (0x00007fffe79ee000)
        libavfilter.so.9 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavfilter.so.9 (0x00007f8db2800000)
        libavformat.so.60 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavformat.so.60 (0x00007f8db2400000)
        libavcodec.so.60 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavcodec.so.60 (0x00007f8db1200000)
        libavutil.so.58 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavutil.so.58 (0x00007f8db0000000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f8dafc00000)
        libswscale.so.7 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswscale.so.7 (0x00007f8db2789000)
        libswresample.so.4 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswresample.so.4 (0x00007f8db2c0d000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f8db26a0000)
	libvpx.so.7 => /lib/x86_64-linux-gnu/libvpx.so.7 (0x00007f8daf800000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f8db2c40000)


objdump -p ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavfilter.so | grep NEEDE
  NEEDED               libswscale.so.7
  NEEDED               libavformat.so.60
  NEEDED               libavcodec.so.60
  NEEDED               libswresample.so.4
  NEEDED               libavutil.so.58
  NEEDED               libm.so.6
  NEEDED               libc.so.6
  
ldd ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavfilter.so
	linux-vdso.so.1 (0x00007ffd34fe1000)
        libswscale.so.7 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswscale.so.7 (0x00007fc76863c000)
        libavformat.so.60 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavformat.so.60 (0x00007fc767e00000)
        libavcodec.so.60 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavcodec.so.60 (0x00007fc766c00000)
        libswresample.so.4 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswresample.so.4 (0x00007fc768623000)
        libavutil.so.58 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavutil.so.58 (0x00007fc765a00000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007fc768117000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fc765600000)
	libvpx.so.7 => /lib/x86_64-linux-gnu/libvpx.so.7 (0x00007fc765200000)
	/lib64/ld-linux-x86-64.so.2 (0x00007fc7686ba000)


objdump -p ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavformat.so | grep NEEDE
  NEEDED               libavcodec.so.60
  NEEDED               libavutil.so.58
  NEEDED               libm.so.6
  NEEDED               libc.so.6
  
ldd ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavformat.so
	linux-vdso.so.1 (0x00007fffad7ec000)
        libavcodec.so.60 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavcodec.so.60 (0x00007f4498600000)
        libavutil.so.58 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavutil.so.58 (0x00007f4497400000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f4498517000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f4497000000)
        libswresample.so.4 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswresample.so.4 (0x00007f4499b3b000)
	libvpx.so.7 => /lib/x86_64-linux-gnu/libvpx.so.7 (0x00007f4496c00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f4499b5b000)


objdump -p ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavutil.so | grep NEEDE
  NEEDED               libm.so.6
  NEEDED               libc.so.6
  
ldd ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavutil.so
	linux-vdso.so.1 (0x00007ffd821fd000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007faa92d17000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007faa92a00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007faa93f46000)


objdump -p ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswresample.so | grep NEEDE
  NEEDED               libavutil.so.58
  NEEDED               libm.so.6
  NEEDED               libc.so.6
  
ldd ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswresample.so
	linux-vdso.so.1 (0x00007ffd85927000)
        libavutil.so.58 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavutil.so.58 (0x00007f416b200000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f416b117000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f416ae00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f416c387000)


objdump -p ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswscale.so | grep NEEDE
  NEEDED               libavutil.so.58
  NEEDED               libm.so.6
  NEEDED               libc.so.6
  
ldd ~/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libswscale.so
	linux-vdso.so.1 (0x00007fff5e561000)
        libavutil.so.58 => /home/q/libav_MJPEG-transcode-VP9_C_Universe/FFMpeg_themself/FFmpeg_build/lib/libavutil.so.58 (0x00007f574c000000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f574d0dc000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f574bc00000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f574d243000)
```
