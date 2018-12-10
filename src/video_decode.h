#ifndef __VIDEO_DECODE_H__
#define __VIDEO_DECODE_H__

#include <iostream>
#include <thread>
#include "err.h"

#ifdef __cplusplus
extern "C"{
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include "libswresample/swresample.h"
#include "libavfilter/avfilter.h"
}
#endif

typedef void(*__pDataCallBack)(int err, int width, int height, unsigned char* data, std::string strRtsp);
typedef void(*__pOpenAsyncCallBack)(int err, std::string strRtsp);

class VideoDecode
{
public:
	VideoDecode();
	~VideoDecode();

public:
	typedef enum
	{
		GPU = 0,
		CPU = 1,
	}DECODE_MODE;

	typedef enum
	{
		RGB = 0,
		BGR = 1,
	}RETURN_DATA_TYPE;

public:
	int Open(const std::string rtsp_addr, int timeout = 3); //timeout: s
	int OpenAsync(const std::string rtsp_addr, __pOpenAsyncCallBack async_cb = NULL, int timeout = 3);
	void SetMode(DECODE_MODE eMode);		//GPU 0, CPU 1
	bool Close();
	int DecodeThreadBegin(__pDataCallBack data_func, RETURN_DATA_TYPE eType = RGB);

public:
	void* DecodeThreadProc();
	void* OpenThreadProc();

private:
	int rtsp_open(const std::string rtsp_addr, int timeout = 3);
	bool global_init();

	void print_stream_info(AVFormatContext* ctx, unsigned int stream_id);
	int gen_codec_param();

private:
	DECODE_MODE m_eDecodeMode;
	__pDataCallBack m_data_cbfunc;
	RETURN_DATA_TYPE m_e_data_type;
	__pOpenAsyncCallBack m_open_cbfunc;
	int m_nOpenTimeOut;
	std::string m_strRtsp;
	std::thread m_trdDecode;
	std::thread m_trdOpenRtsp;
	bool m_bDecodeRunning;
	bool m_bOpened;

	//帧数计算
	long m_lCapTime;
	int m_nCapCount;
	float m_fAveFrameRate;
	int m_nCalcFrameRateCount;

	AVFormatContext *m_ctx;
	unsigned int m_video_stream_id;
};


#endif //__VIDEO_DECODE_H__
