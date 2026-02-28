#include"VTW.h"
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")

int main()
{
	double VideoPTS;
	double AudioClock;
	double diff;

	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	SetProcessDPIAware();

	string inputLine;
	while (TRUE)
	{
		VTWPARAMS v;
	start:
		// 视频路径
		cout << "视频路径（绝对路径且不含中文）：";
		if (!std::getline(cin, inputLine)) inputLine.clear();
		while (TRUE)
		{
			if (inputLine.empty())
			{
				cout << "路径不能为空，请重新输入：";
				if (!std::getline(cin, inputLine)) inputLine.clear();
			}
			else if (!(PathFileExistsA((const_cast<LPSTR>(inputLine.c_str())))))
			{
				cout << "文件不存在！请重新输入:";
				if (!std::getline(cin, inputLine)) inputLine.clear();
			}
			else
			{
				break;
			}
		}
		v.SetVideoPath(const_cast<LPSTR>(inputLine.c_str()));

		// 窗口替代颜色
		cout << "窗口替代颜色（\"w\"白，\"b\"黑）:";
		if (!std::getline(cin, inputLine)) inputLine.clear();
		if (!inputLine.empty() && (inputLine[0] == 'b' || inputLine[0] == 'B'))
			v.SetInsteadColor(BLACK);
		else
			v.SetInsteadColor(WHITE);

		// 白色范围
		while (true)
		{
			cout << "白色范围（输入格式：最小值 最大值）（回车使用默认值 225 255）：";
			if (!std::getline(cin, inputLine)) inputLine.clear();
			if (inputLine.empty())
			{
				v.SetWhiteRanges(225, 255);
				break;
			}
			std::istringstream iss(inputLine);
			int minVal, maxVal;
			if (iss >> minVal >> maxVal)
			{
				v.SetWhiteRanges(minVal, maxVal);
				break;
			}
			cout << "输入错误，输入 y 以重新输入，输入 n 以使用默认值 225 255：";
			if (!std::getline(cin, inputLine)) inputLine.clear();
			if (!inputLine.empty() && (inputLine[0] == 'y' || inputLine[0] == 'Y')) continue;
			v.SetWhiteRanges(225, 255);
			break;
		}

		// 黑色范围
		while (true)
		{
			cout << "黑色范围（输入格式：最小值 最大值）（回车使用默认值 16 36）：";
			if (!std::getline(cin, inputLine)) inputLine.clear();
			if (inputLine.empty())
			{
				v.SetBlackRanges(16, 36);
				break;
			}
			std::istringstream iss(inputLine);
			int minVal, maxVal;
			if (iss >> minVal >> maxVal)
			{
				v.SetBlackRanges(minVal, maxVal);
				break;
			}
			cout << "输入错误，输入 y 以重新输入，输入 n 以使用默认值 16 36：";
			if (!std::getline(cin, inputLine)) inputLine.clear();
			if (!inputLine.empty() && (inputLine[0] == 'y' || inputLine[0] == 'Y')) continue;
			v.SetBlackRanges(16, 36);
			break;
		}

		// 宽度与高度
		while (true)
		{
			cout << "宽度与高度（输入格式：宽度 高度）（回车使用屏幕分辨率）：";
			if (!std::getline(cin, inputLine)) inputLine.clear();
			if (inputLine.empty())
			{
				v.SetDisplaySize(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
				break;
			}
			std::istringstream iss(inputLine);
			int width, height;
			if (iss >> width >> height && width > 0 && height > 0)
			{
				v.SetDisplaySize(width, height);
				break;
			}
			cout << "输入错误，输入 y 以重新输入，输入 n 以全屏运行：";
			if (!std::getline(cin, inputLine)) inputLine.clear();
			if (!inputLine.empty() && (inputLine[0] == 'y' || inputLine[0] == 'Y')) continue;
			v.SetDisplaySize(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
			break;
		}

		// 是否启用缩放
		while (true)
		{
			cout << "是否启用缩放（改善性能，但损失画面）（\"y\"是，\"n\"否）（默认关闭）:";
			if (!std::getline(cin, inputLine)) inputLine.clear();

			if (inputLine.empty())
			{
				v.SetUsedResize(FALSE);
				break;
			}

			char c = inputLine[0];
			if (c == 'y' || c == 'Y')
			{
				// 启用缩放，询问目标宽高（回车使用屏幕分辨率）
				cout << "缩放后宽度 与 高度（输入格式：宽度 高度）（回车使用屏幕分辨率）:";
				if (!std::getline(cin, inputLine)) inputLine.clear();

				if (inputLine.empty())
				{
					v.SetDisplaySize(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
				}
				else
				{
					std::istringstream iss2(inputLine);
					int w, h;
					if (iss2 >> w >> h && w > 0 && h > 0)
					{
						v.SetResizeSize(w, h);
					}
					else
					{
						// 输入错误，使用屏幕分辨率作为回退
						cout << "输入错误，请重新输入" << endl;
						continue;
					}
				}
				v.SetUsedResize(TRUE);
				break;
			}
			else if (c == 'n' || c == 'N')
			{
				v.SetUsedResize(FALSE);
				// 保持已设置的显示尺寸或默认值
				break;
			}
			else
			{
				cout << "输入错误，请输入 y 或 n。" << endl;
				continue;
			}
		}

		// 最小矩形尺寸
		cout << "最小矩形尺寸（输入0则为默认）：";
		if (!std::getline(cin, inputLine)) inputLine.clear();
		int val = 0;
		if (!inputLine.empty())
			val = atoi(inputLine.c_str());
		if (val <= 0)
		{
			v.SetRectMinSize((v.GetWidth() + v.GetHeight()) / 200);
		}
		else
		{
			v.SetRectMinSize(val);
		}

		// 窗口计算算法
		cout << endl;
		cout << "窗口计算算法（输入0为默认，即扩展法）：" << endl;
		cout << "100.扩展法（贡献者Longn1ght）" << endl;
		cout << "101.贪心算法？" << endl;
		cout << "请输入算法编号（回车默认 100）：";
		if (!std::getline(cin, inputLine)) inputLine.clear();
		int methodVal = 0;
		if (!inputLine.empty()) methodVal = atoi(inputLine.c_str());
		switch (methodVal)
		{
		case EXTEND_METHOD:
			v.SetComputeMethod(EXTEND_METHOD);
			break;
		case GREEDY_METHOD:
			v.SetComputeMethod(GREEDY_METHOD);
			break;
		case 0:
		default:
			v.SetComputeMethod(EXTEND_METHOD);
			break;
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
			cout << "读帧成功，回车以继续运行" << endl;
			getchar();
		}
		else
		{
			cout << "读帧失败，回车以重填参数" << endl;
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
			VideoPTS = v.GetVideoPTS();
			AudioClock = v.GetAudioClock();
			diff= VideoPTS - AudioClock;
			if (diff<-0.100)//丢帧
			{
				continue;
			}
			if (diff>0.025)//等待音频
			{
				double WaitTime = min(diff, 1.0 / v.GetVideoFPS());
				Sleep((DWORD)(WaitTime * 1000));
			}

			v.ComputeWindow();
			v.DisplayWindowFrame();
		}

		cout << "是否播放其他视频（y/n）：";
		if (!std::getline(cin, inputLine)) inputLine.clear();
		if (!inputLine.empty() && (inputLine[0] == 'y' || inputLine[0] == 'Y'))
		{
			continue;
		}
		else
			break;
	}
	CoUninitialize();
	return 0;
}