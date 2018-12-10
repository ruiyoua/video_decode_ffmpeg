# video_codec

######## Describtion
Video Decoder by ffmpeg, inlcude cpu decoder and gpu decoder. It can read rtsp stream directly.

######## Install

1. opencv 3.2 version，opencv only use for test, never use for transcode and decode.
2. gpu decode version need to compile ffmpeg lib by yourself. Download source code from https://github.com/FFmpeg/FFmpeg(release-n4.1), use following complie command:
./configure --enable-cuda --enable-cuvid --enable-nvenc --enable-nonfree --enable-libnpp 
--extra-cflags=-I/usr/local/cuda/include --extra-ldflags=-L/usr/local/cuda/lib64

######## ffmpeg modify
read ffmpeg.patch, modify some ffmpeg source code, Solve the problem that the rtsp stream will receive some invalid frame when network is unstable.

