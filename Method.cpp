#include"VTW.h"

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

	//——————各变量含义——————
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
			if (v.processed[off]) 
				continue;

			uint8_t val = v.data[off].y;
			bool match = !bInsteadColor ? (val >= MinWhite && val <= MaxWhite) : (val >= MinBlack && val <= MaxBlack);
			if (!match) 
				continue;

			// 从 (x, y) 向右扩展，找到连续匹配的最大 rx
			int rx = x;//先令rx为x，再进行下一步计算
			while (rx + 1 < targetW)//在0至targetW-1的范围内扩展，直到遇到不满足条件的点或者越界
			{
				size_t noff = static_cast<size_t>(rowOffBase + rx + 1);
				if (v.processed[noff])
					break;
				uint8_t mval = v.data[noff].y;
				bool nmatch = !bInsteadColor ? (mval >= MinWhite && mval <= MaxWhite) : (mval >= MinBlack && mval <= MaxBlack);
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
					bool m = !bInsteadColor ? (nval >= MinWhite && nval <= MaxWhite) : (nval >= MinBlack && nval <= MaxBlack);
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
				if (max(rectW, rectH) >= RectMinSizeLong / ResizeRatioX && min(rectW, rectH) >= RectMinSizeShort / ResizeRatioY)
					v.ResizeTemp.push_back({ x, y, x + rectW, y + rectH });//把当前矩形的坐标和尺寸存入ResizeTemp，等会再次进行放大缩放
				else
					if (max(rectW, rectH) >= RectMinSizeLong && min(rectW, rectH) >= RectMinSizeShort)
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
			if (max(bestW, bestH) >= RectMinSizeLong / ResizeRatioX && min(bestW, bestH) >= RectMinSizeShort / ResizeRatioY)
				v.ResizeTemp.push_back({ bestX, bestY, bestX + bestW, bestY + bestH });
		// 仅当满足最小尺寸要求时创建窗口，与扩展法保持一致
			else
				if (max(bestW, bestH) >= RectMinSizeLong && min(bestW, bestH) >= RectMinSizeShort)
					RectToWindow(bestX + startx, bestY + starty, bestW, bestH);

	}
}

// 实验性算法 102：基于行运行长度（run-length）合并的快速矩形生成
// 思路：先对每行生成连续匹配段（start,end），然后在纵向合并相同列范围的段以生成最大矩形
// 目标：比扩展法在大块连续区域下更快，并避免多次重复检查
VOID VTWPARAMS::ComputeWindow_EXPERIMENTAL_METHOD()
{
	if (bUseGPU)
	{
		if (ComputeWindow_EXPERIMENTAL_GPU())
			return;
		// otherwise fall through to CPU implementation
	}

	int targetW = bUsedResize ? ResizeWidth : Width;
	int targetH = bUsedResize ? ResizeHeight : Height;

	// 清空标记
	fill(v.processed.begin(), v.processed.end(), false);

	// 每行的段集合，存储为向量的向量：每个段为 pair<start,end>
	vector<vector<pair<int, int>>> runs(targetH);

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
	for (int y = 0; y < targetH; ++y)
	{
		std::vector<std::pair<int, int>> rowRuns;
		int rowOff = y * targetW;
		int x = 0;
		while (x < targetW)
		{
			size_t off = static_cast<size_t>(rowOff + x);
			if (v.processed[off]) { ++x; continue; }
			uint8_t val = v.data[off].y;
			bool match = !bInsteadColor ? (val >= MinWhite && val <= MaxWhite) : (val >= MinBlack && val <= MaxBlack);
			if (!match) { ++x; continue; }
			int start = x;
			int end = x;
			while (end + 1 < targetW)
			{
				size_t noff = static_cast<size_t>(rowOff + end + 1);
				if (v.processed[noff]) break;
				uint8_t nval = v.data[noff].y;
				bool nm = !bInsteadColor ? (nval >= MinWhite && nval <= MaxWhite) : (nval >= MinBlack && nval <= MaxBlack);
				if (!nm) break;
				++end;
			}
			rowRuns.push_back({ start, end });
			x = end + 1;
		}
		runs[y] = std::move(rowRuns);
	}

	// 合并纵向相同列范围的段，形成矩形
	struct RectSeg { int sx; int top; int ex; int bottom; };
	vector<vector<RectSeg>> rectsByRow(targetH);

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
	for (int y = 0; y < targetH; ++y)
	{
		auto& localRuns = runs[y];
		auto& localRects = rectsByRow[y];
		localRects.reserve(localRuns.size());
		for (auto& seg : localRuns)
		{
			int sx = seg.first;
			int ex = seg.second;
			int top = y;
			int bottom = y;
			// 尝试向下合并（只读取 runs，安全并行）
			for (int ny = y + 1; ny < targetH; ++ny)
			{
				bool found = false;
				for (auto& nseg : runs[ny])
				{
					if (nseg.first <= sx && nseg.second >= ex)
					{
						found = true;
						break;
					}
				}
				if (found) bottom = ny;
				else break;
			}
			localRects.push_back({ sx, top, ex, bottom });
		}
	}

	for (int y = 0; y < targetH; ++y)
	{
		for (auto& r : rectsByRow[y])
		{
			int sx = r.sx;
			int top = r.top;
			int ex = r.ex;
			int bottom = r.bottom;

			if (v.processed[static_cast<size_t>(top * targetW + sx)]) continue;

			int rectW = ex - sx + 1;
			int rectH = bottom - top + 1;

			// 标记为已处理
			for (int yy = top; yy <= bottom; ++yy)
			{
				int rowOff = yy * targetW;
				for (int xx = sx; xx <= ex; ++xx)
					v.processed[static_cast<size_t>(rowOff + xx)] = true;
			}

			// 输出窗口（考虑缩放）
			if (bUsedResize)
			{
				if (max(rectW, rectH) >= RectMinSizeLong / ResizeRatioX && min(rectW, rectH) >= RectMinSizeShort / ResizeRatioY)
					v.ResizeTemp.push_back({ sx, top, sx + rectW, top + rectH });
			}
			else
			{
				if (max(rectW, rectH) >= RectMinSizeLong && min(rectW, rectH) >= RectMinSizeShort)
					RectToWindow(sx + startx, top + starty, rectW, rectH);
			}
		}
	}
};

BOOL VTWPARAMS::ComputeWindow_EXPERIMENTAL_GPU()
{
	if (!bUseGPU) return FALSE;
	if (!InitD3DIfNeeded()) return FALSE;

	int targetW = bUsedResize ? ResizeWidth : Width;
	int targetH = bUsedResize ? ResizeHeight : Height;
	if (targetW <= 0 || targetH <= 0) return FALSE;
	if (static_cast<size_t>(targetW) * static_cast<size_t>(targetH) > v.planeY.size()) return FALSE;

	ID3D11Texture2D* inputTex = nullptr;
	ID3D11ShaderResourceView* inputSRV = nullptr;
	ID3D11Texture2D* outTex = nullptr;
	ID3D11UnorderedAccessView* outUAV = nullptr;
	ID3D11Texture2D* stagingTex = nullptr;
	ID3D11Buffer* cb = nullptr;

	auto cleanup = [&]() {
		if (cb) { cb->Release(); cb = nullptr; }
		if (stagingTex) { stagingTex->Release(); stagingTex = nullptr; }
		if (outUAV) { outUAV->Release(); outUAV = nullptr; }
		if (outTex) { outTex->Release(); outTex = nullptr; }
		if (inputSRV) { inputSRV->Release(); inputSRV = nullptr; }
		if (inputTex) { inputTex->Release(); inputTex = nullptr; }
		};

	// create input texture (R8)
	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = targetW; texDesc.Height = targetH; texDesc.MipLevels = 1; texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8_UINT; texDesc.SampleDesc.Count = 1; texDesc.Usage = D3D11_USAGE_DEFAULT; texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initData{}; initData.pSysMem = v.planeY.data(); initData.SysMemPitch = targetW;
	HRESULT hr = d3dDevice->CreateTexture2D(&texDesc, &initData, &inputTex);
	if (FAILED(hr)) { cleanup(); return FALSE; }

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{}; srvDesc.Format = texDesc.Format; srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvDesc.Texture2D.MipLevels = 1;
	hr = d3dDevice->CreateShaderResourceView(inputTex, &srvDesc, &inputSRV);
	if (FAILED(hr)) { cleanup(); return FALSE; }

	// output texture + UAV
	D3D11_TEXTURE2D_DESC outDesc = texDesc; outDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE; outDesc.Usage = D3D11_USAGE_DEFAULT;
	hr = d3dDevice->CreateTexture2D(&outDesc, NULL, &outTex);
	if (FAILED(hr)) { cleanup(); return FALSE; }

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{}; uavDesc.Format = outDesc.Format; uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D; uavDesc.Texture2D.MipSlice = 0;
	hr = d3dDevice->CreateUnorderedAccessView(outTex, &uavDesc, &outUAV);
	if (FAILED(hr)) { cleanup(); return FALSE; }

	// staging for readback
	D3D11_TEXTURE2D_DESC stagingDesc = outDesc; stagingDesc.Usage = D3D11_USAGE_STAGING; stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; stagingDesc.BindFlags = 0;
	hr = d3dDevice->CreateTexture2D(&stagingDesc, NULL, &stagingTex);
	if (FAILED(hr)) { cleanup(); return FALSE; }

	// constant buffer
	struct GPUParams { UINT width; UINT height; UINT minW; UINT maxW; UINT minB; UINT maxB; UINT instead; UINT pad; } params;
	params.width = (UINT)targetW; params.height = (UINT)targetH; params.minW = (UINT)MinWhite; params.maxW = (UINT)MaxWhite; params.minB = (UINT)MinBlack; params.maxB = (UINT)MaxBlack; params.instead = bInsteadColor ? 1u : 0u; params.pad = 0;
	D3D11_BUFFER_DESC cbDesc{}; cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; cbDesc.ByteWidth = sizeof(GPUParams); cbDesc.Usage = D3D11_USAGE_DEFAULT; D3D11_SUBRESOURCE_DATA cbd{}; cbd.pSysMem = &params;
	hr = d3dDevice->CreateBuffer(&cbDesc, &cbd, &cb);
	if (FAILED(hr)) { cleanup(); return FALSE; }

	// bind and dispatch
	d3dContext->CSSetShader(d3dCS, NULL, 0);
	ID3D11ShaderResourceView* srvs[1] = { inputSRV };
	d3dContext->CSSetShaderResources(0, 1, srvs);
	ID3D11UnorderedAccessView* uavs[1] = { outUAV };
	d3dContext->CSSetUnorderedAccessViews(0, 1, uavs, NULL);
	d3dContext->CSSetConstantBuffers(0, 1, &cb);
	UINT gx = (targetW + 15) / 16; UINT gy = (targetH + 15) / 16;
	d3dContext->Dispatch(gx, gy, 1);

	// read back
	d3dContext->CopyResource(stagingTex, outTex);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	hr = d3dContext->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mapped);
	if (SUCCEEDED(hr))
	{
		vector<vector<pair<int, int>>> runs(targetH);
		for (int y = 0; y < targetH; ++y)
		{
			uint8_t* rowPtr = reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(mapped.pData) + y * mapped.RowPitch);
			vector<pair<int, int>> rowRuns;
			int x = 0;
			while (x < targetW)
			{
				if (rowPtr[x] == 0) { ++x; continue; }
				int start = x; int end = x;
				while (end + 1 < targetW && rowPtr[end + 1]) ++end;
				rowRuns.push_back({ start, end });
				x = end + 1;
			}
			runs[y] = std::move(rowRuns);
		}

		struct RectSeg { int sx; int top; int ex; int bottom; };
		vector<vector<RectSeg>> rectsByRow(targetH);
		for (int y = 0; y < targetH; ++y)
		{
			auto& localRuns = runs[y];
			auto& localRects = rectsByRow[y];
			localRects.reserve(localRuns.size());
			for (auto& seg : localRuns)
			{
				int sx = seg.first; int ex = seg.second; int top = y; int bottom = y;
				for (int ny = y + 1; ny < targetH; ++ny)
				{
					bool found = false;
					for (auto& nseg : runs[ny]) if (nseg.first <= sx && nseg.second >= ex) { found = true; break; }
					if (found) bottom = ny; else break;
				}
				localRects.push_back({ sx, top, ex, bottom });
			}
		}

		fill(v.processed.begin(), v.processed.end(), false);
		for (int y = 0; y < targetH; ++y)
		{
			for (auto& r : rectsByRow[y])
			{
				int sx = r.sx; int top = r.top; int ex = r.ex; int bottom = r.bottom;
				if (v.processed[static_cast<size_t>(top * targetW + sx)]) continue;
				int rectW = ex - sx + 1; int rectH = bottom - top + 1;
				for (int yy = top; yy <= bottom; ++yy)
				{
					int rowOff = yy * targetW;
					for (int xx = sx; xx <= ex; ++xx)
						v.processed[static_cast<size_t>(rowOff + xx)] = true;
				}
				if (bUsedResize)
				{
					if (max(rectW, rectH) >= RectMinSizeLong / ResizeRatioX && min(rectW, rectH) >= RectMinSizeShort / ResizeRatioY)
						v.ResizeTemp.push_back({ sx, top, sx + rectW, top + rectH });
				}
				else
				{
					if (max(rectW, rectH) >= RectMinSizeLong && min(rectW, rectH) >= RectMinSizeShort)
						RectToWindow(sx + startx, top + starty, rectW, rectH);
				}
			}
		}

		d3dContext->Unmap(stagingTex, 0);
	}

	{
		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		d3dContext->CSSetShaderResources(0, 1, nullSRV);
		ID3D11UnorderedAccessView* nullUAV[1] = { nullptr };
		d3dContext->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);
		ID3D11Buffer* nullCB[1] = { nullptr };
		d3dContext->CSSetConstantBuffers(0, 1, nullCB);
		d3dContext->CSSetShader(nullptr, nullptr, 0);
		d3dContext->Flush();
	}

	cleanup();
	return TRUE;
}