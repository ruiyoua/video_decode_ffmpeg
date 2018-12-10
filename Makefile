#if use gcc, you need to link libstdc++ : gcc -lstdc++ xxx.cpp
CC = g++
CFLAGS = -g -O0 -std=c++11
CUDA_PATH = /usr/local/cuda
INCPATH = -Isrc/*.h -I$(CUDA_PATH)/include -I./include/opencv3.2 -I/home/oeasy/source/github/ffmpeg/install/include
LIBPATH = -L$(CMAKE_PREFIX_PATH)/lib -L./lib/opencv3.2 -L/home/oeasy/source/github/ffmpeg/install/lib
OPENCV_LIBS = -lopencv_core -lopencv_highgui -lopencv_imgproc
STATIC_FFMPEG_EXTRA_LIBS = -lz -llzma -lbz2 -lxcb -lxcb-shm -lxcb-shape -lxcb-xfixes -lnppig -lnppicc -lnppidei
FFMPEG_LIBS = -lavcodec -lavfilter -lavutil -lswscale -lavdevice -lavformat -lswresample
LIB =  $(FFMPEG_LIBS) $(OPENCV_LIBS) $(STATIC_FFMPEG_EXTRA_LIBS)

all : decode.o main.o yuv2bgr.o
	nvcc -ccbin $(CC) $(CFLAGS) $(LIBPATH) $(LIB)  decode.o main.o yuv2bgr.o -o video_decode

decode.o : src/video_decode.cpp
	$(CC) -o decode.o -c $(INCPATH) $(CFLAGS) src/video_decode.cpp

main.o : src/main.cpp
	$(CC) -o main.o -c $(INCPATH) $(CFLAGS) src/main.cpp

yuv2bgr.o: src/yuv2bgr.cu
	nvcc -o yuv2bgr.o -c $(INCPATH) $(CFLAGS) src/yuv2bgr.cu
	#nvcc -ccbin $(CC) -m64 $(CUDA_FLAG) -o yuv2bgr.o -c $(INCPATH) $(CFLAGS) src/yuv2bgr.cu

clean:
	rm *.o
	rm video_decode




