#include "DirectXDevice.h"
#include "ResourceStatics.h"
#include "..\\CoreMinimal.h"
#include "..\\EngineDefine.h"
#include "..\\FrameBuffer.h"
#include "..\\EngineKeyString.h"
#include "..\\..\\Core\\Pipeline\\PipelineDefine.h"
#include "..\\..\\Core\\Pipeline\\RootSignature.h"
#include "..\\..\\Core\\Pipeline\\PipelineState.h"
#include "..\\..\\3D\\Camera\\Camera.h"
#include "..\\Thread\\ResourceCopyThread.h"
#include "..\\DescriptorHeap\\DescriptorHeap.h"

#include <d3dx12.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d12.h>
#include <d3dcommon.h>
#include <dxgiformat.h>
#include <DirectXMath.h>

#if _DEBUG
#include <pix3.h>
#include <PIXEvents.h>
#include <pix3_win.h>
#endif

#include <Windows.h>
#include <cstdio>
#include <string>
#include <utility>
#include <string.h>
#include <cstdint>
#include <vector>

using namespace DirectX;
using namespace KaguyaEngine;
using namespace KaguyaEngine::Core;
using namespace KaguyaEngine::AsyncTask;

DirectXDevice::DirectXDevice(HWND hwnd)
	: m_hWnd(hwnd)
	, m_globalFenceValue(0)
	, m_computeFenceValue(0)
	, m_currentBackBufferIndex(0)
	, m_uiMainSrvIndex(UINT32_MAX)
	, m_renderViewport{}
	, m_renderScissor{}
	, m_nowWindowSize(0, 0)
	, m_pFinalRootSignature(nullptr)
	, m_pFinalPipelineState(nullptr)
	, m_pCopyDescriptor(nullptr)
	, m_pUIResource(nullptr)
	, m_vertexBufferView(D3D12_VERTEX_BUFFER_VIEW())
	, m_indexBufferView(D3D12_INDEX_BUFFER_VIEW())
{
}

DirectXDevice::~DirectXDevice()
{
	// コピー用スレッドを停止してリソースを解放
	KaguyaEngine::AsyncTask::g_resourceCopyThread->StopThread();
	KaguyaEngine::AsyncTask::g_resourceCopyThread.reset();

	// デバイスのリソースを解放
	CloseHandle(m_fenceEvent);
	CloseHandle(m_computeFenceEvent);
}

bool DirectXDevice::CreateDevice(XMUINT2 windowSize)
{
#ifdef _DEBUG
	// デバッグレイヤーの設定
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_pDebugController))))
	{
		m_pDebugController->SetEnableGPUBasedValidation(TRUE);
		m_pDebugController->SetEnableSynchronizedCommandQueueValidation(TRUE);
		m_pDebugController->EnableDebugLayer();
		printf("デバッグレイヤーを有効化しました。\n");
	}
#endif

	m_nowWindowSize = windowSize;

	CreateViewScissor();

	if (!CreateDevice())
	{
		printf("デバイスの生成に失敗しました。GPUがレイトレーシングに対応していない可能性があります。\n");
		return false;
	}

	if (!CreateCommandQueue())
	{
		printf("コマンドキューの生成に失敗\n");
		return false;
	}

	if (!CreateSwapChain())
	{
		printf("スワップチェインの生成に失敗\n");
		return false;
	}

	if (!CreateCommandList())
	{
		printf("コマンドリストの生成に失敗\n");
		return false;
	}

	if (!CreateFence())
	{
		printf("フェンスの生成に失敗\n");
		return false;
	}

	if (!CreateRenderTarget())
	{
		printf("レンダーターゲットの生成に失敗\n");
		return false;
	}

	if (!CreateDepthStencil())
	{
		printf("デプスステンシルバッファの生成に失敗\n");
		return false;
	}

	if (!CreateCameraPipeline())
	{
		printf("最終描画のパイプラインの構築に失敗しました。\n");
		return false;
	}

	ResourceStatics::Initialize(m_pDevice.Get());
	KaguyaEngine::AsyncTask::ResourceCopyThread::CreateInstance(m_pDevice.Get());

	// 最終出力用の頂点バッファとインデックスバッファを作成
	CreateVertexBuffer();
	CreateIndexBuffer();

#if _DEBUG
	PIXLoadLatestWinPixGpuCapturerLibrary();
#endif

	return true;
}

void DirectXDevice::UpdateRenderTargets(CopyDescriptor* pCopyDescriptor, const std::vector<Camera*>& pCameraList, ID3D12Resource* pUIResource)
{
	m_pCopyDescriptor = pCopyDescriptor;
	m_pCameraList = pCameraList;
	m_pUIResource = pUIResource;

	if (m_uiMainSrvIndex == UINT32_MAX) {
		m_uiMainSrvIndex = m_pCopyDescriptor->pMainHeap->GetNextAvailableDescriptor();
	}

	m_pCopyDescriptor->pMainHeap->SetResource(pUIResource, m_uiMainSrvIndex);
}

void DirectXDevice::FinalRender()
{
	CameraInfo cameraInfo{};
	for (uint32 i = 0; i < static_cast<uint32>(m_pCameraList.size()); i++)
	{
		cameraInfo.cameras[i].x = m_pCameraList[i]->m_drawPosition.x;
		cameraInfo.cameras[i].y = m_pCameraList[i]->m_drawPosition.y;
		cameraInfo.cameras[i].z = m_pCameraList[i]->GetDrawSize().x;
		cameraInfo.cameras[i].w = m_pCameraList[i]->GetDrawSize().y;
	}
	cameraInfo.cameraCount = static_cast<uint32>(m_pCameraList.size());

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(GetRtvResource(m_currentBackBufferIndex), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	GetCommandList()->ResourceBarrier(1, &barrier);

	uint32 copyIndex = m_pCopyDescriptor->pCopyHeap->GetNextAvailableDescriptor(static_cast<uint32>(m_pCameraList.size() + 1));
	m_pDevice->CopyDescriptorsSimple(1, m_pCopyDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex), m_pCopyDescriptor->pMainHeap->GetCpuDescriptorHandle(m_uiMainSrvIndex), m_pCopyDescriptor->pMainHeap->GetHeapType());
	for (uint32 i = 0; i < cameraInfo.cameraCount; i++){
		uint32 tempIndex = copyIndex + i + 1;
		const RenderResource* finalRender = m_pCameraList[i]->GetFinalRender();
		m_pDevice->CopyDescriptorsSimple(1, m_pCopyDescriptor->pCopyHeap->GetCpuDescriptorHandle(tempIndex), finalRender->mainSrvHandleCPU, m_pCopyDescriptor->pMainHeap->GetHeapType());
	}
	D3D12_CPU_DESCRIPTOR_HANDLE cpuRenderTarget = m_pRtvHeap->GetCpuDescriptorHandle(m_currentBackBufferIndex);
	GetCommandList()->OMSetRenderTargets(1, &cpuRenderTarget, false, nullptr);
	GetCommandList()->RSSetViewports(1, &m_renderViewport);
	GetCommandList()->RSSetScissorRects(1, &m_renderScissor);
	GetCommandList()->SetGraphicsRootSignature(m_pFinalRootSignature);
	GetCommandList()->SetPipelineState(m_pFinalPipelineState);
	GetCommandList()->SetGraphicsRoot32BitConstants(0, static_cast<uint32>(sizeof(CameraInfo) / 4), &cameraInfo, 0);
	GetCommandList()->SetGraphicsRootDescriptorTable(1, m_pCopyDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyIndex));
	GetCommandList()->IASetVertexBuffers(0, 1, &GetVertexBufferView());					// 頂点情報を送信
	GetCommandList()->IASetIndexBuffer(&GetIndexBufferView());							// インデックス情報を送信
	GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);      // 3角ポリゴンのみ
	GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(GetRtvResource(m_currentBackBufferIndex), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	GetCommandList()->ResourceBarrier(1, &barrier);
}

void DirectXDevice::BeginFrame()
{
	// フレーム開始時に実行
	WaitRender(false, true);

#if _DEBUG
	PIXBeginEvent(GetCommandList(), PIX_COLOR(255, 0, 0), L"Shadow");
#endif
}

void DirectXDevice::ExecuteCommandList()
{
#if _DEBUG
	PIXEndEvent(GetCommandList());
#endif

	GetCommandList()->Close();

	ID3D12CommandList* ppCmdLists[] = { GetCommandList() };
	m_pQueue->ExecuteCommandLists(1, ppCmdLists);

	// スワップチェーンを切り替え
	HRESULT hr = m_pSwapChain->Present(1, 0);
	if (FAILED(hr)) {
		printf("Presentに失敗しました。\n");
	}

	m_globalFenceValue++;
	hr = m_pQueue->Signal(m_pFence.Get(), m_globalFenceValue);
	if (FAILED(hr)) {
		printf("Signalに失敗しました。\n");
	}

	m_frameResources[m_currentBackBufferIndex].m_fenceValue = m_globalFenceValue;
}

bool DirectXDevice::CreateDevice()
{
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_pDxgiFactory));

	if (FAILED(hr)) {
		return false;
	}

	// レイトレーシングをサポートするGPUを取得
	ComPtr<IDXGIAdapter1> adapter;
	for (uint32 i = 0; ; ++i) {
		hr = m_pDxgiFactory->EnumAdapters1(i, &adapter);
		if (FAILED(hr)) {
			break;
		}

		// レイトレーシング機能を持つアダプタか確認
		hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_pDevice));

		if (SUCCEEDED(hr)) {
			// レイトレーシングをサポートしているか確認
			D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
			hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));

			if (SUCCEEDED(hr) && options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
				return true;
			}
		}
	}

	return false;
}

bool DirectXDevice::CreateCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	HRESULT hr = m_pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(m_pQueue.ReleaseAndGetAddressOf()));

	if (FAILED(hr)) {
		return false;
	}

	desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

	hr = m_pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_pComputeQueue));

	return SUCCEEDED(hr);
}

bool DirectXDevice::CreateSwapChain()
{
	// DXGIファクトリーの生成
	ComPtr<IDXGIFactory2> pFactory;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
	if (FAILED(hr))
	{
		return false;
	}

	// スワップチェインの生成
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = m_nowWindowSize.x;
	desc.Height = m_nowWindowSize.y;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = FRAME_BUFFER_COUNT;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	// スワップチェインの生成
	ComPtr<IDXGISwapChain1> swapChain1;

	hr = pFactory->CreateSwapChainForHwnd(m_pQueue.Get(), m_hWnd, &desc, nullptr, nullptr, &swapChain1);

	hr = swapChain1.As(&m_pSwapChain);
	if (FAILED(hr))
		return false;

	// バックバッファ番号を取得
	m_currentBackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	return true;
}

bool DirectXDevice::CreateCommandList()
{
	HRESULT hr;
	for (uint32 i = 0; i < FRAME_BUFFER_COUNT; i++) {
		// コマンドアロケーターの作成
		hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_frameResources[i].m_pAllocator.ReleaseAndGetAddressOf()));

		if (FAILED(hr)) {
			printf("アロケータの作成に失敗しました。\n");
			return false;
		}

		// 標準のグラフィックスコマンドリストを作成
		hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_frameResources[i].m_pAllocator.Get(), nullptr, IID_PPV_ARGS(&m_frameResources[i].m_pCommandList));
		if (FAILED(hr)) {
			printf("コマンドリストの作成に失敗しました。\n");
			return false;
		}

		m_frameResources[i].m_pAllocator->SetName(std::wstring(L"m_frameResource[" + std::to_wstring(i) + L"].m_pAllocator").c_str());

		// コマンドリストは開かれている状態で作成されるので、いったん閉じる。
		m_frameResources[i].m_pCommandList->Close();
	}

	// Compute用アロケータ
	hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_pComputeAllocator));
	if (FAILED(hr)) {
		return false;
	}
	m_pComputeAllocator->SetName(L"m_pComputeAllocator");

	// Compute用コマンドリスト
	hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_pComputeAllocator.Get(), nullptr, IID_PPV_ARGS(&m_pComputeCommandList));
	if (FAILED(hr)) {
		return false;
	}
	m_pComputeAllocator->SetName(L"m_pComputeCommandList");
	m_pComputeCommandList->Close();

	// コマンドを初期化してためる準備をする
	m_pComputeAllocator->Reset();
	m_pComputeCommandList->Reset(m_pComputeAllocator.Get(), nullptr);

	return true;
}

bool DirectXDevice::CreateFence()
{
	HRESULT hr = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_pFence.ReleaseAndGetAddressOf()));
	if (FAILED(hr)) {
		return false;
	}
	hr = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_pComputeFence.ReleaseAndGetAddressOf()));
	if (FAILED(hr)) {
		return false;
	}

	m_globalFenceValue++;
	m_computeFenceValue++;

	// 同期を行うときのイベントハンドラを作成する。
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	m_computeFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	return m_fenceEvent != nullptr;
}

void DirectXDevice::CreateViewScissor()
{
	m_renderViewport.TopLeftX = 0;
	m_renderViewport.TopLeftY = 0;
	m_renderViewport.Width = static_cast<float>(m_nowWindowSize.x);
	m_renderViewport.Height = static_cast<float>(m_nowWindowSize.y);
	m_renderViewport.MinDepth = 0.0f;
	m_renderViewport.MaxDepth = 1.0f;

	m_renderScissor.left = 0;
	m_renderScissor.right = static_cast<int>(m_nowWindowSize.x);
	m_renderScissor.top = 0;
	m_renderScissor.bottom = static_cast<int>(m_nowWindowSize.y);

}

bool DirectXDevice::CreateDepthStencil()
{
	if (!m_pDsvHeap)
	{
		// DSV用のディスクリプタヒープを作成する
		m_pDsvHeap = CreateObjectPtr<DescriptorHeap>();
		m_pDsvHeap->Initialize(m_pDevice.Get(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 100);
	}

	return true;
}

bool DirectXDevice::CreateRenderTarget()
{
	// RTV用のディスクリプタヒープを作成
	m_pRtvHeap = CreateObjectPtr<DescriptorHeap>();
	m_pRtvHeap->Initialize(m_pDevice.Get(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 100);

	// ディスクリプタのサイズを取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvHeap->GetCpuDescriptorHandle(0);
	// スワップチェーンのバックバッファを取得
	for (uint32 i = 0; i < FRAME_BUFFER_COUNT; i++) {
		HRESULT r = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets[i]));
		m_pDevice->CreateRenderTargetView(m_pRenderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += m_pRtvHeap->GetHeapSize();

		std::wstring name = L"m_pRenderTargets[" + std::to_wstring(i) + L"]";
		m_pRenderTargets[i]->SetName(name.c_str());

		m_pRtvHeap->GetNextAvailableDescriptor();	// 他で使われないように予約
	}

	return true;
}

void DirectXDevice::CreateVertexBuffer()
{
	// -1.0f～1.0fのNDC座標
	UIVertex vertices[] =
	{
		{ {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} }, // 左上
		{ { 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f} }, // 右上
		{ {-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f} }, // 左下
		{ { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} }  // 右下
	};

	const uint32 bufferSize = sizeof(vertices);

	// DEFAULTヒープ（GPU専用）
	CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

	HRESULT hr = m_pDevice->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer)
	);

	if (FAILED(hr)) {
		printf("DEFAULT 頂点バッファの生成に失敗しました。%1xl\n", hr);
		return;
	}
	m_vertexBuffer->SetName(L"UIManager::m_vertexBuffer");

	// 一時UPLOADバッファ
	ID3D12Resource* pUploadBuffer = ResourceStatics::CreateUploadResource(bufferSize);

	// UPLOADバッファに書き込み
	void* vertexDataBegin;
	pUploadBuffer->Map(0, nullptr, &vertexDataBegin);
	memcpy(vertexDataBegin, vertices, bufferSize);
	pUploadBuffer->Unmap(0, nullptr);

	// リソースのコピーは非同期で行い、完了したらbufferViewに情報を入れる
	CopyJob copyJob{};
	copyJob.func = [this, bufferSize, pUploadBuffer](ID3D12GraphicsCommandList* commandList) {
		// バリアを設定
		D3D12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier2);

		// コマンドでDEFAULTにコピー
		commandList->CopyBufferRegion(m_vertexBuffer.Get(), 0, pUploadBuffer, 0, bufferSize);
		};

	copyJob.OnComplete = [this, bufferSize, pUploadBuffer]() {
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		GetCommandList()->ResourceBarrier(1, &barrier);

		// VertexBufferView設定
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(UIVertex);
		m_vertexBufferView.SizeInBytes = bufferSize;

		pUploadBuffer->Release();
		};

	LockMutex lock(g_copyJobManager->m_mutex);
	g_copyJobManager->m_jobs.push_back(std::move(copyJob));
	g_copyJobManager->m_cv.notify_one();
}

void DirectXDevice::CreateIndexBuffer()
{
	uint16 indices[] = { 0, 1, 2, 2, 1, 3 };

	// インデックスバッファの作成
	const uint32 indexBufferSize = sizeof(indices);

	// DEFAULTヒープ（GPU専用）
	CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

	HRESULT hr = m_pDevice->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&indexBufferDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&m_indexBuffer)
	);

	if (FAILED(hr)) {
		printf("DEFAULT インデックスバッファの生成に失敗しました。\n");
		return;
	}
	m_indexBuffer->SetName(L"UIManager::m_indexBuffer");

	// 一時UPLOAD
	ID3D12Resource* pUploadBuffer = ResourceStatics::CreateUploadResource(indexBufferSize);

	// インデックスデータをGPUに送信
	void* indexDataBegin;
	pUploadBuffer->Map(0, nullptr, &indexDataBegin);
	memcpy(indexDataBegin, indices, indexBufferSize);
	pUploadBuffer->Unmap(0, nullptr);

	// リソースのコピーは非同期で行い、完了したらbufferViewに情報を入れる
	CopyJob copyJob{};
	copyJob.func = [this, indexBufferSize, pUploadBuffer](ID3D12GraphicsCommandList* commandList) {
		D3D12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier2);

		// DEFAULTヒープへコピー
		commandList->CopyBufferRegion(m_indexBuffer.Get(), 0, pUploadBuffer, 0, indexBufferSize);
		};

	copyJob.OnComplete = [this, indexBufferSize, pUploadBuffer]() {
		if (!this) {
			return;
		}

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_indexBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_INDEX_BUFFER
		);
		GetCommandList()->ResourceBarrier(1, &barrier);

		// IndexBufferView設定
		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
		m_indexBufferView.SizeInBytes = indexBufferSize;

		pUploadBuffer->Release();
		};

	LockMutex lock(g_copyJobManager->m_mutex);
	g_copyJobManager->m_jobs.push_back(std::move(copyJob));
	g_copyJobManager->m_cv.notify_one();
}

bool DirectXDevice::CreateCameraPipeline()
{
	// RootSignatureの設定
	const uint32 rootParamCount = 2;
	CD3DX12_ROOT_PARAMETER rootParameters[rootParamCount]{};
	// UIConstants(b0)を設定。 XMFLOAT2が6つ、XMMATRIXが2つあるため48
	rootParameters[0].InitAsConstants(sizeof(CameraInfo) / 4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	// テクスチャ用をスロットt0を設定
	CD3DX12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1 + MAX_CAMERA_COUNT, 0);
	rootParameters[1].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

	// PipelineStateの設定
	const std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements = GetBasicVertexLayout();
	CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// PsoDescKeyの設定
	PsoDescKey desc{};
	desc.cullMode = D3D12_CULL_MODE_BACK;
	desc.textureAddressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc.vertexShaderPath = KeyString::SHADER_MAINRENDER_VS;
	desc.pixelShaderPath = KeyString::SHADER_MAINRENDER_PS;
	desc.depthStencilPreset = DepthStencilPreset_UI;
	desc.numRenderTargets = 1;
	desc.rtvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.bAlphaBlend = true;

	uint64 shaderHashID = GetShaderHash(desc.vertexShaderPath, desc.pixelShaderPath);
	uint64 temp;

	// パイプラインの作成。ポインタで返すが、ゲーム終了時に自動で解放される
	m_pFinalRootSignature = RootSignature::CreateRootSignature(m_pDevice.Get(), shaderHashID, rootParameters, rootParamCount, &sampler, 1);
	m_pFinalPipelineState = PipelineState::CreatePipelineState(m_pDevice.Get(), inputElements.data(), static_cast<uint32>(inputElements.size()), shaderHashID, desc, temp);

	return m_pFinalRootSignature && m_pFinalPipelineState;
}

void DirectXDevice::WaitRender(bool bWaitAllFrame, bool bResetCommandList)
{
	m_currentBackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	FrameGPUBuffer& fr = m_frameResources[m_currentBackBufferIndex];

	if (bWaitAllFrame) {
		for (FrameGPUBuffer& buffer : m_frameResources)
		{
			if (m_pFence->GetCompletedValue() < buffer.m_fenceValue)
			{
				m_pFence->SetEventOnCompletion(buffer.m_fenceValue, m_fenceEvent);
				WaitForSingleObject(m_fenceEvent, INFINITE);
			}
		}
	}
	else
	{
		// このフレームリソースが使用中かどうか
		if (m_pFence->GetCompletedValue() < fr.m_fenceValue)
		{
			m_pFence->SetEventOnCompletion(fr.m_fenceValue, m_fenceEvent);
			WaitForSingleObject(m_fenceEvent, INFINITE);
		}
	}

	if (bResetCommandList)
	{
		// GPUがこのフレームリソースを使い終わったのでResetできる
		fr.m_pAllocator->Reset();
		fr.m_pCommandList->Reset(fr.m_pAllocator.Get(), nullptr);
	}
}
