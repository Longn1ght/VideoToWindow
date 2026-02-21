#include"VTW.h"
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")

HINSTANCE g_hInstance;

int main()
{
	g_hInstance = GetModuleHandle(nullptr);

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	CHAR szInputBuffer[1024] = { 0 };
	CHAR* token1 = nullptr;
	CHAR* token2 = nullptr;
	CHAR* next_token = nullptr;
	while (TRUE)
	{
		VTWPARAMS v;
	start:

		cout << "视频路径（绝对路径且不含中文）：";
		cin.getline(szInputBuffer, sizeof(szInputBuffer));
		v.SetVideoPath(szInputBuffer);

		ZeroMemory(szInputBuffer, sizeof(szInputBuffer));
		cout << endl;
		cout << "窗口替代颜色（“w”白，“b”黑）:";
		cin >> szInputBuffer;
		if (strcmp(szInputBuffer, "w") == 0)
		{
			v.SetInsteadColor(WHITE);
		}
		else if(strcmp(szInputBuffer, "b") == 0)
		{
			v.SetInsteadColor(BLACK);
		}
		else
		{
			cout << "输入错误，默认使用白色范围" << endl;
			v.SetInsteadColor(WHITE);
		}
		
WhiteRangeInput:		
		ZeroMemory(szInputBuffer, sizeof(szInputBuffer));
		cout << endl;
		cout << "白色范围（输入格式：最小值 最大值）：";
		cin.clear(); // 清除错误标志
		cin.ignore(10000, '\n');
		cin.getline(szInputBuffer, sizeof(szInputBuffer));
		token1 = strtok_s(szInputBuffer, " ", &next_token);
		token2 = strtok_s(nullptr, " ", &next_token);

		if (token1 && token2)
		{
			int minVal = atoi(token1);
			int maxVal = atoi(token2);
			v.SetWhiteRanges(minVal, maxVal);
		}
		else
		{
			cout << "输入错误，输入y以重新输入，输入n以默认值225 255运行" << endl;
			ZeroMemory(szInputBuffer, sizeof(szInputBuffer));
			cin >> szInputBuffer;
			if (strcmp(szInputBuffer, "y") == 0)
			{
				goto WhiteRangeInput;
			}
		}

BlackRangeInput:	
		ZeroMemory(szInputBuffer, sizeof(szInputBuffer));
		next_token = nullptr;
		cout << endl;
		cout << "黑色范围（输入格式：最小值 最大值）：";
		cin.clear();
		cin.getline(szInputBuffer, sizeof(szInputBuffer));
		token1 = strtok_s(szInputBuffer, " ", &next_token);
		token2 = strtok_s(nullptr, " ", &next_token);

		if (token1 && token2)
		{
			int minVal = atoi(token1);
			int maxVal = atoi(token2);
			v.SetBlackRanges(minVal, maxVal);
		}
		else
		{
			cout << "输入错误，输入y以重新输入，输入n以默认值16 36运行" << endl;
			ZeroMemory(szInputBuffer, sizeof(szInputBuffer));
			cin >> szInputBuffer;
			if (strcmp(szInputBuffer, "y") == 0)
			{
				goto BlackRangeInput;
			}
		}
		
SizeInput:	
		ZeroMemory(szInputBuffer, sizeof(szInputBuffer));
		next_token = nullptr;
		cout << endl;
		cout << "宽度与高度（输入格式：宽度 高度）：";
		cin.clear();
		cin.getline(szInputBuffer, sizeof(szInputBuffer));

		token1 = strtok_s(szInputBuffer, " ", &next_token);
		token2 = strtok_s(nullptr, " ", &next_token);

		if (token1 && token2)
		{
			int width = atoi(token1);
			int height = atoi(token2);
			v.SetDisplaySize(width, height);
		}
		else
		{
			cout << "输入错误，输入y以重新输入，输入n以全屏运行" << endl;
			ZeroMemory(szInputBuffer, sizeof(szInputBuffer));
			cin >> szInputBuffer;
			if (strcmp(szInputBuffer, "y") == 0)
			{
				goto SizeInput;
			}
		}


		ZeroMemory(szInputBuffer, sizeof(szInputBuffer));
		cout << endl;
		cout << "最小矩形尺寸（输入0则为默认）：";
		cin >> szInputBuffer;
		{
			int val = atoi(szInputBuffer);
			if (val <= 0)
			{
				// 恢复为构造函数中的默认比例，避免设得过大
				v.SetRectMinSize((v.GetWidth() + v.GetHeight()) / 200);
			}
			else
			{
				v.SetRectMinSize(val);
			}
		}

		ZeroMemory(szInputBuffer, sizeof(szInputBuffer));
		cout << endl;
		cout << "窗口计算算法（输入0为默认，即扩展法）："<<endl;
		cout << "100.扩展法（贡献者Longn1ght）"<<endl;
		cout << "101.占位符（别打，保留，打了没用）"<<endl;
		cin >> szInputBuffer;
		{
			int val = atoi(szInputBuffer);
			
			switch (val)
			{
			case EXTEND_METHOD:
				v.SetComputeMethod(EXTEND_METHOD);
				break;

			case 101:
				// 占位符算法，暂未实现
				cout << "占位符算法尚未实现，默认使用扩展法" << endl;

			case 0:
				v.SetComputeMethod(EXTEND_METHOD);
				break;
			}
		}

		if (!v.AutoUpdate())
		{
			cout << "初始化错误！" << endl;
			goto start;
		}

		system("cls");

		if (v.RequestFrame())
		{
			v.ComputeWindow();
			cout << "读帧成功，回车以继续运行"<<endl;
			getchar();
		}
		else
		{
			cout << "读帧失败，回车以重填参数"<<endl;
			getchar();
			continue;
		}

		v.DisplayWindowFrame();

		while (TRUE)
		{
			if (!(v.RequestFrame()))
			{
				v.StopAudioThread();
				break;
			}
			v.ComputeWindow();
			v.DisplayWindowFrame();
		}

		cout << "是否播放其他视频（y/n）：";
		cin >> szInputBuffer;
		if (strcmp(szInputBuffer, "y") == 0)
		{
			continue;
		}
		else
			break;
	}
	CoUninitialize();
	return 0;
}

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

	//音频结构体初始化
	a.bThreadRunning = FALSE;
	a.hThread = nullptr;
	a.fmtCtx = avformat_alloc_context();
	a.codecCtx = nullptr;
	a.pXAudio2 = nullptr;
	a.wfx.wFormatTag = WAVE_FORMAT_PCM;
	a.wfx.wBitsPerSample = 16;//因为我们要转为PCM格式，所以固定为16位
	InitializeCriticalSection(&a.bufferCS);
	a.hStartEvent = nullptr;
	a.bStartEventTriggered = FALSE;
	a.hFreeBufferEvent = nullptr;
	a.pSourceVoice = nullptr;
	a.pSourceVoice = nullptr;
	a.pkt = av_packet_alloc();
	a.SwrCtx = nullptr;
	a.frame = av_frame_alloc();

	pHeader = nullptr;
    wndnum = 0;      
	curWndIndex = 0;

	WNDCLASSEX wndclassex;
	wndclassex.cbSize = sizeof(WNDCLASSEX);
    wndclassex.style = CS_HREDRAW | CS_VREDRAW;
	wndclassex.lpfnWndProc = VTWProc;
	wndclassex.cbClsExtra = 0;
	wndclassex.cbWndExtra = 0;
	wndclassex.hInstance = g_hInstance;
	wndclassex.hIcon = LoadIcon(g_hInstance,MAKEINTRESOURCE(IDI_VTW));
	wndclassex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclassex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
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

BOOL VTWPARAMS::AutoUpdate()
{
	startx = GetSystemMetrics(SM_CXSCREEN) / 2 - Width / 2;
	starty = GetSystemMetrics(SM_CYSCREEN) / 2 - Height / 2;

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
    v.SwsCtx = sws_getContext(v.codecCtx->width, v.codecCtx->height, v.codecCtx->pix_fmt,
		Width, Height, AV_PIX_FMT_YUV444P, SWS_POINT, NULL,
		NULL, NULL);
	v.data.resize(Width * Height);
	v.processed.resize(Width * Height, false);//见VTW.h中processed的定义
	size_t planeSize = static_cast<size_t>(Width) * static_cast<size_t>(Height);
	v.planeY.resize(planeSize);
	v.planeU.resize(planeSize);
	v.planeV.resize(planeSize);

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

    size_t planeSize = static_cast<size_t>(Width) * static_cast<size_t>(Height);

    // 设置目标指针
    uint8_t* temp[AV_NUM_DATA_POINTERS] = 
	{
        v.planeY.data(),
        v.planeU.data(),
        v.planeV.data(),
        nullptr, nullptr, nullptr, nullptr, nullptr
    };
    int linesize[AV_NUM_DATA_POINTERS] = { 0 };
    av_image_fill_linesizes(linesize, AV_PIX_FMT_YUV444P, Width);

    // 转换图像格式
    int ret = sws_scale(v.SwsCtx, v.frame->data, v.frame->linesize, 0, v.codecCtx->height, temp, linesize);
    if (ret <= 0) 
	{
        return FALSE;
    }

    // 填充 data（确保不会越界）
    for (size_t i = 0, n = planeSize; i < n; ++i) 
	{
        v.data[i].y = v.planeY[i];
        v.data[i].u = v.planeU[i];
        v.data[i].v = v.planeV[i];
    }

	switch (ComputeMethod)
	{
	case COMPUTE_WINDOW_METHOD::EXTEND_METHOD:
		ComputeWindow_EXTEND_METHOD();
		break;

	}
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
		// 复用第 curWndIndex 个窗口（0-based）
		WNDPARAMS* p = pHeader;
		for (int j = 0; j < curWndIndex; ++j) {
			if (p) p = p->next;
		}
		if (p) {
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

VOID VTWPARAMS::DisplayWindowFrame()
{
	cout << "当前窗口数: " << wndnum << endl;//调试用

	if (pHeader == nullptr || wndnum == 0) return;

	// 开始批量移动
	HDWP hdwp = BeginDeferWindowPos(wndnum);
	WNDPARAMS* pBuffer = pHeader;
	while (pBuffer != nullptr)
	{
		if (pBuffer->hwnd) 
		{
			hdwp = DeferWindowPos(hdwp, pBuffer->hwnd, HWND_TOP,
				pBuffer->x, pBuffer->y,
				pBuffer->width, pBuffer->height,
				SWP_NOCOPYBITS | SWP_SHOWWINDOW);
			InvalidateRect(pBuffer->hwnd, NULL, TRUE);
		}
		pBuffer = pBuffer->next;
	}
	// 应用所有移动
	EndDeferWindowPos(hdwp);

	RECT videoRect = { startx, starty, startx + Width, starty + Height };
	RedrawWindow(NULL, &videoRect, NULL, 
		RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

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
			pHeader->hwnd = CreateWindowA("VTW", szFileName, WS_POPUP, x, y, width, height, NULL, NULL, g_hInstance, NULL);
			if (pHeader->hwnd)
			{
				ShowWindow(pHeader->hwnd, SW_SHOWNOACTIVATE);
				UpdateWindow(pHeader->hwnd);
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
		pBuffer->hwnd = CreateWindowA("VTW", szFileName, WS_POPUP, x, y, width, height, NULL, NULL, g_hInstance, NULL);
		if (pBuffer->hwnd)
		{
			ShowWindow(pBuffer->hwnd, SW_SHOWNOACTIVATE);
			UpdateWindow(pBuffer->hwnd);
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
				ShowWindow(pHeader->hwnd, SW_SHOWNOACTIVATE);
				UpdateWindow(pHeader->hwnd);
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
			ShowWindow(pBuffer->hwnd, SW_SHOWNOACTIVATE);
			UpdateWindow(pBuffer->hwnd);
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

//音频线程
DWORD WINAPI VTWPARAMS::AudioThread(LPVOID lpParam)
{

	VTWPARAMS* pThis = (VTWPARAMS*)lpParam;

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	int ret;
	bool first = true;

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
	for (int y = 0; y < Height; ++y)
	{
		int rowOffBase = y * Width;	
		for (int x = 0; x < Width; ++x)
		{
			size_t off = static_cast<size_t>(rowOffBase + x);
			if (v.processed[off]) continue;

			uint8_t val = v.data[off].y;
			bool match = !bInsteadColor? (val >= MinWhite && val <= MaxWhite): (val >= MinBlack && val <= MaxBlack);
			if (!match) continue;

			// 从 (x, y) 向右扩展，找到连续匹配的最大 rx
			int rx = x;//先令rx为x，再进行下一步计算
			while (rx + 1 < Width)//在0至Width-1的范围内扩展，直到遇到不满足条件的点或者越界
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
			while (by + 1 < Height)
			{
				bool rowOK = true;// 先假设当前行完全匹配，后续检查如果发现有不匹配的点就置为false
				int nextRowOff = (by + 1) * Width;
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
				int rowOff = yy * Width;
				for (int xx = x; xx <= rx; ++xx)
				{
					v.processed[static_cast<size_t>(rowOff + xx)] = true;
				}
			}

			//计算当前矩形的宽高（注意这里是坐标差+1，因为坐标是从0开始的）
			int rectW = rx - x + 1;
			int rectH = by - y + 1;
			//确定当前矩形是否满足最小尺寸要求
			if (max(rectW,rectH) >= RectMinSizeLong && min(rectW,rectH) >= RectMinSizeShort)
			{
				RectToWindow(x + startx, y + starty, rectW, rectH);
			}

			x = rx;	// 跳过已处理的部分
		}
	}
}
