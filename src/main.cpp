#include "video_decode.h"
#include <iostream>
#include <thread>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

void data_callback(int err, int width, int height, unsigned char* data, std::string strRtsp)
{
	if (err == NO_ERROR)
	{
		cv::Mat image(height, width, CV_8UC3, (void*)data);
		cv::namedWindow(strRtsp);
		cv::startWindowThread();
		cv::imshow(strRtsp, image);
	}
	else
	{
		printf("!!!!!!!!!!!!!!!!!!!!!!decode err %d!!!!!!!!!!!!!!!!!!!!!!\n", err);
	}
}

//VideoDecode *video_dec0 = NULL;
//bool IsSucc = false;
//
//void open_callback(int err, std::string strRtsp)
//{
//	if (err == NO_ERROR)
//	{
//		printf("%s, login succ", strRtsp.c_str());
//		video_dec0->DecodeThreadBegin(data_callback);
//
//	}
//}

int main()
{
	VideoDecode* video_dec0 = new VideoDecode();
	if (video_dec0->Open("rtsp://admin:oeasy123@192.168.5.96", 3) == NO_ERROR)
	{
		if (video_dec0->DecodeThreadBegin(data_callback, VideoDecode::BGR) == NO_ERROR)
		{
			printf("decode 5.96 begin\n");
		}
	}

	VideoDecode* video_dec1 = new VideoDecode();
	if (video_dec1->Open("rtsp://admin:oeasy808@192.168.5.213", 3) == NO_ERROR)
	{
		if (video_dec1->DecodeThreadBegin(data_callback, VideoDecode::BGR) == NO_ERROR)
		{
			printf("decode 5.213 begin\n");
		}
	}

	VideoDecode* video_dec2 = new VideoDecode();
	if (video_dec2->Open("rtsp://admin:oeasy123@192.168.5.246", 3) == NO_ERROR)
	{
		if (video_dec2->DecodeThreadBegin(data_callback, VideoDecode::BGR) == NO_ERROR)
		{
			printf("decode 5.246 begin\n");
		}
	}

	VideoDecode* video_dec3 = new VideoDecode();
	if (video_dec3->Open("rtsp://admin:oeasy123@192.168.5.131", 3) == NO_ERROR)
	{
		if (video_dec3->DecodeThreadBegin(data_callback, VideoDecode::BGR) == NO_ERROR)
		{
			printf("decode 5.131 begin\n");
		}
	}

	VideoDecode* video_dec4 = new VideoDecode();
	if (video_dec4->Open("rtsp://admin:oeasy808@192.168.5.108", 3) == NO_ERROR)
	{
		if (video_dec4->DecodeThreadBegin(data_callback, VideoDecode::BGR) == NO_ERROR)
		{
			printf("decode 5.108 begin\n");
		}
	}

	VideoDecode* video_dec5 = new VideoDecode();
	if (video_dec5->Open("rtsp://admin:oeasy123@192.168.5.194", 3) == NO_ERROR)
	{
		if (video_dec5->DecodeThreadBegin(data_callback, VideoDecode::BGR) == NO_ERROR)
		{
			printf("decode 5.194 begin\n");
		}
	}

//	if (video_dec0.Open("rtsp://admin:oeasy808@192.168.5.213") == 0)
//		video_dec0.DecodeThreadBegin(data_callback);
//
//	VideoDecode video_dec1;
//	if (video_dec1.Open("rtsp://admin:oeasy123@192.168.5.96", 3) == 0)
//		video_dec1.DecodeThreadBegin(data_callback);
//
//	VideoDecode video_dec2;
//	if (video_dec2.Open("rtsp://admin:oeasy123@192.168.5.246") == 0)
//		video_dec2.DecodeThreadBegin(data_callback);
//
//	VideoDecode video_dec3;
//	if (video_dec3.Open("rtsp://admin:oeasy123@192.168.5.131") == 0)
//		video_dec3.DecodeThreadBegin(data_callback);
//
//	VideoDecode video_dec4;
//	if (video_dec4.Open("rtsp://admin:oeasy808@192.168.5.140") == 0)
//		video_dec4.DecodeThreadBegin(data_callback);
//
//	VideoDecode video_dec5;
//	if (video_dec5.Open("rtsp://admin:oeasy123@192.168.5.194") == 0)
//		video_dec5.DecodeThreadBegin(data_callback);
//
//	VideoDecode video_dec6;
//	if (video_dec6.Open("rtsp://admin:oeasy808@192.168.5.108", 1) == 0)
//		video_dec6.DecodeThreadBegin(data_callback);
//
//	VideoDecode video_dec7;
//	if (video_dec7.Open("rtsp://admin:oeasy808@192.168.5.147", 1) == 0)
//		video_dec7.DecodeThreadBegin(data_callback);
//
//	VideoDecode video_dec8;
//	if (video_dec8.Open("rtsp://admin:oeasy808@192.168.5.117", 1) == 0)
//		video_dec8.DecodeThreadBegin(data_callback);

	getchar();

	return 0;
}
