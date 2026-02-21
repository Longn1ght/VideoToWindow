#pragma once
#include <algorithm>
#include <iostream>
#include <strsafe.h>
#include <TCHAR.h>
#include <vector>
#include <Windows.h>
#include <xaudio2.h>
#include <combaseapi.h>
#include "resource.h"
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>   
#include<libswresample/swresample.h>
}

#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"swscale.lib")

#pragma comment(lib, "ole32.lib") 
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "swresample.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WHITE 0
#define BLACK 1

using namespace std;

struct WNDPARAMS
{
	HWND hwnd;
	int x;
	int y;
	int width;
	int height;
	WNDPARAMS* next = nullptr;
};

struct YUV
{
	uint8_t y;
	uint8_t u;
	uint8_t v;

};

class VTWPARAMS;
class VoiceCallback;

//视频相关参数结构体
struct VIDEOPARAMS
{

	AVFormatContext* fmtCtx;
	AVCodecContext* codecCtx;
	int VideoStreamIndex;
	AVFrame* frame;
	SwsContext* SwsCtx;
	AVPacket* pkt;
	vector<YUV>data;
	vector<bool> processed;	// 标记每个像素是否已被处理过，避免重复处理(1.1更新，尽管一般说来YUV格式中
							// Y不为0至16的值，但为防止特殊情况，还是在损失性能的情况下添加了这个数组，
							// 因为在1.0中，我们是通过直接向data写0来表示已处理的，
							// 但是该程序性能需求其实没那么大，这是在没有写过特意优化的情况下,)(Version1.2:现在不是了)
	vector<uint8_t> planeY;
	vector<uint8_t> planeU;
	vector<uint8_t> planeV;
};

//音频相关参数结构体
struct AUDIOPARAMS
{
	VoiceCallback* pCallback;
	HANDLE hThread;
	volatile BOOL bThreadRunning;
	HANDLE hStartEvent;
	BOOL bStartEventTriggered;
	AVFormatContext* fmtCtx;
	WAVEFORMATEX wfx;
	IXAudio2* pXAudio2;
	IXAudio2SourceVoice* pSourceVoice;
	IXAudio2MasteringVoice* pMasteringVoice;
	uint8_t* pcmBuffers[4];
	uint8_t* dst_data[8] = {nullptr};
	bool bIsBufferInUse[4] = {FALSE};
	int currentBufferIndex;
	CRITICAL_SECTION bufferCS;//关键段，保护缓冲区状态的访问
	HANDLE hFreeBufferEvent;//空闲缓冲区事件，通知音频线程有空闲缓冲区可以使用
	XAUDIO2_BUFFER xaBuffer;//避免在循环中分配，直接在结构体中定义一个成员变量
	AVCodecContext* codecCtx;
	int AudioStreamIndex;
	AVFrame* frame;
	SwrContext* SwrCtx;
	AVPacket* pkt;
};


enum COMPUTE_WINDOW_METHOD
{
	EXTEND_METHOD = 100,
};


//为窗口提供一个窗口过程，凑数
INT_PTR CALLBACK VTWProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

class VTWPARAMS
{
	friend class VoiceCallback;//将回调函数加入VTWPARAMS中，使其能访问私有单位
private:
	CHAR szFileName[MAX_PATH];
	int MinWhite;
	int MaxWhite;
	int MinBlack;
	int MaxBlack;
	BOOL bInsteadColor;
	int Width;
	int Height;
	int RectMinSize;
	RECT rect;
	COMPUTE_WINDOW_METHOD ComputeMethod;

	VIDEOPARAMS v;
	AUDIOPARAMS a;

	WNDPARAMS *pHeader;
	int wndnum;
	int startx;
	int starty;
    int curWndIndex;//当前窗口计数

public:
	VTWPARAMS();
	~VTWPARAMS();

	VOID SetVideoPath(LPSTR lpFileName);
	VOID SetWhiteRanges(int min, int max);
	VOID SetBlackRanges(int min, int max);
	VOID SetInsteadColor(BOOL bBlack);
	VOID SetDisplaySize(int width, int height);
	int GetWidth();
	int GetHeight();
	VOID SetRectMinSize(int size);
	VOID SetComputeMethod(COMPUTE_WINDOW_METHOD method);

	BOOL AutoUpdate();

	BOOL RequestFrame();
	BOOL ComputeWindow();
	VOID RectToWindow(int x, int y, int width, int height);
	VOID DisplayWindowFrame();
	VOID CreateNewWindow(int x, int y, int width, int height);
	VOID DeleteNumberOfEndWindow(int number);

	VOID StopAudioThread();//因为a是private的，所以需要一个public函数来停止音频线程。
	static DWORD WINAPI AudioThread(LPVOID lpParam);

	//算法们
	VOID ComputeWindow_EXTEND_METHOD();
};

// 设置 XAudio2 回调,提醒写入数据至XAudio2缓冲区（需要实现 IXAudio2VoiceCallback 类）
class VoiceCallback : public IXAudio2VoiceCallback
{
public:
	VTWPARAMS* pParams;
	VoiceCallback(VTWPARAMS* p) :pParams(p) {}
	void OnBufferEnd(void* pBufferContext)
	{
		int idx = (int)(intptr_t)pBufferContext;
		/*进入关键段*/
		EnterCriticalSection(&pParams->a.bufferCS);
		pParams->a.bIsBufferInUse[idx] = FALSE;
		SetEvent(pParams->a.hFreeBufferEvent);
		LeaveCriticalSection(&pParams->a.bufferCS);
	};
	void OnVoiceProcessingPassStart(UINT32) {}
	void OnVoiceProcessingPassEnd() {}
	void OnStreamEnd() {}
	void OnBufferStart(void*) {}
	void OnLoopEnd(void*) {}
	void OnVoiceError(void*, HRESULT) {}
};