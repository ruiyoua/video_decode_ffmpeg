#include "video_decode.h"
#include "yuv2bgr.h"
#include <cuda_runtime.h>
#include <cuda_profiler_api.h>
#include <curand.h>
#include <sys/time.h>

long getCurrentTime()	//ms
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
bool IsEqual(float a, float b)
{
	return fabs(a-b) < 1e-6;
}

#define SelectuSleep(n) struct timeval timeout_unique;\
	timeout_unique.tv_sec = 0;\
    timeout_unique.tv_usec = n;\
	select(0, NULL, NULL, NULL, &timeout_unique);

VideoDecode::VideoDecode()
{
	global_init();
	m_nCapCount = 0;
	m_lCapTime = getCurrentTime();
	m_fAveFrameRate = 0;
	m_nCalcFrameRateCount = 0;
	m_bOpened = false;
	m_bDecodeRunning = false;

	m_eDecodeMode = VideoDecode::CPU;
	m_data_cbfunc = NULL;
}

VideoDecode::~VideoDecode()
{

}

void* DecodeThreadFunc(void* param)
{
	if (!param)
	{
		return NULL;
	}

	VideoDecode* pVideoDecode = (VideoDecode*)param;
	if (!pVideoDecode)
	{
		return NULL;
	}

	return pVideoDecode->DecodeThreadProc();
}

void* OpenThreadFunc(void* param)
{
	if (!param)
	{
		return NULL;
	}

	VideoDecode* pVideoDecode = (VideoDecode*)param;
	if (!pVideoDecode)
	{
		return NULL;
	}

	return pVideoDecode->OpenThreadProc();
}

void* VideoDecode::OpenThreadProc()
{
	int err = this->rtsp_open(m_strRtsp, m_nOpenTimeOut);

	if (m_open_cbfunc)
	{
		m_open_cbfunc(err, m_strRtsp);
	}

	return NULL;
}

void VideoDecode::print_stream_info(AVFormatContext* ctx, unsigned int stream_id)
{
	//duration
	int64_t duration = ctx->duration / AV_TIME_BASE;
	//fps
	int fps_num = ctx->streams[stream_id]->r_frame_rate.num;
	int fps_den = ctx->streams[stream_id]->r_frame_rate.den;
	double fps = 0.0;
	if (fps_den > 0)
	{
		fps = fps_num / fps_den;
	}
	av_log(NULL, AV_LOG_INFO, "duration:%ld,\nfps:%f,\n", duration, fps);
}

void* VideoDecode::DecodeThreadProc()
{
	this->print_stream_info(m_ctx, m_video_stream_id);

	AVCodecParameters *codecpar = NULL;
	AVCodec *codec = NULL;
	AVCodecContext *codec_ctx = NULL;

	codecpar = m_ctx->streams[m_video_stream_id]->codecpar;
	if (m_eDecodeMode == VideoDecode::GPU){
		//Nvidia硬解码, AV_CODEC_ID_H264 cuvid.c
		codec = avcodec_find_decoder_by_name("h264_cuvid");
	} else {
		//软解码
		codec = avcodec_find_decoder(codecpar->codec_id);
	}
	if (NULL == codec){
		av_log(NULL, AV_LOG_FATAL, "mode = %d, codecpar->codec_id = %d, avcodec_find_decoder failed\n", m_eDecodeMode, codecpar->codec_id);
		m_data_cbfunc(DECODE_FIND_DECODER_ERROR, 0, 0, NULL, m_strRtsp);
		return NULL;
	}

	codec_ctx = avcodec_alloc_context3(codec);
	if (NULL == codec_ctx){
		av_log(NULL, AV_LOG_FATAL, "avcodec_alloc_context3 failed\n");
		m_data_cbfunc(DECODE_ALLOC_CTX_ERROR, 0, 0, NULL, m_strRtsp);
		return NULL;
	}

	//time_base 不需要赋值
	//codec_ctx->time_base = codecpar->sample_aspect_ratio;
	//codec_id不需要赋值
	//codec_ctx->codec_id = codecpar->codec_id;
	//硬解码不需要赋值,从硬件读取, AV_PIX_FMT_NV12;
	//软编码需要赋值
	codec_ctx->pix_fmt = AVPixelFormat(codecpar->format);
	codec_ctx->height = codecpar->height;
	codec_ctx->width = codecpar->width;
	codec_ctx->thread_count = 8; //设置解码线程数目
//	codec_ctx->thread_type  = FF_THREAD_FRAME; //设置解码type
	codec_ctx->thread_type  = FF_THREAD_SLICE; //设置解码type

	int err = avcodec_open2(codec_ctx, codec, NULL);
	if (err < 0){
		av_log(NULL, AV_LOG_FATAL, "avcodec_open2 failed\n");
		m_data_cbfunc(DECODE_CODEC_OPEN2_FAIL, 0, 0, NULL, m_strRtsp);
		return NULL;
	}

	//视频流中解码的frame
	AVFrame *frame = av_frame_alloc();
	//sws frame
	AVFrame *frame_bgr = av_frame_alloc();
	/**
	 * AV_PIX_FMT_BGRA 对应opencv中的CV_8UC4,
	 * AV_PIX_FMT_BGR24对应opencv中的CV_8UC3
	 */
	AVPixelFormat format = (m_e_data_type == RGB) ? AV_PIX_FMT_RGB24 : AV_PIX_FMT_BGR24;
	int align = 1;
	av_log(NULL, AV_LOG_INFO, "width:%d,\nheight:%d,\n", codec_ctx->width, codec_ctx->height);
	int buffer_size = av_image_get_buffer_size(format, codec_ctx->width, codec_ctx->height, align);
	unsigned char *buffer = (unsigned char*) av_malloc(buffer_size);
	if (NULL == buffer){
		av_log(NULL, AV_LOG_FATAL, "allocate buffer failed!\n");
		m_data_cbfunc(DECODE_ALLOC_BUF_FAIL, 0, 0, NULL, m_strRtsp);
		return NULL;
	}
	av_image_fill_arrays(frame_bgr->data, frame_bgr->linesize, buffer, format, codec_ctx->width, codec_ctx->height, align);

	//初始化图像pixer format进行转换的context
	struct SwsContext * sws_ctx;
	int dest_width = codec_ctx->width;
	int dest_height = codec_ctx->height;
	av_log(NULL, AV_LOG_INFO, "src_codec_id:%d,\ncodec_id:%d,\n", codecpar->codec_id, codec_ctx->codec_id);
	av_log(NULL, AV_LOG_INFO, "src_pix_format:%d,\ncodec_pix_format:%d,\ndest_pix_format:%d,\n", codecpar->format, codec_ctx->pix_fmt, format);
	//软解码
	sws_ctx = sws_getContext(codec_ctx->width,
			codec_ctx->height,
			codec_ctx->pix_fmt,
			dest_width,
			dest_height,
			format,
			SWS_FAST_BILINEAR,
			NULL, NULL,NULL);

	//硬解码
	int is_first_frame = false;
	int bufsize0, bufsize1, resolution;
	unsigned char* d_yuv = NULL;
	unsigned char* d_bgr = NULL;
	unsigned char* h_bgr = NULL;
	static const int CHANNEL_NUM = 3;
	//开始解码
	AVPacket pkt;
	bool bIsFindLost = false;
	int ret;
	long lCurTime;
	long lLastTime;
	long lEndTime = 0;
	long lSpendTime;

	while (m_bDecodeRunning){
		ret = av_read_frame(m_ctx, &pkt);
		if (ret == AVERROR(EAGAIN))
			continue;
		if (ret < 0){
			m_data_cbfunc(DECODE_READ_FRAME_ERROR, 0, 0, NULL, m_strRtsp);
			break;
		}

		if (pkt.stream_index != m_video_stream_id || pkt.size < 1)
			goto discard_packet;

		static const int MIN_I_FRAME_SIZE = 8096;
		if (pkt.lost_packet > 1){
			bIsFindLost = true;
			av_log(NULL, AV_LOG_WARNING, "read frame lost %d packet\n", pkt.lost_packet);
		}
		else if (pkt.flags == 1 && pkt.size > MIN_I_FRAME_SIZE)
			bIsFindLost = false;

		if (bIsFindLost)
			goto discard_packet;

		//解码packet
		err = avcodec_send_packet(codec_ctx, &pkt);
		if (err == AVERROR(EAGAIN) || err == AVERROR_EOF || err < 0)
			goto discard_packet;

		err = avcodec_receive_frame(codec_ctx, frame);
		if (err == AVERROR(EAGAIN))
			goto discard_packet;
		else if (err == AVERROR_EOF) {
			av_packet_unref(&pkt);
			m_data_cbfunc(DECODE_RECEIVCE_FRAME_ERROR, 0, 0, NULL, m_strRtsp);
			break;
		}
		else if (err < 0)
			goto discard_packet;

		//use gpu将frame转化为cv::Mat 格式
		if (m_eDecodeMode == VideoDecode::GPU){
			if (!is_first_frame){
				bufsize0 = frame->height * frame->linesize[0];
				bufsize1 = frame->height * frame->linesize[1] / 2;
				resolution = frame->height * frame->width;

				static const int CHANNEL_NUM = 3;
				//硬解码
			    int cuda_err = cudaMalloc((void **)&d_yuv, resolution * CHANNEL_NUM);
			    if (cuda_err != cudaSuccess){
			    	m_data_cbfunc(DECODE_CUDA_ALLOC_ERROR, 0, 0, NULL, m_strRtsp);
			    	break;
			    }
			    cuda_err = cudaMalloc((void **)&d_bgr, resolution * CHANNEL_NUM);
			    if (cuda_err != cudaSuccess){
			    	m_data_cbfunc(DECODE_CUDA_ALLOC_ERROR, 0, 0, NULL, m_strRtsp);
			    	break;
			    }
			    h_bgr = (unsigned char*)malloc(resolution * CHANNEL_NUM);
				is_first_frame = true;
			}
			cudaMemcpy(d_yuv, frame->data[0], bufsize0, cudaMemcpyHostToDevice);
			cudaMemcpy(d_yuv + bufsize0, frame->data[1], bufsize1, cudaMemcpyHostToDevice);

			cvtColor(d_yuv, d_bgr, resolution, frame->height, frame->width, frame->linesize[0]);

			cudaMemcpy(h_bgr, d_bgr, resolution * CHANNEL_NUM, cudaMemcpyDeviceToHost);

			//回调
			m_data_cbfunc(NO_ERROR ,frame->width, frame->height, h_bgr, m_strRtsp);
			m_nCapCount++;
		}else{ //软解码
			memset(buffer, 0, buffer_size);
			sws_scale(sws_ctx, frame->data,
					frame->linesize, 0, frame->height, frame_bgr->data,
					frame_bgr->linesize);

			/*show 出来的img出现问题,是因为linesize设置错误, 因为是bgr24,
			 * 所以linesize应该为 width * 3
			 * */
//			printf ("dec = %ld\n", lCurTime - lLastTime);
			lLastTime = lCurTime;

			m_data_cbfunc(NO_ERROR, frame->width, frame->height, frame_bgr->data[0], m_strRtsp);
			m_nCapCount++;
		}

discard_packet:
		av_frame_unref(frame);
		av_packet_unref(&pkt);

		if (lCurTime - m_lCapTime >= 10000 && m_nCapCount > 0){
			float fCur10FrameRate = m_nCapCount / ((lCurTime - m_lCapTime) / 1000.0);
			av_log(NULL, AV_LOG_FATAL, "------------[%s] last 10s frame rate %.2f frame/s------------\n", m_strRtsp.c_str(), fCur10FrameRate);
			if (IsEqual(m_fAveFrameRate, 0.0f)){
				m_fAveFrameRate = fCur10FrameRate;
			}	else	{
				m_fAveFrameRate = (m_fAveFrameRate * m_nCalcFrameRateCount + fCur10FrameRate) / (m_nCalcFrameRateCount + 1);
			}

			m_nCapCount = 0;
			m_lCapTime = lCurTime;
			m_nCalcFrameRateCount++;
		}

		lEndTime = getCurrentTime();

		SelectuSleep(2 * 1000);
	}
	sws_freeContext(sws_ctx);
	av_frame_unref(frame);
	av_frame_unref(frame_bgr);
	av_freep(&frame);
	av_freep(&frame_bgr);
	av_freep(&buffer);
	avcodec_close(codec_ctx);
	av_freep(&codec_ctx);
	return 0;
}

bool VideoDecode::global_init()
{
	static bool bIsInit = false;
	if (!bIsInit)
	{
		avdevice_register_all();
		avformat_network_init();
		av_log_set_level(AV_LOG_INFO);
		bIsInit = true;
	}

	return true;
}

int VideoDecode::rtsp_open(const std::string rtsp_addr, int timeout /*= 3*/)
{
	if (m_bOpened)
		return RTSP_ALREADY_OPEN;

	long lStart = getCurrentTime();
	printf("Open Begin\n");
	m_ctx = avformat_alloc_context();
	if (!m_ctx)
	{
		return ALLOCTE_CTX_ERROR;
	}

	//设置opts参数
	AVDictionary *fmt_opts = NULL;
	std::string ustimeout = "-1";
	if (timeout > 0)
		ustimeout = std::to_string(timeout * 1000 * 1000); //us -> s
	av_dict_set(&fmt_opts, "stimeout", ustimeout.c_str(), 0);  //设置超时断开连接时间(us)
	av_dict_set(&fmt_opts, "rtsp_transport", "tcp", 0);

	long lStartCon = getCurrentTime();
	printf("connect begin %s\n", rtsp_addr.c_str());
	int err = avformat_open_input(&m_ctx, rtsp_addr.c_str(), NULL, &fmt_opts);
	printf("spendtime %ld ms\n", getCurrentTime() - lStartCon);
	if (err < 0)
	{
		av_dict_free(&fmt_opts);
		return OPEN_RTSP_ERROR;
	}

	err = avformat_find_stream_info(m_ctx, NULL);
	if (err < 0)
	{
		av_dict_free(&fmt_opts);
		avformat_close_input(&m_ctx);
		return RTSP_STREAM_ERROR;
	}

	//选出分辨率最大的的video stream
	bool has_video_stream = false;
	int max_video_resolution = 0;

	for (unsigned int i = 0; i < m_ctx->nb_streams; i++)
	{
		AVStream *st = m_ctx->streams[i];
		if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			if ((st->disposition & AV_DISPOSITION_ATTACHED_PIC) &&
					(st->discard < AVDISCARD_ALL))
			{
				//audio cover stream
			}
			else
			{
				int resolution = st->codecpar->width * st->codecpar->height;
				if (resolution > max_video_resolution &&
						st->codecpar->codec_id != AV_CODEC_ID_NONE)
				{
					has_video_stream = true;
					max_video_resolution = resolution;
					m_video_stream_id = i;
				}
			}
		}
	}

	//若没有video_stream, 返回错误
	if (!has_video_stream)
	{
		av_dict_free(&fmt_opts);
		avformat_close_input(&m_ctx);
		return RTSP_NO_VIDEO_STREAM_ERROR;
	}

	printf("open %ld ms\n", getCurrentTime() - lStart);
	m_bOpened = true;
	return NO_ERROR;
}

int VideoDecode::OpenAsync(const std::string rtsp_addr, __pOpenAsyncCallBack async_cb /*= NULL*/, int timeout /*= 3*/)
{
	if (async_cb == NULL)
		return OPEN_PARAM_ERROR;

	m_strRtsp = rtsp_addr;
	m_open_cbfunc = async_cb;
	m_nOpenTimeOut = timeout;

	if (m_trdOpenRtsp.joinable())
		return OPEN_THREAD_EXIST_ERROR;
	m_trdOpenRtsp = std::thread(OpenThreadFunc, this);
	return NO_ERROR;
}

int VideoDecode::Open(const std::string rtsp_addr, int timeout /*= 3*/)
{
	m_strRtsp = rtsp_addr;
	m_nOpenTimeOut = timeout;
	return this->rtsp_open(rtsp_addr, timeout);
}

void VideoDecode::SetMode(DECODE_MODE eMode)
{
	m_eDecodeMode = eMode;
	return;
}

int VideoDecode::DecodeThreadBegin(__pDataCallBack data_func, RETURN_DATA_TYPE eType /*= RGB*/)
{
	if (!m_bOpened)
		return RTSP_NO_OPEN;

	m_data_cbfunc = data_func;
	m_e_data_type = eType;
	m_bDecodeRunning = true;
	if (m_trdDecode.joinable())		//已经创建该线程
		return DECODE_THREAD_EXIST;

	m_trdDecode = std::thread(DecodeThreadFunc, this);
	return NO_ERROR;
}

bool VideoDecode::Close()
{
	m_bDecodeRunning = false;
	m_bOpened = false;
	if (m_trdDecode.joinable())		//解码线程
		m_trdDecode.join();

	if (m_trdOpenRtsp.joinable())		//异步open线程
		m_trdOpenRtsp.join();

	//释放内存
	avformat_close_input(&m_ctx);

	return true;
}

