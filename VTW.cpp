#include "VTW.h"
HINSTANCE g_hInstance;

VTWPARAMS::VTWPARAMS()
{
	MinWhite = 225;
	MaxWhite = 255;
	MinBlack = 16;
	MaxBlack = 46;
	bInsteadColor = WHITE;
	Width = GetSystemMetrics(SM_CXSCREEN);
	Height = GetSystemMetrics(SM_CYSCREEN);
	RectMinSizeLong = (Width + Height) / 200;
	RectMinSizeShort = RectMinSizeLong * Height / Width;
	ComputeMethod = COMPUTE_WINDOW_METHOD::EXTEND_METHOD;

	startx = 0;
	starty = 0;

	//视频结构体初始化
	v.fmtCtx = avformat_alloc_context();
	v.codecCtx = nullptr;
	v.frame = av_frame_alloc();
	v.SwsCtx = nullptr;
	v.pkt = av_packet_alloc();
	v.VideoPTS = 0.0;

	//音频结构体初始化
	a.bThreadRunning = FALSE;
	a.hThread = nullptr;
	a.fmtCtx = avformat_alloc_context();
	a.codecCtx = nullptr;
	a.pXAudio2 = nullptr;
	a.wfx.wFormatTag = WAVE_FORMAT_PCM;
	a.wfx.wBitsPerSample = 16; //因为我们要转为PCM格式，所以固定为16位
	InitializeCriticalSection(&a.bufferCS);
	a.hStartEvent = nullptr;
	a.bStartEventTriggered = FALSE;
	a.hFreeBufferEvent = nullptr;
	a.pSourceVoice = nullptr;
	a.pSourceVoice = nullptr;
	a.AudioClock = 0.0;
	InitializeCriticalSection(&a.AudioClockCS);
	a.pkt = av_packet_alloc();
	a.SwrCtx = nullptr;
	a.frame = av_frame_alloc();

	pHeader = nullptr;
	wndnum = 0;
	curWndIndex = 0;


	g_hInstance = GetModuleHandle(NULL);
	WNDCLASSEX wndclassex;
	wndclassex.cbSize = sizeof(WNDCLASSEX);
	wndclassex.style = CS_HREDRAW | CS_VREDRAW;
	wndclassex.lpfnWndProc = VTWProc;
	wndclassex.cbClsExtra = 0;
	wndclassex.cbWndExtra = 0;
	wndclassex.hInstance = g_hInstance;
	wndclassex.hIcon = LoadIcon(g_hInstance,MAKEINTRESOURCE(IDI_VTW));
	wndclassex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclassex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclassex.lpszMenuName = NULL;
	wndclassex.lpszClassName = TEXT("VTW");
	wndclassex.hIconSm = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_VTW));

	RegisterClassEx(&wndclassex);
}

VTWPARAMS::~VTWPARAMS()
{
	WNDPARAMS* current = pHeader;
	while (current != nullptr) 
	{
		WNDPARAMS* next = current->next;
		if (current->hwnd) DestroyWindow(current->hwnd);
		delete current;
		current = next;
	}
	while (!WindowsPool.empty())
	{
		current = WindowsPool.back();
		WindowsPool.pop_back();
		if (current->hwnd) DestroyWindow(current->hwnd);
		delete current;
	}
	// 确保音频线程已停止并清理资源
	a.bThreadRunning = FALSE;
	if (a.hFreeBufferEvent) SetEvent(a.hFreeBufferEvent);  
	if (a.hThread) 
	{
		WaitForSingleObject(a.hThread, INFINITE);
		CloseHandle(a.hThread);
	}
	if (a.hFreeBufferEvent) CloseHandle(a.hFreeBufferEvent);
	if (a.hStartEvent) CloseHandle(a.hStartEvent);
	DeleteCriticalSection(&a.bufferCS);
	DeleteCriticalSection(&a.AudioClockCS);
	if (a.pSourceVoice) a.pSourceVoice->DestroyVoice();
	if (a.pMasteringVoice) a.pMasteringVoice->DestroyVoice();
	if (a.pXAudio2) a.pXAudio2->Release();
	if (a.pCallback) 
	{
		delete a.pCallback;
		a.pCallback = nullptr;
	}

	av_frame_free(&v.frame);
	av_frame_free(&a.frame);
	av_packet_free(&v.pkt);
	av_packet_free(&a.pkt);
	avcodec_free_context(&v.codecCtx);
	avcodec_free_context(&a.codecCtx);
	avformat_close_input(&v.fmtCtx);
	avformat_close_input(&a.fmtCtx);
	sws_freeContext(v.SwsCtx);
	swr_free(&a.SwrCtx);
}

VOID VTWPARAMS::SetVideoPath(LPSTR lpFileName)
{
	StringCchCopyA(szFileName, MAX_PATH, lpFileName);
	return;
}

VOID VTWPARAMS::SetWhiteRanges(int min, int max)
{
	MinWhite = min;
	MaxWhite = max;
	return;
}

VOID VTWPARAMS::SetBlackRanges(int min, int max)
{
	MinBlack = min;
	MaxBlack = max;
	return;
}

VOID VTWPARAMS::SetInsteadColor(BOOL bBlack)
{
	bInsteadColor = bBlack;
	return;
}

VOID VTWPARAMS::SetUsedResize(BOOL bUsed)
{
	bUsedResize = bUsed;
	return;
}

VOID VTWPARAMS::SetResizeSize(int rwidth, int rheight)
{
	ResizeWidth = rwidth;
	ResizeHeight = rheight;
	return;
}

VOID VTWPARAMS::SetDisplaySize(int width, int height)
{
	Width = width;
	Height = height;
	return;
}

int VTWPARAMS::GetWidth()
{
	return Width;
}

int VTWPARAMS::GetHeight()
{
	return Height;
}

VOID VTWPARAMS::SetRectMinSize(int size)
{
	RectMinSizeLong = size;
	RectMinSizeShort = size * Height / Width;
	return;
}

VOID VTWPARAMS::SetComputeMethod(COMPUTE_WINDOW_METHOD method)
{
	ComputeMethod = method;
	return;
}

double VTWPARAMS::GetAudioClock()
{
	EnterCriticalSection(&a.AudioClockCS);
	double clock = a.AudioClock;
	LeaveCriticalSection(&a.AudioClockCS);
	return clock;
}

double VTWPARAMS::GetVideoPTS()
{
	return v.VideoPTS;
}

VOID VTWPARAMS::ResetAudioClock()
{
	a.AudioClock = 0.0;
	return;
}

VOID VTWPARAMS::ResetVideoPTS()
{
	v.VideoPTS = 0.0;
	return;
}

double VTWPARAMS::GetVideoFPS()
{
	if (v.fmtCtx && v.VideoStreamIndex >= 0) {
		AVStream* stream = v.fmtCtx->streams[v.VideoStreamIndex];
		double fps = av_q2d(stream->avg_frame_rate);
		if (fps <= 0) {
			// 如果 avg_frame_rate 无效，尝试用 r_frame_rate
			fps = av_q2d(stream->r_frame_rate);
		}
		if (fps > 0) return fps;
	}
	return 24.0;
}

BOOL VTWPARAMS::AutoUpdate()
{
	startx = (GetSystemMetrics(SM_CXSCREEN) - Width )/ 2;
	starty = (GetSystemMetrics(SM_CYSCREEN) - Height )/ 2;

	avformat_open_input(&v.fmtCtx, szFileName, NULL, NULL);
	avformat_find_stream_info(v.fmtCtx, NULL);
	for (int i = 0;i < v.fmtCtx->nb_streams;i++)
	{ 	
		const AVCodec* vcodec = avcodec_find_decoder(v.fmtCtx->streams[i]->codecpar->codec_id);
		if (vcodec->type == AVMEDIA_TYPE_VIDEO)
		{
			v.VideoStreamIndex = i;
			v.codecCtx = avcodec_alloc_context3(vcodec);
			avcodec_parameters_to_context(v.codecCtx, v.fmtCtx->streams[i]->codecpar);
			if (!avcodec_open2(v.codecCtx, vcodec, NULL))
				break;
		}
		if (i == v.fmtCtx->nb_streams - 1)
			return FALSE;
	}

	//音频需要另外打开，因为FFmpeg的设计
	avformat_open_input(&a.fmtCtx, szFileName, NULL, NULL);
	avformat_find_stream_info(a.fmtCtx, NULL);
	for (int i = 0; i < a.fmtCtx->nb_streams; i++)
	{
		const AVCodec* acodec = avcodec_find_decoder(a.fmtCtx->streams[i]->codecpar->codec_id);
		if (acodec->type == AVMEDIA_TYPE_AUDIO)
		{
			a.AudioStreamIndex = i;
			a.codecCtx = avcodec_alloc_context3(acodec);
			avcodec_parameters_to_context(a.codecCtx, a.fmtCtx->streams[i]->codecpar);
			if (!avcodec_open2(a.codecCtx, acodec, NULL))
				break;
		}
		if (i == a.fmtCtx->nb_streams - 1)
			return FALSE;
	}
	//视频重采样
	if (bUsedResize)
	{
		v.SwsCtx = sws_getContext(v.codecCtx->width, v.codecCtx->height, v.codecCtx->pix_fmt,
		                          ResizeWidth, ResizeHeight, AV_PIX_FMT_YUV444P, SWS_AREA, NULL,
		                          NULL, NULL);
		v.data.resize(ResizeWidth * ResizeHeight);
		v.processed.resize(ResizeWidth * ResizeHeight, false);//见VTW.h中processed的定义
		v.planeSize = static_cast<size_t>(ResizeWidth) * static_cast<size_t>(ResizeHeight);
		v.planeY.resize(v.planeSize);
		v.planeU.resize(v.planeSize);
		v.planeV.resize(v.planeSize);
		// 不要将 ResizeTemp 大量 resize（会创建大量默认元素并在后续帧中累积），改为清空并可选预留
		v.ResizeTemp.clear();
		v.ResizeTemp.reserve((size_t)max(1024, (ResizeWidth * ResizeHeight) / 100));
		// ResizeRatio 表示从缩放后坐标映射到显示坐标的乘数（display = resized * ResizeRatio）
		ResizeRatioX = static_cast<float>(Width) / static_cast<float>(ResizeWidth);
		ResizeRatioY = static_cast<float>(Height) / static_cast<float>(ResizeHeight);
	}
	else
	{
		v.SwsCtx = sws_getContext(v.codecCtx->width, v.codecCtx->height, v.codecCtx->pix_fmt,
		                          Width, Height, AV_PIX_FMT_YUV444P, SWS_POINT, NULL,
		                          NULL, NULL);
		v.data.resize(Width * Height);
		v.processed.resize(Width * Height, false);//见VTW.h中processed的定义
		v.planeSize = static_cast<size_t>(Width) * static_cast<size_t>(Height);
		v.planeY.resize(v.planeSize);
		v.planeU.resize(v.planeSize);
		v.planeV.resize(v.planeSize);
	}

	//音频重采样及XAudio2初始化
	swr_alloc_set_opts2(&a.SwrCtx,&a.codecCtx->ch_layout, AV_SAMPLE_FMT_S16,
	                    a.codecCtx->sample_rate, &a.codecCtx->ch_layout,
	                    a.codecCtx->sample_fmt, a.codecCtx->sample_rate,
	                    0, NULL);//转为PCM格式即可，别的不变
	swr_init(a.SwrCtx);
	if (a.SwrCtx == 0)
		return FALSE;
	a.wfx.nChannels = a.codecCtx->ch_layout.nb_channels;
	a.wfx.nSamplesPerSec = a.codecCtx->sample_rate;
	a.wfx.nBlockAlign = a.wfx.nChannels * a.wfx.wBitsPerSample / 8;
	a.wfx.nAvgBytesPerSec = a.wfx.nSamplesPerSec * a.wfx.nBlockAlign;	
	a.wfx.cbSize = 0;
	size_t bufferSize = av_samples_get_buffer_size(NULL, a.wfx.nChannels, 8192,
	                                               AV_SAMPLE_FMT_S16, 1);
	for (int i = 0; i < 4; i++)
		a.pcmBuffers[i] = (uint8_t*)VirtualAlloc(NULL, bufferSize,
		                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);//预分配4个PCM缓冲区，避免在循环中频繁分配内存，
	//分配的内存大小为通道数 × 采样个数 × 采样位数 / 8
	a.hStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	a.hFreeBufferEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	a.pCallback = new VoiceCallback(this);
	HRESULT hr;
	hr=XAudio2Create(&a.pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr))
		return FALSE;
	hr=a.pXAudio2->CreateMasteringVoice(&a.pMasteringVoice);
	if (FAILED(hr))
		return FALSE;
	hr=a.pXAudio2->CreateSourceVoice(&a.pSourceVoice, (WAVEFORMATEX*)&a.wfx, 0,
	                                 XAUDIO2_DEFAULT_FREQ_RATIO, a.pCallback, NULL, NULL);
	if (FAILED(hr))
		return FALSE;
	a.hThread=CreateThread(NULL, 0, AudioThread, this, 0, NULL);
	if (!a.hThread)
		return FALSE;
	a.bThreadRunning = TRUE;

	return TRUE;
}

BOOL VTWPARAMS::RequestFrame()
{
	int ret;

	while (TRUE)
	{
		ret = av_read_frame(v.fmtCtx, v.pkt);
		//检测返回值，0表示成功；AVERROR_EOF表示文件读至末尾，此时应发送NULL给解码器
		if (ret == 0 && v.pkt->stream_index == v.VideoStreamIndex)
			avcodec_send_packet(v.codecCtx, v.pkt);
		else if (ret == AVERROR_EOF)
			avcodec_send_packet(v.codecCtx, NULL);

		ret = avcodec_receive_frame(v.codecCtx, v.frame);
			
		if (ret == 0)
		{
			av_packet_unref(v.pkt);
			//获取视频PTS
			v.VideoPTS = v.frame->pts * av_q2d(v.fmtCtx->streams[v.VideoStreamIndex]->time_base);
			return TRUE;
		}
		else if (ret == AVERROR(EAGAIN))
		{
			av_packet_unref(v.pkt);
			continue;
		}
		else if (ret == AVERROR_EOF)
		{
			av_packet_unref(v.pkt);
			return FALSE;
		}
	}
}

BOOL VTWPARAMS::ComputeWindow()
{
	curWndIndex = 0;

	// 清空 ResizeTemp（每帧应重建缩放后矩形列表，防止帧间残留）
	v.ResizeTemp.clear();

	// 设置目标指针
	uint8_t* temp[AV_NUM_DATA_POINTERS] = 
	{
		v.planeY.data(),
		v.planeU.data(),
		v.planeV.data(),
		nullptr, nullptr, nullptr, nullptr, nullptr
	};
	int linesize[AV_NUM_DATA_POINTERS] = { 0 };
	if (bUsedResize)
		av_image_fill_linesizes(linesize, AV_PIX_FMT_YUV444P, ResizeWidth);
	else
		av_image_fill_linesizes(linesize, AV_PIX_FMT_YUV444P, Width);

	// 转换图像格式
	int ret = sws_scale(v.SwsCtx, v.frame->data, v.frame->linesize, 0, v.codecCtx->height, temp, linesize);
	if (ret <= 0) 
	{
		return FALSE;
	}

	// 填充 data（确保不会越界）
	for (size_t i = 0, n = v.planeSize; i < n; ++i) 
	{
		v.data[i].y = v.planeY[i];
	}

	switch (ComputeMethod)
	{
	case COMPUTE_WINDOW_METHOD::EXTEND_METHOD:
		ComputeWindow_EXTEND_METHOD();
		break;

	case COMPUTE_WINDOW_METHOD::GREEDY_METHOD:
		ComputeWindow_GREEDY_METHOD();
		break;
	}

	if (bUsedResize)
		R_RectToWindow();
	else
		RectToWindow(-1, -1, -1, -1);

	return TRUE;

}

VOID VTWPARAMS::RectToWindow(int x, int y, int width, int height)
{
	// 帧结束：删除多余窗口
	if (x == -1)
	{
		int toDelete = wndnum - curWndIndex;   // 应删除的数量
		if (toDelete > 0)
			DeleteNumberOfEndWindow(toDelete);
		wndnum = curWndIndex;                   // 更新总窗口数
		curWndIndex = 0;                         // 重置索引
		return;
	}

	// 处理新矩形
	if (curWndIndex < wndnum)
	{
		// 复用第 curWndIndex 个窗口
		WNDPARAMS* p = pHeader;
		for (int j = 0; j < curWndIndex; ++j)
			if (p)
				p = p->next;
		if (p)
		{
			p->x = x;
			p->y = y;
			p->width = width;
			p->height = height;
		}
	}
	else
	{
		// 需要创建新窗口
		CreateNewWindow(x, y, width, height);
	}

	++curWndIndex;
}

VOID VTWPARAMS::R_RectToWindow()
{
	int x, y, width, height;
	for (size_t i = 0; i < v.ResizeTemp.size(); ++i)
	{
		x = static_cast<int>(round(v.ResizeTemp[i].left * ResizeRatioX));
		y = static_cast<int>(round(v.ResizeTemp[i].top * ResizeRatioY));
		width = static_cast<int>(round((v.ResizeTemp[i].right - v.ResizeTemp[i].left) * ResizeRatioX));
		height = static_cast<int>(round((v.ResizeTemp[i].bottom - v.ResizeTemp[i].top) * ResizeRatioY));
		RectToWindow(startx+x, starty+y, width, height);
	}
	RectToWindow(-1, -1, -1, -1);
}

VOID VTWPARAMS::DisplayWindowFrame()
{
	//cout << "当前窗口数: " << wndnum << endl;//调试用

	if (pHeader == nullptr || wndnum == 0)
		return;


	HDWP hdwp = BeginDeferWindowPos(wndnum);
	WNDPARAMS* pBuffer = pHeader;
	while (pBuffer != nullptr)
	{
		if (pBuffer->hwnd)
		{
			hdwp = DeferWindowPos(hdwp, pBuffer->hwnd, HWND_TOP,
			                      pBuffer->x, pBuffer->y,
			                      pBuffer->width, pBuffer->height,
			                      SWP_NOACTIVATE | SWP_NOCOPYBITS);
		}
		pBuffer = pBuffer->next;
	}
	if (hdwp) EndDeferWindowPos(hdwp);

    pBuffer = pHeader;
    while (pBuffer != nullptr)
    {
        if (pBuffer->hwnd && !IsWindowVisible(pBuffer->hwnd))
        {
            // 使用 SetWindowPos 替代 ShowWindow/UpdateWindow，避免激活并一次性设置位置和显示状态
            SetWindowPos(pBuffer->hwnd, HWND_TOP, pBuffer->x, pBuffer->y, pBuffer->width, pBuffer->height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        pBuffer = pBuffer->next;
    }

	GdiFlush();

	if (a.hStartEvent && !a.bStartEventTriggered)
	{
		SetEvent(a.hStartEvent);
		a.bStartEventTriggered = TRUE;
	}

}

VOID VTWPARAMS::CreateNewWindow(int x, int y, int width, int height)
{
	if (WindowsPool.empty())
	{
		if (pHeader == nullptr)
		{
			pHeader = new WNDPARAMS();
			pHeader->next = nullptr;
			pHeader->x = x; pHeader->y = y; pHeader->width = width; pHeader->height = height;
			pHeader->hwnd = CreateWindowExA(WS_EX_TOPMOST|WS_EX_NOACTIVATE,"VTW", szFileName,
			                                WS_POPUP, x, y, width, height, NULL, NULL, g_hInstance, NULL);
            if (pHeader->hwnd)
            {
				SetWindowPos(pHeader->hwnd, HWND_TOP, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
			wndnum = 1;
			return;
		}
		WNDPARAMS* pBuffer = pHeader;
		while (pBuffer->next != nullptr) pBuffer = pBuffer->next;
		pBuffer->next = new WNDPARAMS();
		pBuffer = pBuffer->next;
		pBuffer->next = nullptr;
		pBuffer->x = x; pBuffer->y = y; pBuffer->width = width; pBuffer->height = height;
		pBuffer->hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_NOACTIVATE, "VTW", szFileName,
		                                WS_POPUP, x, y, width, height, NULL, NULL, g_hInstance, NULL);
        if (pBuffer->hwnd)
        {
            SetWindowPos(pBuffer->hwnd, HWND_TOP, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
		++wndnum;
	}
	else
	{
		if (pHeader == nullptr)
		{
			pHeader = WindowsPool.back();
			WindowsPool.pop_back();
			pHeader->next = nullptr;
			pHeader->x = x; pHeader->y = y; pHeader->width = width; pHeader->height = height;
            if (pHeader->hwnd)
            {
                SetWindowPos(pHeader->hwnd, HWND_TOP, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
			wndnum = 1;
			return;
		}
		WNDPARAMS* pBuffer = pHeader;
		while (pBuffer->next != nullptr) 
			pBuffer = pBuffer->next;
		pBuffer->next = WindowsPool.back();
		WindowsPool.pop_back();
		pBuffer = pBuffer->next;
		pBuffer->next = nullptr;
		pBuffer->x = x; pBuffer->y = y; pBuffer->width = width; pBuffer->height = height;
        if (pBuffer->hwnd)
        {
            SetWindowPos(pBuffer->hwnd, HWND_TOP, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
		++wndnum;
	}
}

VOID VTWPARAMS::DeleteNumberOfEndWindow(int number)
{
	if (number <= 0 || pHeader == nullptr || wndnum <= 0) return;

	// 若删除数量 >= 当前数量，则删除全部并重置头
	if (number >= wndnum)
	{
		WNDPARAMS* cur = pHeader;
		while (cur) 
		{
			WNDPARAMS* next = cur->next;
			if (cur->hwnd)
			{
				ShowWindow(cur->hwnd, SW_HIDE);
				WindowsPool.push_back(cur);
			}
			cur = next;
		}
		pHeader = nullptr;
		wndnum = 0;
		return;
	}

	// 找到保留链表的最后一个节点（新的尾）
	int keep = wndnum - number;
	WNDPARAMS* prev = pHeader;
	for (int i = 1; i < keep; ++i)
		if (prev->next) prev = prev->next;

	// 将 prev->next 之后的所有节点存入窗口池
	WNDPARAMS* cur = prev->next;
	while (cur) 
	{
		WNDPARAMS* next = cur->next;
		if (cur->hwnd)
		{
			ShowWindow(cur->hwnd, SW_HIDE);
			WindowsPool.push_back(cur);
		}
		cur = next;
	}

	prev->next = nullptr;
	wndnum -= number;
}

VOID VTWPARAMS::StopAudioThread()
{
	a.bThreadRunning = FALSE;
	if (a.hFreeBufferEvent) SetEvent(a.hFreeBufferEvent);  // 唤醒线程以便它能检查 bThreadRunning
	if (a.hThread) 
	{
		WaitForSingleObject(a.hThread, INFINITE);
		CloseHandle(a.hThread);
		a.hThread = nullptr;
	}
}

//音频线程
DWORD WINAPI VTWPARAMS::AudioThread(LPVOID lpParam)
{

	VTWPARAMS* pThis = (VTWPARAMS*)lpParam;

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	int ret;
	bool first = true;
	double BufferDuration = 0.0;

	WaitForSingleObject(pThis->a.hStartEvent, INFINITE);
	pThis->a.pSourceVoice->Start(0);

	while (pThis->a.bThreadRunning)
	{
		WaitForSingleObject(pThis->a.hFreeBufferEvent, INFINITE);
		
		/*进入关键段*/
		EnterCriticalSection(&pThis->a.bufferCS);
		int bufIdx = -1;
		for ( pThis->a.currentBufferIndex = 0; pThis->a.currentBufferIndex < 4; pThis->a.currentBufferIndex++)//遍历空闲缓冲区
		{
			if (!pThis->a.bIsBufferInUse[pThis->a.currentBufferIndex])
			{
				bufIdx = pThis->a.currentBufferIndex;
				pThis->a.bIsBufferInUse[pThis->a.currentBufferIndex] = true;//标记为使用中
				break;
			}
		}

		// 检查是否所有缓冲区都已占用
		bool allBusy = true;
		for (int i = 0; i < 4; i++)
		{
			if (!pThis->a.bIsBufferInUse[i])
			{
				allBusy = false;
				break;
			}
		}
		if (allBusy) ResetEvent(pThis->a.hFreeBufferEvent);  // 没有空闲缓冲区了，事件无信号
		LeaveCriticalSection(&pThis->a.bufferCS);
		/*离开关键段*/

		if (bufIdx == -1) continue;

		ret = av_read_frame(pThis->a.fmtCtx, pThis->a.pkt);
		//检测返回值，0表示成功；AVERROR_EOF表示文件读至末尾，此时应发送NULL给解码器
		if (ret == 0 && pThis->a.pkt->stream_index == pThis->a.AudioStreamIndex)
			avcodec_send_packet(pThis->a.codecCtx, pThis->a.pkt);
		else if (ret == AVERROR_EOF)
		{
			avcodec_send_packet(pThis->a.codecCtx, NULL);
			EnterCriticalSection(&pThis->a.bufferCS);
			pThis->a.bIsBufferInUse[bufIdx] = false;
			SetEvent(pThis->a.hFreeBufferEvent);
			LeaveCriticalSection(&pThis->a.bufferCS);
		}
		ret = avcodec_receive_frame(pThis->a.codecCtx, pThis->a.frame);

		if (ret == 0)
			av_packet_unref(pThis->a.pkt);
		else if (ret == AVERROR(EAGAIN))
		{
			EnterCriticalSection(&pThis->a.bufferCS);
			pThis->a.bIsBufferInUse[bufIdx] = false;
			SetEvent(pThis->a.hFreeBufferEvent);
			LeaveCriticalSection(&pThis->a.bufferCS);
			av_packet_unref(pThis->a.pkt);
			continue;
		}
		else if (ret == AVERROR_EOF)
		{
			EnterCriticalSection(&pThis->a.bufferCS);
			pThis->a.bIsBufferInUse[bufIdx] = false;
			SetEvent(pThis->a.hFreeBufferEvent);
			LeaveCriticalSection(&pThis->a.bufferCS);
			av_packet_unref(pThis->a.pkt);
			break;
		}

		int dst_samples = av_rescale_rnd(pThis->a.frame->nb_samples, pThis->a.codecCtx->sample_rate,
		                                 pThis->a.codecCtx->sample_rate,
		                                 AV_ROUND_UP);
		pThis->a.dst_data[0] = pThis->a.pcmBuffers[pThis->a.currentBufferIndex];
		int converted = swr_convert(pThis->a.SwrCtx,pThis->a.dst_data,
		                            dst_samples, (const uint8_t**)pThis->a.frame->extended_data,
		                            pThis->a.frame->nb_samples);
		if (converted < 0)
		{
			// 转换失败，释放缓冲区标记
			EnterCriticalSection(&pThis->a.bufferCS);
			pThis->a.bIsBufferInUse[pThis->a.currentBufferIndex] = FALSE;
			SetEvent(pThis->a.hFreeBufferEvent);
			LeaveCriticalSection(&pThis->a.bufferCS);
			av_frame_unref(pThis->a.frame);
			continue;
		}

		pThis->a.xaBuffer.AudioBytes = converted * pThis->a.wfx.nChannels * pThis->a.wfx.wBitsPerSample/8;
		pThis->a.xaBuffer.pAudioData = pThis->a.pcmBuffers[pThis->a.currentBufferIndex];
		pThis->a.xaBuffer.pContext = (void*)(intptr_t)pThis->a.currentBufferIndex;
		pThis->a.pSourceVoice->SubmitSourceBuffer(&pThis->a.xaBuffer);

		//每提交一次音频数据，就更新一次音频时钟，单位为秒
		BufferDuration = (double)converted / pThis->a.wfx.nSamplesPerSec;
		EnterCriticalSection(&pThis->a.AudioClockCS);
		pThis->a.AudioClock += BufferDuration;
		LeaveCriticalSection(&pThis->a.AudioClockCS);

		av_frame_unref(pThis->a.frame);
	}
	pThis->a.pSourceVoice->Stop(0);
	pThis->a.pSourceVoice->FlushSourceBuffers();
	CoUninitialize();
	return 0;
}


// （1.1更新，方便添加算法）算法实现部分
// 1.1版本作者决定增添注释方便阅读，主要放假比较有空，把注释补一补，
// 其他大部分在没有注释的情况下阅读起来难度不大，因此简略注释或不注释它处
VOID VTWPARAMS::ComputeWindow_EXTEND_METHOD()
{
	// 重置标记数组
	fill(v.processed.begin(), v.processed.end(), false);

	// 根据是否启用缩放选择处理的目标尺寸（缩放后尺寸或显示尺寸）
	int targetW = bUsedResize ? ResizeWidth : Width;
	int targetH = bUsedResize ? ResizeHeight : Height;

	/*——————各变量含义——————*/
	//x,y分别指当前的列与行，也就是这个点的坐标
	//rowOffBase是当前行的起始偏移量，也就是把(0,0)看作data数组的起点，那么(rowOffBase+x)就是当前点在data数组中的偏移量
	//比如说有个点(5,20),那么它的rowOffBase就是20*Width，而它在data数组中的偏移量就是20*Width+5
	//off是当前点在data数组中的偏移量,由第一个for循环的rowOffBase和x计算可以得知含义
	//val是当前点的亮度值，也就是图像中点(x,y)这个像素的Y的值（关于Y是什么，可自行查阅YUV格式）
	//match是一个布尔值，表示当前点是否满足颜色条件（根据用户选择的替代颜色和设定的范围来判断）
	//rx是从当前点(x,y)向右扩展时，连续满足颜色条件的最大列坐标,
	//比如说有个点(5,20)，如果它满足颜色条件，并且(6,20),(7,20)也满足，但(8,20)不满足，那么rx就是7
	//by同理是从当前点(x,y)向下扩展时，连续满足颜色条件的最大行坐标，
	//比如说有个点(5,20)，如果它满足颜色条件，并且(5,21),(5,22)也满足，但(5,23)不满足，那么by就是22
	//noff是一个临时变量，用于在扩展过程中计算下一个点在data数组中的偏移量，以此确定下一个点是否满足颜色条件
	//rowOK是一个布尔值，在向下扩展时用于判断当前行是否完全满足颜色条件，
	//如果有任意一个点不满足，就不继续向下扩展了
	//rectW和rectH分别是当前矩形的宽度和高度，通过计算扩展后的坐标差得到
	for (int y = 0; y < targetH; ++y)
	{
		int rowOffBase = y * targetW;    
		for (int x = 0; x < targetW; ++x)
		{
			size_t off = static_cast<size_t>(rowOffBase + x);
			if (v.processed[off]) continue;

			uint8_t val = v.data[off].y;
			bool match = !bInsteadColor? (val >= MinWhite && val <= MaxWhite): (val >= MinBlack && val <= MaxBlack);
			if (!match) continue;

			// 从 (x, y) 向右扩展，找到连续匹配的最大 rx
			int rx = x;//先令rx为x，再进行下一步计算
			while (rx + 1 < targetW)//在0至targetW-1的范围内扩展，直到遇到不满足条件的点或者越界
			{
				size_t noff = static_cast<size_t>(rowOffBase + rx + 1);
				if (v.processed[noff]) 
					break;
				uint8_t mval = v.data[noff].y;
				bool nmatch = !bInsteadColor? (mval >= MinWhite && mval <= MaxWhite): (mval	 >= MinBlack && mval <= MaxBlack);
				if (!nmatch)
					break;
				++rx;
			}

			// 从 (x, y) 向下扩展，找到连续匹配的最大 by
			int by = y;
			while (by + 1 < targetH)
			{
				bool rowOK = true;// 先假设当前行完全匹配，后续检查如果发现有不匹配的点就置为false
				int nextRowOff = (by + 1) * targetW;
				for (int xx = x; xx <= rx; ++xx)
				{
					size_t noff = static_cast<size_t>(nextRowOff + xx);
					if (v.processed[noff])//如果下一个点已经被处理过了，那么说明它之前已经被某个矩形覆盖了，
					//不应该再继续扩展了，否则会导致重叠的矩形过多，性能下降
					{
						rowOK = false;
						break;
					}
					uint8_t nval = v.data[noff].y;
					bool m = !bInsteadColor? (nval >= MinWhite && nval <= MaxWhite): (nval >= MinBlack && nval <= MaxBlack);
					if (!m) 
					{
						rowOK = false;
						break; 
					}
				}
				if (!rowOK) 
					break;
				++by;
			}

			// 标记矩形区域为已处理
			for (int yy = y; yy <= by; ++yy)
			{
				int rowOff = yy * targetW;
				for (int xx = x; xx <= rx; ++xx)
				{
					v.processed[static_cast<size_t>(rowOff + xx)] = true;
				}
			}

			//计算当前矩形的宽高（注意这里是坐标差+1，因为坐标是从0开始的）
			int rectW = rx - x + 1;
			int rectH = by - y + 1;
			//是否启用缩放
			if (bUsedResize)
				//确定当前矩形是否满足最小尺寸要求
				if (max(rectW, rectH) >= RectMinSizeLong/ResizeRatioX && min(rectW, rectH) >= RectMinSizeShort/ResizeRatioY)
					v.ResizeTemp.push_back({ x, y, x + rectW, y + rectH });//把当前矩形的坐标和尺寸存入ResizeTemp，等会再次进行放大缩放
				else
					if (max(rectW,rectH) >= RectMinSizeLong && min(rectW,rectH) >= RectMinSizeShort)
						RectToWindow(x + startx, y + starty, rectW, rectH);

			x = rx;	// 跳过已处理的部分
		}
	}
}

// 贪心算法,灵感来源：https://github.com/mon/bad_apple_virus
VOID VTWPARAMS::ComputeWindow_GREEDY_METHOD()
{
	// 类似 Python 版：重复寻找当前帧中面积最大的连续匹配矩形，标记访问并记录
	int targetW = bUsedResize ? ResizeWidth : Width;
	int targetH = bUsedResize ? ResizeHeight : Height;
	size_t planeSize = static_cast<size_t>(targetW) * static_cast<size_t>(targetH);

	// 使用一个简单的访问标记数组（0 表示未访问，1 表示已访问或不匹配）
	vector<char> visited(planeSize, 0);

	// 不使用 v.Mergetemp；直接把贪心找到的矩形传给 RectToWindow

	while (true)
	{
		bool foundAny = false;
		int bestArea = 0;
		int bestX = 0, bestY = 0, bestW = 0, bestH = 0;

		// 在所有像素点尝试以该点为左上角的最大矩形
		for (int y = 0; y < targetH; ++y)
		{
			int rowOffBase = y * targetW;
			for (int x = 0; x < targetW; ++x)
			{
				size_t off = static_cast<size_t>(rowOffBase + x);
				if (visited[off]) continue;

				uint8_t val = v.data[off].y;
				bool match = !bInsteadColor ? (val >= MinWhite && val <= MaxWhite) : (val >= MinBlack && val <= MaxBlack);
				if (!match)
				{
					// 非目标像素，标记为已访问以加速后续搜索
					visited[off] = 1;
					continue;
				}

				// 以 (x,y) 为左上角，向下逐行扩展，维护当前行的最小可用宽度 widest
				int widest = targetW - x;
				int bestLocalW = 0, bestLocalH = 0;

				for (int h = 0; h < targetH - y; ++h)
				{
					int curRow = y + h;
					int curW = 0;
					int curRowOff = curRow * targetW + x;

					// 计算当前行从 x 开始连续匹配的宽度（不超过 widest）
					for (int wx = 0; wx < widest; ++wx)
					{
						size_t noff = static_cast<size_t>(curRowOff + wx);
						if (visited[noff])
						{
							break;
						}
						uint8_t nval = v.data[noff].y;
						bool nmatch = !bInsteadColor ? (nval >= MinWhite && nval <= MaxWhite) : (nval >= MinBlack && nval <= MaxBlack);
						if (!nmatch) break;
						++curW;
					}

					if (curW == 0) break; // 当前行无法延展

					if (curW < widest) widest = curW;

					int area = widest * (h + 1);
					if (area > bestLocalW * bestLocalH)
					{
						bestLocalW = widest;
						bestLocalH = h + 1;
					}
				}

				if (bestLocalW > 0 && bestLocalH > 0)
				{
					int area = bestLocalW * bestLocalH;
					if (area > bestArea)
					{
						bestArea = area;
						bestX = x;
						bestY = y;
						bestW = bestLocalW;
						bestH = bestLocalH;
						foundAny = true;
					}
				}
			}
		}

		if (!foundAny || bestArea == 0)
			break;

		// 标记该最大矩形区域为已访问
		for (int yy = bestY; yy < bestY + bestH; ++yy)
		{
			int rowOff = yy * targetW;
			for (int xx = bestX; xx < bestX + bestW; ++xx)
			{
				visited[static_cast<size_t>(rowOff + xx)] = 1;
			}
		}

		// 标记 v.processed 并按最小尺寸条件输出窗口
		for (int yy = bestY; yy < bestY + bestH; ++yy)
		{
			int rowOff = yy * targetW; // 使用 targetW，保持与 visited/v.data 的分配一致
			for (int xx = bestX; xx < bestX + bestW; ++xx)
			{
				size_t idx = static_cast<size_t>(rowOff + xx);
				visited[idx] = 1;
				v.processed[idx] = true;
			}
		}
		if (bUsedResize)
			if (max(bestW, bestH) >= RectMinSizeLong/ResizeRatioX && min(bestW, bestH) >= RectMinSizeShort/ResizeRatioY)
				v.ResizeTemp.push_back({ bestX, bestY, bestX + bestW, bestY + bestH });
				// 仅当满足最小尺寸要求时创建窗口，与扩展法保持一致
			else
				if (max(bestW, bestH) >= RectMinSizeLong && min(bestW, bestH) >= RectMinSizeShort)
					RectToWindow(bestX + startx, bestY + starty, bestW, bestH);
		
	}
}
