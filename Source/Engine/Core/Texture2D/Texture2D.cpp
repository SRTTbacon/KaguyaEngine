#include "..\\..\\Engine.h"
#include "..\CoreMinimal.h"
#include "..\\Main\\Utility.h"
#include "..\\DirectXCore\\ResourceStatics.h"
#include "..\\Thread\\ResourceCopyThread.h"
#include "..\\Thread\\WorkerThread.h"
#include "Texture2D.h"

#include <memory>
#include <dxgitype.h>
#include <cstdio>
#include <DirectXTex.h>
#include <Windows.h>
#include <d3dx12.h>
#include <d3d12.h>
#include <mutex>
#include <utility>
#include <algorithm>
#include <dxgiformat.h>
#include <string.h>
#include <vector>
#include <string>
#include <functional>

using namespace DirectX;
using namespace KaguyaEngine::AsyncTask;

namespace KaguyaEngine::Texture
{
	std::vector<ObjectPtr<TextureManage>> Texture2D::s_textures;

	// -------------------------TextureManage-------------------------
	TextureManage::TextureManage()
		: uniqueID(0)
		, bSimpleTex(true)
		, bLoaded(false)
		, pResource(nullptr)
	{}

	TextureManage::~TextureManage()
	{
		if (pResource)
		{
			pResource = nullptr;
		}
	}

	// -------------------------Texture2D-------------------------

	Texture2D::Texture2D()
	{
		Initialize();
	}

	Texture2D::~Texture2D()
	{
		Destroy();
	}

	void Texture2D::Initialize()
	{
	}

	void Texture2D::Destroy()
	{
		if (!m_pManage) {
			return;
		}

		uint32 uniqueID = m_pManage->uniqueID;

		m_pManage = nullptr;

		//TextureManageを参照している数が1(std::vectorのみ)になったら解放
		for (uint32 i = 0; i < static_cast<uint32>(s_textures.size()); i++)
		{
			if (s_textures[i]->uniqueID == uniqueID && s_textures[i].use_count() <= 1)
			{
				std::vector<ObjectPtr<TextureManage>>::iterator it = s_textures.begin();
				it += i;
				s_textures.erase(it);
			}
		}
	}

	void Texture2D::LoadTexture(const std::string& path, bool bDDSFile)
	{
		uint32 uniqueID = GenerateIDFromFile(path);

		//既に読み込まれているテクスチャかどうかを調べる
		for (ObjectPtr<TextureManage>& tex : s_textures)
		{
			if (tex->uniqueID == uniqueID)
			{
				//既に読み込まれているテクスチャを参照
				Destroy();
				m_pManage = tex;
				if (m_onLoaded) {
					m_onLoaded();
				}
				return;
			}
		}

		m_loadIndex++;

		Destroy();

		//テクスチャ管理配列に登録
		ObjectPtr<TextureManage> manage = CreateObjectPtr<TextureManage>();
		manage->uniqueID = uniqueID;
		manage->bSimpleTex = false;
		s_textures.push_back(manage);
		m_pManage = manage;

		LoadAsync(path, bDDSFile);
	}

	void Texture2D::LoadAsync(const std::string& path, bool bDDSFile)
	{
		AsyncTask::Job task{};
		ObjectPtr<Texture2D> pSelfPtr = shared_from_this();
		task.func = [pSelfPtr, path, bDDSFile]()
			{
			//WICテクスチャのロード
			TexMetadata meta = {};
			ScratchImage scratch = {};

			std::wstring wPath = GetWideString(path);

			HRESULT hr = 0;
			if (bDDSFile) {
				hr = LoadFromDDSFile(wPath.c_str(), DDS_FLAGS_NONE, &meta, scratch);
			}
			else {
				hr = LoadFromWICFile(wPath.c_str(), WIC_FLAGS_NONE, &meta, scratch);
			}

			ObjectPtr<AsyncData> pAsyncData = CreateObjectPtr<AsyncData>();
			pAsyncData->m_outTextureInfo.bDDSFormat = bDDSFile;
			pAsyncData->m_outTextureInfo.filePath = path;
			pAsyncData->m_loadIndex = pSelfPtr->m_loadIndex.load();
			pAsyncData->m_outTextureInfo.bHCSFile = false;
			pAsyncData->m_outTextureInfo.bUseColor = false;

			if (FAILED(hr))
			{
				printf("テクスチャの読み込みに失敗 - %s\n", path.c_str());
				return;
			}

			pAsyncData->m_outTextureInfo.metadata = meta;

			pSelfPtr->LoadAsync(pSelfPtr, pAsyncData, meta, scratch);
			};

		//	別スレッドにタスクを渡す
		LockMutex lock(g_jobManager->m_mutex);
		g_jobManager->m_queue.push(std::move(task));
		g_jobManager->m_cv.notify_one();
	}

	void Texture2D::LoadTexture(std::vector<char>& data, bool bDDSFile)
	{
		uint32 uniqueID = GenerateIDFromFile(data.data(), data.size());

		//既に読み込まれているテクスチャかどうかを調べる
		for (ObjectPtr<TextureManage>& tex : s_textures)
		{
			if (tex->uniqueID == uniqueID)
			{
				//既に読み込まれているテクスチャを参照
				Destroy();
				m_pManage = tex;
				if (m_onLoaded) {
					m_onLoaded();
				}
				return;
			}
		}

		m_loadIndex++;

		Destroy();

		//テクスチャ管理配列に登録
		ObjectPtr<TextureManage> manage = std::make_shared<TextureManage>();
		manage->uniqueID = uniqueID;
		manage->bSimpleTex = false;
		s_textures.push_back(manage);
		m_pManage = manage;

		LoadAsync(data, bDDSFile);
	}

	void Texture2D::LoadTexture(const char* data, size_t dataSize, bool bDDSFile)
	{
		std::vector<char> temp(data, data + dataSize);
		LoadTexture(temp, bDDSFile);
	}

	void Texture2D::LoadAsync(std::vector<char>& data, bool bDDSFile)
	{
		AsyncTask::Job task{};
		ObjectPtr<Texture2D> pSelfPtr = shared_from_this();
		task.func = [pSelfPtr, data = std::move(data), bDDSFile]() {

			//WICテクスチャのロード
			TexMetadata meta = {};
			ScratchImage scratch = {};

			HRESULT hr = 0;
			if (bDDSFile) {
				hr = LoadFromDDSMemory(data.data(), data.size(), DDS_FLAGS_NONE, &meta, scratch);
			}
			else {
				hr = LoadFromWICMemory(data.data(), data.size(), WIC_FLAGS_NONE, &meta, scratch);
			}

			ObjectPtr<AsyncData> pAsyncData = CreateObjectPtr<AsyncData>();
			pAsyncData->m_outTextureInfo.bDDSFormat = bDDSFile;
			pAsyncData->m_outTextureInfo.filePath = "";
			pAsyncData->m_loadIndex = pSelfPtr->m_loadIndex.load();
			pAsyncData->m_outTextureInfo.bHCSFile = false;
			pAsyncData->m_outTextureInfo.bUseColor = false;

			if (FAILED(hr))
			{
				printf("テクスチャの読み込みに失敗\n");
				return;
			}

			pAsyncData->m_outTextureInfo.metadata = meta;

			pSelfPtr->LoadAsync(pSelfPtr, pAsyncData, meta, scratch);
			};

		// 別スレッドにタスクを渡す
		LockMutex lock(g_jobManager->m_mutex);
		g_jobManager->m_queue.push(std::move(task));
		g_jobManager->m_cv.notify_one();
		lock.unlock();
	}

	void Texture2D::LoadAsync(ObjectPtr<Texture2D> pSelfPtr, ObjectPtr<AsyncData> pAsyncData, TexMetadata& meta, ScratchImage& scratch)
	{
		CD3DX12_HEAP_PROPERTIES prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(meta.format, static_cast<uint32>(meta.width), static_cast<uint32>(meta.height),
			static_cast<uint16>(meta.arraySize), static_cast<uint16>(meta.mipLevels));

		//リソースを生成
		HRESULT hr = g_Engine->GetDevice()->CreateCommittedResource(
			&prop,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&pAsyncData->m_pOutResource)
		);
		if (FAILED(hr))
		{
			printf("テクスチャの読み込みに失敗 エラーコード = %1x\n", hr);
			return;
		}
		pAsyncData->m_pOutResource->SetName(L"TextureResource");

		//アップロードバッファのサイズを計算
		uint32 numSubresources = static_cast<uint32>(meta.mipLevels * meta.arraySize);
		uint64 totalBytes = 0;
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(numSubresources);
		std::vector<uint32> numRows(numSubresources);
		std::vector<uint64> rowSizeInBytes(numSubresources);

		g_Engine->GetDevice()->GetCopyableFootprints(
			&desc, 0, numSubresources, 0,
			layouts.data(), numRows.data(), rowSizeInBytes.data(), &totalBytes
		);

		//アップロードバッファを作成
		ID3D12Resource* pUploadBuffer = ResourceStatics::CreateUploadResource(totalBytes);
		pUploadBuffer->SetName(L"TempTextureUpload");

		// pUploadBufferにscratchをコピー
		void* mappedData = nullptr;
		pUploadBuffer->Map(0, nullptr, &mappedData);

		uint8* pDestBase = reinterpret_cast<uint8*>(mappedData);

		for (uint32 i = 0; i < meta.arraySize; i++)
		{
			for (uint32 j = 0; j < meta.mipLevels; j++)
			{
				const Image* img = scratch.GetImage(j, i, 0);

				uint32 subresourceIndex = D3D12CalcSubresource(j, i, 0, static_cast<uint32>(meta.mipLevels), static_cast<uint32>(meta.arraySize));

				uint8* pDestSlice = pDestBase + layouts[subresourceIndex].Offset;
				uint8* pSrcSlice = img->pixels;

				for (uint32 y = 0; y < numRows[subresourceIndex]; y++) {
					memcpy(pDestSlice + layouts[subresourceIndex].Footprint.RowPitch * y, pSrcSlice + img->rowPitch * y, rowSizeInBytes[subresourceIndex]);
				}
			}
		}

		pUploadBuffer->Unmap(0, nullptr);

		// コピーコマンドを発行
		CopyJob copyJob = {};

		copyJob.func = [pAsyncData, pUploadBuffer, layouts, meta](ID3D12GraphicsCommandList* pCommandList) {
			CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(pAsyncData->m_pOutResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
			pCommandList->ResourceBarrier(1, &barrier2);

			// 各面をコピー
			for (uint32 i = 0; i < meta.arraySize; ++i)
			{
				for (uint32 j = 0; j < meta.mipLevels; ++j)
				{
					uint32 subresourceIndex = D3D12CalcSubresource(j, i, 0, static_cast<uint32>(meta.mipLevels), static_cast<uint32>(meta.arraySize));

					D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
					dstLocation.pResource = pAsyncData->m_pOutResource.Get();
					dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					dstLocation.SubresourceIndex = subresourceIndex;

					D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
					srcLocation.pResource = pUploadBuffer;
					srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					srcLocation.PlacedFootprint = layouts[subresourceIndex];

					pCommandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
				}
			}
			};

		// 終了時にバリアを移行。ここはメインスレッド
		copyJob.OnComplete = [pSelfPtr, pUploadBuffer, pAsyncData](){
			if (pSelfPtr && pSelfPtr->m_pManage && pSelfPtr.use_count() > 1)
			{
				if (pSelfPtr->m_loadIndex.load() == pAsyncData->m_loadIndex)
				{
					pSelfPtr->m_pManage->pResource = pAsyncData->m_pOutResource;
					pSelfPtr->m_pManage->textureInfo = pAsyncData->m_outTextureInfo;
					pSelfPtr->m_pManage->bLoaded.store(true);

					// バリアを設定
					CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(pSelfPtr->m_pManage->pResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					g_Engine->GetCommandList()->ResourceBarrier(1, &barrier);

					if (pSelfPtr->m_onLoaded) {
						pSelfPtr->m_onLoaded();
					}
				}
			}
			pUploadBuffer->Release();
			};

		//	コピー処理をコピー用スレッドに登録
		LockMutex lock2(g_copyJobManager->m_mutex);
		g_copyJobManager->m_jobs.push_back(std::move(copyJob));
		g_copyJobManager->m_cv.notify_one();
	}

	// 単色テクスチャを生成
	void Texture2D::LoadTexture(float r, float g, float b, float a, uint32 width, uint32 height, DXGI_FORMAT format)
	{
		size_t rowPitch = (width * sizeof(uint32) + 255) & ~255; //256の倍数になるように
		size_t tempWidth = rowPitch / sizeof(uint32);

		std::vector<uint32> data(tempWidth * height);
		std::fill(data.begin(), data.end(), ColorToUINT(r, g, b, a));

		//uint32データをバイト単位に変換するためのVector<char>
		std::vector<char> charVector(data.size() * sizeof(uint32));
		// データをコピー
		std::memcpy(charVector.data(), data.data(), data.size() * sizeof(uint32));

		// 既に読み込まれているテクスチャかどうかを調べる
		uint32 uniqueID = GenerateIDFromFile(charVector);

		for (ObjectPtr<TextureManage>& tex : s_textures) {
			if (tex->uniqueID == uniqueID) {
				Destroy();
				m_pManage = tex;
				if (m_onLoaded) {
					m_onLoaded();
				}
				return;
			}
		}

		Destroy();

		// 今のところまでのリソースを登録
		ObjectPtr<TextureManage> manage = CreateObjectPtr<TextureManage>();
		manage->uniqueID = uniqueID;
		manage->bSimpleTex = true;

		s_textures.push_back(manage);

		m_pManage = manage;
		m_pManage->textureInfo.textureColor = DXGI_RGBA(r, g, b, a);
		m_pManage->textureInfo.bUseColor = true;

		size_t size = 0;
		if (format == DXGI_FORMAT_NV12) {
			size_t yRowPitch = GetAlignedSize(width);
			size_t yPlaneSize = yRowPitch * height;

			size_t uvRowPitch = GetAlignedSize(width); // UVもRowPitch同じ
			size_t uvPlaneSize = uvRowPitch * (height / 2);

			size = yPlaneSize + uvPlaneSize;
		}
		else {
			size = rowPitch * height * sizeof(uint32);
		}

		ID3D12Resource* pUploadBuffer = ResourceStatics::CreateUploadResource(size);

		// アップロードバッファへのデータコピー
		void* mappedData = nullptr;
		HRESULT hr = pUploadBuffer->Map(0, nullptr, &mappedData);
		if (FAILED(hr)) {
			printf("アップロードバッファのマップに失敗しました。\n");
			return;
		}

		if (format == DXGI_FORMAT_NV12) {
			std::vector<uint8> nv12Buffer;
			nv12Buffer.resize(static_cast<std::vector<uint8, std::allocator<uint8>>::size_type>(width) * height * 3 / 2);

			// Y面を埋める（真っ白）
			memset(nv12Buffer.data(), ColorToUINT(r, g, b, a), static_cast<size_t>(width) * height);

			// UV面を埋める（無彩色）
			memset(nv12Buffer.data() + width * height, 0x80, static_cast<size_t>(width) * height / 2);

			memcpy(mappedData, nv12Buffer.data(), nv12Buffer.size());
		}
		else {
			memcpy(mappedData, data.data(), rowPitch * height);
		}
		pUploadBuffer->Unmap(0, nullptr);

		AsyncData asyncData{};
		asyncData.m_loadIndex = m_loadIndex.load();

		// デフォルトヒープのリソースを作成
		ResourceStatics::CreateResource(format, D3D12_HEAP_TYPE_DEFAULT, width, height, asyncData.m_pOutResource);

		ObjectPtr<Texture2D> pThis = shared_from_this();
		CopyJob copyJob{};
		copyJob.func = [pThis, format, width, height, rowPitch, asyncData, pUploadBuffer](ID3D12GraphicsCommandList* pCommandList)
			{
				CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(asyncData.m_pOutResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
				pCommandList->ResourceBarrier(1, &barrier2);

				if (format == DXGI_FORMAT_NV12) {
					// Y平面（Luma）
					D3D12_TEXTURE_COPY_LOCATION dstLocationY = {};
					dstLocationY.pResource = asyncData.m_pOutResource.Get();
					dstLocationY.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					dstLocationY.SubresourceIndex = 0;

					D3D12_TEXTURE_COPY_LOCATION srcLocationY = {};
					srcLocationY.pResource = pUploadBuffer;
					srcLocationY.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					srcLocationY.PlacedFootprint.Offset = 0;
					srcLocationY.PlacedFootprint.Footprint.Width = width;
					srcLocationY.PlacedFootprint.Footprint.Height = height;
					srcLocationY.PlacedFootprint.Footprint.Depth = 1;
					srcLocationY.PlacedFootprint.Footprint.RowPitch = static_cast<uint32>(GetAlignedSize(width));
					srcLocationY.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8_UNORM;

					pCommandList->CopyTextureRegion(&dstLocationY, 0, 0, 0, &srcLocationY, nullptr);

					// UV平面（Chroma）
					D3D12_TEXTURE_COPY_LOCATION dstLocationUV = {};
					dstLocationUV.pResource = asyncData.m_pOutResource.Get();
					dstLocationUV.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					dstLocationUV.SubresourceIndex = 1;

					size_t yRowPitch = GetAlignedSize(width); // 幅を256バイトアライメント
					size_t yPlaneSize = yRowPitch * height;

					D3D12_TEXTURE_COPY_LOCATION srcLocationUV = {};
					srcLocationUV.pResource = pUploadBuffer;
					srcLocationUV.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					srcLocationUV.PlacedFootprint.Offset = yPlaneSize;				// Yの後ろにUVが続く
					srcLocationUV.PlacedFootprint.Footprint.Width = width / 2;		// 横半分扱い
					srcLocationUV.PlacedFootprint.Footprint.Height = height / 2;	// 縦半分
					srcLocationUV.PlacedFootprint.Footprint.Depth = 1;
					srcLocationUV.PlacedFootprint.Footprint.RowPitch = static_cast<uint32>(GetAlignedSize(width)); // 横×2バイトだからwidth*2のAlign256
					srcLocationUV.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8_UNORM;

					pCommandList->CopyTextureRegion(&dstLocationUV, 0, 0, 0, &srcLocationUV, nullptr);
				}
				else
				{
					// コピー先を設定
					D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
					dstLocation.pResource = asyncData.m_pOutResource.Get();
					dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					dstLocation.SubresourceIndex = 0;

					// コピー元を設定
					D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
					srcLocation.pResource = pUploadBuffer;
					srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					srcLocation.PlacedFootprint.Footprint.Width = width;
					srcLocation.PlacedFootprint.Footprint.Height = height;
					srcLocation.PlacedFootprint.Footprint.Depth = 1;
					srcLocation.PlacedFootprint.Footprint.RowPitch = static_cast<uint32>(rowPitch);
					srcLocation.PlacedFootprint.Footprint.Format = format;

					// コマンドリストでコピー操作
					pCommandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
				}
			};
		copyJob.OnComplete = [pThis, pUploadBuffer, asyncData]()
			{
				if (asyncData.m_loadIndex == pThis->m_loadIndex.load())
				{
					// リソースバリアを変更
					CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(asyncData.m_pOutResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
					g_Engine->GetCommandList()->ResourceBarrier(1, &barrier);
					pThis->m_pManage->pResource = asyncData.m_pOutResource;
					pThis->m_pManage->bLoaded.store(true);
				}
				pUploadBuffer->Release();

				if (pThis->m_onLoaded) {
					pThis->m_onLoaded();
				}
			};
		LockMutex lock(g_copyJobManager->m_mutex);
		g_copyJobManager->m_jobs.push_back(copyJob);
		g_copyJobManager->m_cv.notify_one();
	}

	void Texture2D::SetOnLoaded(std::function<void()> callback)
	{
		m_onLoaded = std::move(callback);
	}
}