#include "VTW.h"
#ifdef _OPENMP
#include <omp.h>
#endif
HINSTANCE g_hInstance;
HANDLE IsPlayingEvent;
BOOL bRunning;

INT_PTR VTWControlProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static VTWPARAMS* pPlayer = nullptr;  // 播放器对象指针
	wchar_t buffer[256];
	static HANDLE hPlayer;
	static BOOL StartAndPauseStatus = FALSE;//开始/暂停状态，FALSE为未开始或已暂停，TRUE为正在播放

	switch (uMsg)
	{
	case WM_CREATE:
	{
		// 控件创建
		WindowInit(hwnd);
		IsPlayingEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
		return 0;
	}
	//处理拖进窗口的文件
	case WM_DROPFILES:
	{
		HDROP hDrop = (HDROP)wParam;
		UINT len = DragQueryFileA(hDrop, 0, NULL, 0) + 1;
		LPSTR lpFileName = new char[len];
		DragQueryFileA(hDrop, 0, lpFileName, len);
		SetDlgItemTextA(hwnd, IDE_PATH, lpFileName);
		delete[]lpFileName;
		DragFinish(hDrop);
		break;
	}
	case WM_COMMAND:
	{
		int id = LOWORD(wParam);
		switch (id)
		{
		case IDB_PATHCHOOSE:
		{
			// 打开文件选择对话框，将路径设置到编辑框 IDE_PATH
			OPENFILENAMEA ofn = { sizeof(ofn) };
			CHAR path[MAX_PATH] = { 0 };
			ofn.hwndOwner = hwnd;
			ofn.lpstrFilter = "Video Files\0*.mp4;*.avi;*.mkv\0All Files\0*.*\0";
			ofn.lpstrFile = path;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
			if (GetOpenFileNameA(&ofn))
			{
				SetDlgItemTextA(hwnd, IDE_PATH, path);
			}
			return 0;
		}

		case IDB_INSTEADCOLORWHITE:
		case IDB_INSTEADCOLORBLACK:
			// 处理单选：确保只有一个被选中
			CheckRadioButton(hwnd, IDB_INSTEADCOLORWHITE, IDB_INSTEADCOLORBLACK, id);
			return 0;

		case IDB_USEDRESIZE:
			// 缩放复选框状态改变，可启用/禁用缩放尺寸组合框
		{
			BOOL enabled = IsDlgButtonChecked(hwnd, IDB_USEDRESIZE) == BST_CHECKED;
			EnableWindow(GetDlgItem(hwnd, IDCB_RESIZEWIDTH), enabled);
			EnableWindow(GetDlgItem(hwnd, IDCB_RESIZEHEIGHT), enabled);
		}
		return 0;

		case IDB_STARTPAUSE:
		{
			if (!StartAndPauseStatus)
			{
				if (!pPlayer)
				{
					pPlayer = new VTWPARAMS();

					// 从控件获取值并设置到 pPlayer（示例）
					CHAR path[MAX_PATH];
					GetDlgItemTextA(hwnd, IDE_PATH, path, MAX_PATH);
					pPlayer->SetVideoPath(path);

					// GPU 选项
					BOOL useGPU = IsDlgButtonChecked(hwnd, IDB_USEGPU) == BST_CHECKED;
					pPlayer->SetUseGPU(useGPU);

					// 替代颜色
					if (IsDlgButtonChecked(hwnd, IDB_INSTEADCOLORWHITE) == BST_CHECKED)
						pPlayer->SetInsteadColor(WHITE);
					else
						pPlayer->SetInsteadColor(BLACK);

					// 范围
					if (IsDlgButtonChecked(hwnd, IDB_INSTEADCOLORWHITE) == BST_CHECKED)
					{
						GetDlgItemText(hwnd, IDCB_RANGEMIN, buffer, 256);
						int minWhite = _wtoi(buffer);
						ZeroMemory(buffer, sizeof(buffer));
						GetDlgItemText(hwnd, IDCB_RANGEMAX, buffer, 256);
						int maxWhite = _wtoi(buffer);
						ZeroMemory(buffer, sizeof(buffer));
						pPlayer->SetWhiteRanges(minWhite, maxWhite);
					}
					else
					{
						GetDlgItemText(hwnd, IDCB_RANGEMIN, buffer, 256);
						int minBlack = _wtoi(buffer);
						ZeroMemory(buffer, sizeof(buffer));
						GetDlgItemText(hwnd, IDCB_RANGEMAX, buffer, 256);
						int maxBlack = _wtoi(buffer);
						ZeroMemory(buffer, sizeof(buffer));
						pPlayer->SetBlackRanges(minBlack, maxBlack);
					}

					// 显示尺寸
					GetDlgItemText(hwnd, IDCB_DISPLAYWIDTH, buffer, 256);
					int displayWidth = _wtoi(buffer);
					GetDlgItemText(hwnd, IDCB_DISPLAYHEIGHT, buffer, 256);
					int displayHeight = _wtoi(buffer);
					pPlayer->SetDisplaySize(displayWidth, displayHeight);

					// 是否启用缩放
					BOOL resize = IsDlgButtonChecked(hwnd, IDB_USEDRESIZE) == BST_CHECKED;
					pPlayer->SetUsedResize(resize);
					if (resize)
					{
						GetDlgItemText(hwnd, IDCB_RESIZEWIDTH, buffer, 256);
						int rw = _wtoi(buffer);
						ZeroMemory(buffer, sizeof(buffer));
						GetDlgItemText(hwnd, IDCB_RESIZEHEIGHT, buffer, 256);
						int rh = _wtoi(buffer);
						ZeroMemory(buffer, sizeof(buffer));
						pPlayer->SetResizeSize(rw, rh);
					}

					// 最小矩形尺寸
					GetDlgItemText(hwnd, IDCB_RECTMINSIZE, buffer, 256);
					int RectMinSize = _wtoi(buffer);
					ZeroMemory(buffer, sizeof(buffer));
					pPlayer->SetRectMinSize(RectMinSize);

					// 算法选择
					int curSel = SendMessage(GetDlgItem(hwnd, IDCB_COMPUTEMETHOD), CB_GETCURSEL, 0, 0);
					if (curSel != CB_ERR)
					{
						int method = (int)SendMessage(GetDlgItem(hwnd, IDCB_COMPUTEMETHOD), CB_GETITEMDATA, curSel, 0);
						pPlayer->SetComputeMethod(static_cast<COMPUTE_WINDOW_METHOD>(method));
					}

					// 初始化
					if (!pPlayer->AutoUpdate())
					{
						MessageBox(hwnd, L"初始化失败", L"错误", MB_OK);
						delete pPlayer;
						pPlayer = nullptr;
						return 0;
					}

					// 更新 GPU 信息显示
					HWND hGpuInfo = GetDlgItem(hwnd, IDT_GPUINFO);
					if (hGpuInfo)
					{
						const char* info = pPlayer->GetHWDeviceInfo();
						SetWindowTextA(hGpuInfo, info);
					}

                    bRunning = TRUE;
					SetEvent(IsPlayingEvent);
					hPlayer = CreateThread(NULL, 0, &VTWPARAMS::PlayThread, pPlayer, 0, NULL);
					// Ensure audio thread is running and not paused
					pPlayer->ContinuePauseAudioThread(TRUE);
					SendMessage(GetDlgItem(hwnd, IDB_STARTPAUSE), WM_SETTEXT, 0, (LPARAM)TEXT("暂停"));
					StartAndPauseStatus = TRUE;
				}
				else if (pPlayer && !pPlayer->IsPlayEnded())
				{
					SetEvent(IsPlayingEvent);
					pPlayer->ContinuePauseAudioThread(TRUE);
					SendMessage(GetDlgItem(hwnd, IDB_STARTPAUSE), WM_SETTEXT, 0, (LPARAM)TEXT("暂停"));
					StartAndPauseStatus = TRUE;
					return 0;
				}
			}
			else
			{
				// 暂停
				ResetEvent(IsPlayingEvent);
				if (pPlayer) pPlayer->ContinuePauseAudioThread(FALSE);
				SendMessage(GetDlgItem(hwnd, IDB_STARTPAUSE), WM_SETTEXT, 0, (LPARAM)TEXT("继续"));
				StartAndPauseStatus = FALSE;
			}
		}
		break;

		case IDB_STOP:
		{
           SetEvent(IsPlayingEvent);
			bRunning = FALSE;
			// Stop audio thread and then wait for play thread to exit before deleting player
			if (pPlayer)
			{
				pPlayer->ContinuePauseAudioThread(FALSE);
				pPlayer->StopAudioThread();
			}
			if (hPlayer)
			{
				WaitForSingleObject(hPlayer, INFINITE);
				CloseHandle(hPlayer);
				hPlayer = NULL;
			}
			if (pPlayer)
			{
				delete pPlayer;
				pPlayer = nullptr;
			}
			SendMessage(GetDlgItem(hwnd, IDB_STARTPAUSE), WM_SETTEXT, 0, (LPARAM)TEXT("开始"));
			StartAndPauseStatus = FALSE;
			return 0;
		}
		}
		return 0;
	}

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
	{
		if (pPlayer)
		{
			pPlayer->StopAudioThread();
			delete pPlayer;
		}
        if (IsPlayingEvent) CloseHandle(IsPlayingEvent);
		if (hPlayer) CloseHandle(hPlayer);
		PostQuitMessage(0);
		return 0;
	}

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
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
	v.VideoPTS = 0.0;

	// hw device ctx init
	v.hwDeviceCtx = nullptr;

	// 默认不使用 GPU
	bUseGPU = FALSE;

	// D3D11 members
	d3dDevice = nullptr;
	d3dContext = nullptr;
	d3dCS = nullptr;

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
	a.hPauseEvent = nullptr;
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
	wndclassex.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_VTW));
	wndclassex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclassex.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
	wndclassex.lpszMenuName = NULL;
	wndclassex.lpszClassName = L"VTW";
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

	// 清理 hw device context（如果存在）
	if (v.hwDeviceCtx)
	{
		av_buffer_unref(&v.hwDeviceCtx);
		v.hwDeviceCtx = nullptr;
	}

	// Release D3D resources
	ReleaseD3D();

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

VOID VTWPARAMS::SetUseGPU(BOOL bUse)
{
	bUseGPU = bUse;
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
	startx = (GetSystemMetrics(SM_CXSCREEN) - Width) / 2;
	starty = (GetSystemMetrics(SM_CYSCREEN) - Height) / 2;

	avformat_open_input(&v.fmtCtx, szFileName, NULL, NULL);
	avformat_find_stream_info(v.fmtCtx, NULL);
	for (int i = 0; i < v.fmtCtx->nb_streams; i++)
	{
		const AVCodec* vcodec = avcodec_find_decoder(v.fmtCtx->streams[i]->codecpar->codec_id);
		if (vcodec->type == AVMEDIA_TYPE_VIDEO)
		{
			v.VideoStreamIndex = i;
			v.codecCtx = avcodec_alloc_context3(vcodec);
			avcodec_parameters_to_context(v.codecCtx, v.fmtCtx->streams[i]->codecpar);
            // 如果启用 GPU，尝试创建 hw device 并绑定到 codecCtx（实验性）
			if (bUseGPU)
			{
				const char* try_names[] = { "d3d11va", "dxva2", "cuda", NULL };
				for (const char** p = try_names; *p != NULL; ++p)
				{
					AVHWDeviceType type = av_hwdevice_find_type_by_name(*p);
					if (type == AV_HWDEVICE_TYPE_NONE)
						continue;
					if (av_hwdevice_ctx_create(&v.hwDeviceCtx, type, NULL, NULL, 0) == 0)
					{
						v.codecCtx->hw_device_ctx = av_buffer_ref(v.hwDeviceCtx);
						// 保存设备名（简短）
						_snprintf_s(v.hwDeviceName, _countof(v.hwDeviceName), _TRUNCATE, "%s", *p);
						break;
					}
					else
					{
						if (v.hwDeviceCtx) { av_buffer_unref(&v.hwDeviceCtx); v.hwDeviceCtx = NULL; }
					}
				}
			}
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
		v.ResizeTemp.reserve(static_cast<size_t>(max(1024, (ResizeWidth * ResizeHeight) / 100)));
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
	swr_alloc_set_opts2(&a.SwrCtx, &a.codecCtx->ch_layout, AV_SAMPLE_FMT_S16,
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
		a.pcmBuffers[i] = static_cast<uint8_t*>(VirtualAlloc(NULL, bufferSize,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));//预分配4个PCM缓冲区，避免在循环中频繁分配内存，
	//分配的内存大小为通道数 × 采样个数 × 采样位数 / 8
	a.hStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	a.hPauseEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	a.hFreeBufferEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	a.pCallback = new VoiceCallback(this);
	HRESULT hr = XAudio2Create(&a.pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr))
		return FALSE;
	hr = a.pXAudio2->CreateMasteringVoice(&a.pMasteringVoice);
	if (FAILED(hr))
		return FALSE;
	hr = a.pXAudio2->CreateSourceVoice(&a.pSourceVoice, (WAVEFORMATEX*)&a.wfx, 0,
		XAUDIO2_DEFAULT_FREQ_RATIO, a.pCallback, NULL, NULL);
	if (FAILED(hr))
		return FALSE;
	a.hThread = CreateThread(NULL, 0, AudioThread, this, 0, NULL);
	if (!a.hThread)
		return FALSE;
	a.bThreadRunning = TRUE;

	return TRUE;
}

const char* VTWPARAMS::GetHWDeviceInfo()
{
	if (v.hwDeviceCtx && v.hwDeviceName[0])
		return v.hwDeviceName;
	return "未使用 GPU 或未检测到设备";
}

BOOL VTWPARAMS::InitD3DIfNeeded()
{
	if (d3dDevice && d3dContext && d3dCS) return TRUE;
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION,
		&d3dDevice, NULL, &d3dContext);
	if (FAILED(hr)) return FALSE;

	const char* csSource =
		"RWTexture2D<uint> outTex : register(u0);\n"
		"Texture2D<uint> inTex : register(t0);\n"
		"cbuffer Params : register(b0) { uint width; uint height; uint minW; uint maxW; uint minB; uint maxB; uint instead; uint pad; };\n"
		"[numthreads(16,16,1)] void main(uint3 DTid : SV_DispatchThreadID) {\n"
		"  if (DTid.x >= width || DTid.y >= height) return;\n"
		"  uint val = inTex.Load(int3(DTid.xy,0)) & 0xFF;\n"
		"  bool match = (instead == 0) ? (val >= minW && val <= maxW) : (val >= minB && val <= maxB);\n"
		"  outTex[DTid.xy] = match ? 1u : 0u;\n"
		"}\n";

	ID3DBlob* blob = nullptr; ID3DBlob* err = nullptr;
	HRESULT hr2 = D3DCompile(csSource, strlen(csSource), NULL, NULL, NULL, "main", "cs_5_0", 0, 0, &blob, &err);
	if (FAILED(hr2) || !blob) { if (err) err->Release(); return FALSE; }
	hr2 = d3dDevice->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &d3dCS);
	blob->Release();
	if (FAILED(hr2)) { if (d3dCS) { d3dCS->Release(); d3dCS = nullptr; } return FALSE; }
	return TRUE;
}

VOID VTWPARAMS::ReleaseD3D()
{
	if (d3dCS) { d3dCS->Release(); d3dCS = nullptr; }
	if (d3dContext) { d3dContext->Release(); d3dContext = nullptr; }
	if (d3dDevice) { d3dDevice->Release(); d3dDevice = nullptr; }
}

BOOL VTWPARAMS::RequestFrame()
{
	while (TRUE)
	{
		int ret = av_read_frame(v.fmtCtx, v.pkt);
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
            // 如果帧是 HW 上下文，需要把它转回软件帧
			if (v.frame->hw_frames_ctx)
			{
				// 转换到软件可访问的帧
				AVFrame* swFrame = av_frame_alloc();
				if (av_hwframe_transfer_data(swFrame, v.frame, 0) >= 0)
				{
					av_frame_unref(v.frame);
					// 将 swFrame 的数据复制到 v.frame
					av_frame_move_ref(v.frame, swFrame);
				}
				av_frame_free(&swFrame);
			}
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

	case COMPUTE_WINDOW_METHOD::EXPERIMENTAL_METHOD:
		ComputeWindow_EXPERIMENTAL_METHOD();
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
	for (size_t i = 0; i < v.ResizeTemp.size(); ++i)
	{
		int x = static_cast<int>(round(v.ResizeTemp[i].left * ResizeRatioX));
		int y = static_cast<int>(round(v.ResizeTemp[i].top * ResizeRatioY));
		int width = static_cast<int>(round((v.ResizeTemp[i].right - v.ResizeTemp[i].left) * ResizeRatioX));
		int height = static_cast<int>(round((v.ResizeTemp[i].bottom - v.ResizeTemp[i].top) * ResizeRatioY));
		RectToWindow(startx + x, starty + y, width, height);
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
			pHeader->hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, "VTW", szFileName,
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
		pBuffer->hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, "VTW", szFileName,
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

VOID VTWPARAMS::ContinuePauseAudioThread(BOOL bAction)
{
	if (bAction)
	{
      SetEvent(a.hPauseEvent);
		if (a.pSourceVoice) a.pSourceVoice->Start(0);
	}
	else
	{
      ResetEvent(a.hPauseEvent);
		if (a.pSourceVoice) a.pSourceVoice->Stop(0);
	}

};

VOID VTWPARAMS::StopAudioThread()
{
	a.bThreadRunning = FALSE;
	SetEvent(a.hPauseEvent);
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
	VTWPARAMS* pThis = static_cast<VTWPARAMS*>(lpParam);
	HANDLE events[2] = { pThis->a.hFreeBufferEvent, pThis->a.hPauseEvent };

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	bool first = true;

	WaitForSingleObject(pThis->a.hStartEvent, INFINITE);
	pThis->a.pSourceVoice->Start(0);

    while (pThis->a.bThreadRunning)
	{
		DWORD dw = WaitForMultipleObjects(2, events, TRUE, INFINITE);
		if (dw == WAIT_OBJECT_0)
		{
			/*进入关键段*/
			EnterCriticalSection(&pThis->a.bufferCS);
			int bufIdx = -1;
            for (int i = 0; i < 4; ++i) // 遍历空闲缓冲区
			{
				if (!pThis->a.bIsBufferInUse[i])
				{
					bufIdx = i;
					pThis->a.bIsBufferInUse[i] = true; // 标记为使用中
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

			int ret = av_read_frame(pThis->a.fmtCtx, pThis->a.pkt);
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
			pThis->a.dst_data[0] = pThis->a.pcmBuffers[bufIdx];
			int converted = swr_convert(pThis->a.SwrCtx, pThis->a.dst_data,
				dst_samples, (const uint8_t**)pThis->a.frame->extended_data,
				pThis->a.frame->nb_samples);
			if (converted < 0)
			{
				// 转换失败，释放缓冲区标记
                EnterCriticalSection(&pThis->a.bufferCS);
				pThis->a.bIsBufferInUse[bufIdx] = FALSE;
				SetEvent(pThis->a.hFreeBufferEvent);
				LeaveCriticalSection(&pThis->a.bufferCS);
				av_frame_unref(pThis->a.frame);
				continue;
			}

            pThis->a.xaBuffer.AudioBytes = converted * pThis->a.wfx.nChannels * pThis->a.wfx.wBitsPerSample / 8;
			pThis->a.xaBuffer.pAudioData = pThis->a.pcmBuffers[bufIdx];
			pThis->a.xaBuffer.pContext = (void*)static_cast<intptr_t>(bufIdx);
			pThis->a.pSourceVoice->SubmitSourceBuffer(&pThis->a.xaBuffer);

			//每提交一次音频数据，就更新一次音频时钟，单位为秒
			double BufferDuration = static_cast<double>(converted) / pThis->a.wfx.nSamplesPerSec;
			EnterCriticalSection(&pThis->a.AudioClockCS);
			pThis->a.AudioClock += BufferDuration;
			LeaveCriticalSection(&pThis->a.AudioClockCS);

			av_frame_unref(pThis->a.frame);
		}
		else if (dw==WAIT_OBJECT_0+1)
		{
			break;
		}
		else if (dw==WAIT_FAILED)
			break;
	}
	pThis->a.pSourceVoice->Stop(0);
	pThis->a.pSourceVoice->FlushSourceBuffers();
	CoUninitialize();
	return 0;
};

VOID WindowInit(HWND hwnd)
{
	HDC hdc = GetDC(hwnd);
	int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
	int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(hwnd, hdc);
	float scaleX = dpiX / 96.0f;
	float scaleY = dpiY / 96.0f;

	// 第一行：视频路径
	CreateWindowEx(0, TEXT("STATIC"), TEXT("视频路径（绝对路径且不含中文）："),
		WS_CHILD | WS_VISIBLE,
		SCALE_X(20), SCALE_Y(20), SCALE_X(200), SCALE_Y(25),
		hwnd, NULL, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("EDIT"), TEXT(""),
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
		SCALE_X(230), SCALE_Y(20), SCALE_X(450), SCALE_Y(25),
		hwnd, (HMENU)IDE_PATH, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("BUTTON"), TEXT("..."),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		SCALE_X(700), SCALE_Y(20), SCALE_X(25), SCALE_Y(25),
		hwnd, (HMENU)IDB_PATHCHOOSE, g_hInstance, NULL);

	// 第二行：窗口替代颜色组框
	CreateWindowEx(0, TEXT("BUTTON"), TEXT("窗口替代颜色"),
		WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		SCALE_X(20), SCALE_Y(60), SCALE_X(350), SCALE_Y(70),
		hwnd, NULL, g_hInstance, NULL);

	// 内部：白、黑复选框，最小/最大范围组合框
	CreateWindowEx(0, TEXT("BUTTON"), TEXT("白"),
		WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
		SCALE_X(30), SCALE_Y(80), SCALE_X(40), SCALE_Y(20),
		hwnd, (HMENU)IDB_INSTEADCOLORWHITE, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("BUTTON"), TEXT("黑"),
		WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
		SCALE_X(80), SCALE_Y(80), SCALE_X(40), SCALE_Y(20),
		hwnd, (HMENU)IDB_INSTEADCOLORBLACK, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("COMBOBOX"), TEXT(""),
		WS_CHILD | WS_VISIBLE | CBS_DROPDOWN,
		SCALE_X(130), SCALE_Y(78), SCALE_X(70), SCALE_Y(200),
		hwnd, (HMENU)IDCB_RANGEMIN, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("COMBOBOX"), TEXT(""),
		WS_CHILD | WS_VISIBLE | CBS_DROPDOWN,
		SCALE_X(210), SCALE_Y(78), SCALE_X(70), SCALE_Y(200),
		hwnd, (HMENU)IDCB_RANGEMAX, g_hInstance, NULL);

	// 第二行右侧：显示尺寸组框
	CreateWindowEx(0, TEXT("BUTTON"), TEXT("显示尺寸"),
		WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		SCALE_X(400), SCALE_Y(60), SCALE_X(250), SCALE_Y(70),
		hwnd, NULL, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("COMBOBOX"), TEXT(""),
		WS_CHILD | WS_VISIBLE | CBS_DROPDOWN,
		SCALE_X(410), SCALE_Y(80), SCALE_X(100), SCALE_Y(200),
		hwnd, (HMENU)IDCB_DISPLAYWIDTH, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("COMBOBOX"), TEXT(""),
		WS_CHILD | WS_VISIBLE | CBS_DROPDOWN,
		SCALE_X(530), SCALE_Y(80), SCALE_X(100), SCALE_Y(200),
		hwnd, (HMENU)IDCB_DISPLAYHEIGHT, g_hInstance, NULL);

	// 第三行：缩放设置组框
	CreateWindowEx(0, TEXT("BUTTON"), TEXT("缩放设置"),
		WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		SCALE_X(20), SCALE_Y(140), SCALE_X(450), SCALE_Y(70),
		hwnd, NULL, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("BUTTON"), TEXT("启用缩放"),
		WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		SCALE_X(30), SCALE_Y(160), SCALE_X(120), SCALE_Y(20),
		hwnd, (HMENU)IDB_USEDRESIZE, g_hInstance, NULL);

	// GPU 实验复选框（放在更下方以避免与缩放控件重叠）
	CreateWindowEx(0, TEXT("BUTTON"), TEXT("启用实验性 GPU 解码"),
		WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		SCALE_X(30), SCALE_Y(190), SCALE_X(340), SCALE_Y(20),
		hwnd, (HMENU)IDB_USEGPU, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("COMBOBOX"), TEXT(""),
		WS_CHILD | WS_VISIBLE | WS_DISABLED | CBS_DROPDOWN,
		SCALE_X(170), SCALE_Y(160), SCALE_X(100), SCALE_Y(200),
		hwnd, (HMENU)IDCB_RESIZEWIDTH, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("COMBOBOX"), TEXT(""),
		WS_CHILD | WS_VISIBLE | WS_DISABLED | CBS_DROPDOWN,
		SCALE_X(290), SCALE_Y(160), SCALE_X(100), SCALE_Y(200),
		hwnd, (HMENU)IDCB_RESIZEHEIGHT, g_hInstance, NULL);

	// GPU 信息静态控件
	CreateWindowEx(0, TEXT("STATIC"), TEXT("GPU: 未检测"),
		WS_CHILD | WS_VISIBLE,
		SCALE_X(400), SCALE_Y(190), SCALE_X(420), SCALE_Y(20),
		hwnd, (HMENU)IDT_GPUINFO, g_hInstance, NULL);

	// 第四行：矩形参数组框
	CreateWindowEx(0, TEXT("BUTTON"), TEXT("矩形参数"),
		WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		SCALE_X(20), SCALE_Y(220), SCALE_X(450), SCALE_Y(70),
		hwnd, NULL, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("COMBOBOX"), TEXT(""),
		WS_CHILD | WS_VISIBLE | CBS_DROPDOWN,
		SCALE_X(30), SCALE_Y(240), SCALE_X(150), SCALE_Y(200),
		hwnd, (HMENU)IDCB_RECTMINSIZE, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("COMBOBOX"), TEXT(""),
		WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
		SCALE_X(210), SCALE_Y(240), SCALE_X(150), SCALE_Y(200),
		hwnd, (HMENU)IDCB_COMPUTEMETHOD, g_hInstance, NULL);

	// 第四行：按钮
	CreateWindowEx(0, TEXT("BUTTON"), TEXT("开始"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		SCALE_X(500), SCALE_Y(230), SCALE_X(100), SCALE_Y(30),
		hwnd, (HMENU)IDB_STARTPAUSE, g_hInstance, NULL);

	CreateWindowEx(0, TEXT("BUTTON"), TEXT("停止"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		SCALE_X(620), SCALE_Y(230), SCALE_X(100), SCALE_Y(30),
		hwnd, (HMENU)IDB_STOP, g_hInstance, NULL);

	//获取ComboBox控件句柄
	HWND hRangeMin = GetDlgItem(hwnd, IDCB_RANGEMIN);
	HWND hRangeMax = GetDlgItem(hwnd, IDCB_RANGEMAX);
	HWND hDisplayShort = GetDlgItem(hwnd, IDCB_DISPLAYWIDTH);
	HWND hDisplayHeight = GetDlgItem(hwnd, IDCB_DISPLAYHEIGHT);
	HWND hResizeShort = GetDlgItem(hwnd, IDCB_RESIZEWIDTH);
	HWND hResizeHeight = GetDlgItem(hwnd, IDCB_RESIZEHEIGHT);
	HWND hRectMinSize = GetDlgItem(hwnd, IDCB_RECTMINSIZE);
	HWND hComputeMethod = GetDlgItem(hwnd, IDCB_COMPUTEMETHOD);

	SendMessage(hRangeMin, CB_ADDSTRING, 0, (LPARAM)TEXT("225"));
	SendMessage(hRangeMin, CB_ADDSTRING, 0, (LPARAM)TEXT("129"));
	SendMessage(hRangeMin, CB_ADDSTRING, 0, (LPARAM)TEXT("16"));
	SendMessage(hRangeMin, CB_ADDSTRING, 0, (LPARAM)TEXT("0"));
	SendMessage(hRangeMax, CB_ADDSTRING, 0, (LPARAM)TEXT("255"));
	SendMessage(hRangeMax, CB_ADDSTRING, 0, (LPARAM)TEXT("255"));
	SendMessage(hRangeMax, CB_ADDSTRING, 0, (LPARAM)TEXT("46"));
	SendMessage(hRangeMax, CB_ADDSTRING, 0, (LPARAM)TEXT("127"));

	const int displayWidthPresets[] = { 640, 800, 1024, 1280, 1366, 1600, 1920 };
	for (int val : displayWidthPresets)
	{
		wchar_t text[16];
		wsprintf(text, TEXT("%d"), val);
		SendMessage(hDisplayShort, CB_ADDSTRING, 0, (LPARAM)text);
	}

	const int displayHeightPresets[] = { 480, 600, 768, 720, 768, 900, 1080 };
	for (int val : displayHeightPresets)
	{
		wchar_t text[16];
		wsprintf(text, TEXT("%d"), val);
		SendMessage(hDisplayHeight, CB_ADDSTRING, 0, (LPARAM)text);
	}

	const int resizeWidthPresets[] = { 64, 80, 96, 112, 128, 160, 320 };
	for (int val : resizeWidthPresets)
	{
		wchar_t text[16];
		wsprintf(text, TEXT("%d"), val);
		SendMessage(hResizeShort, CB_ADDSTRING, 0, (LPARAM)text);
	}

	const int resizeHeightPresets[] = { 36, 45, 54, 63, 72, 90, 180 };
	for (int val : resizeHeightPresets)
	{
		wchar_t text[16];
		wsprintf(text, TEXT("%d"), val);
		SendMessage(hResizeHeight, CB_ADDSTRING, 0, (LPARAM)text);
	}

	const int minSizePresets[] = { 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32 };
	for (int val : minSizePresets)
	{
		wchar_t text[16];
		wsprintf(text, TEXT("%d"), val);
		SendMessage(hRectMinSize, CB_ADDSTRING, 0, (LPARAM)text);
	}

	int idx = SendMessage(hComputeMethod, CB_ADDSTRING, 0, (LPARAM)TEXT("扩展法 (100)"));
	SendMessage(hComputeMethod, CB_SETITEMDATA, idx, (LPARAM)EXTEND_METHOD);
	idx = SendMessage(hComputeMethod, CB_ADDSTRING, 0, (LPARAM)TEXT("贪心算法 (101) 灵感来源：https://github.com/mon/bad_apple_virus,由AI转写"));
	SendMessage(hComputeMethod, CB_SETITEMDATA, idx, (LPARAM)GREEDY_METHOD);
	idx = SendMessage(hComputeMethod, CB_ADDSTRING, 0, (LPARAM)TEXT("实验算法 (102) 实验性"));
	SendMessage(hComputeMethod, CB_SETITEMDATA, idx, (LPARAM)EXPERIMENTAL_METHOD);
	SendMessage(hComputeMethod, CB_SETCURSEL, 0, 0);

	SendMessage(hRangeMin, CB_SETCURSEL, 0, 0);
	SendMessage(hRangeMax, CB_SETCURSEL, 0, 0);
	SendMessage(hDisplayShort, CB_SETCURSEL, 0, 0);
	SendMessage(hDisplayHeight, CB_SETCURSEL, 0, 0);
	SendMessage(hResizeShort, CB_SETCURSEL, 0, 0);
	SendMessage(hResizeHeight, CB_SETCURSEL, 0, 0);
	SendMessage(hRectMinSize, CB_SETCURSEL, 0, 0);
	SendMessage(hComputeMethod, CB_SETCURSEL, 0, 0);

};

DWORD WINAPI VTWPARAMS::PlayThread(LPVOID lpParam)
{
	VTWPARAMS* v = static_cast<VTWPARAMS*>(lpParam);
	double VideoPTS;
	double AudioClock;
	double diff;
	MSG msg;
	v->v.IsPlayEnded = FALSE;
	while (bRunning)
	{
		if (WaitForSingleObject(IsPlayingEvent, INFINITE) == WAIT_OBJECT_0)
		{
			if (!(v->RequestFrame()))
			{
				v->StopAudioThread();
				break;
			}
			VideoPTS = v->GetVideoPTS();
			AudioClock = v->GetAudioClock();
			diff = VideoPTS - AudioClock;
			if (diff < -0.100)//丢帧
			{
				continue;
			}
			if (diff > 0.025)//等待音频
			{
				double WaitTime = min(diff, 1.0 / v->GetVideoFPS());
				Sleep((DWORD)(WaitTime * 1000));
			}

			v->ComputeWindow();
			v->DisplayWindowFrame();

			// 处理窗口消息，避免窗口被系统标记为未响应
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
	v->v.IsPlayEnded = TRUE;
	return 0;
}

BOOL VTWPARAMS::IsPlayEnded()
{
	return v.IsPlayEnded;
}