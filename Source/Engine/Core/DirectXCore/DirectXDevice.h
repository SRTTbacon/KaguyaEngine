//*****************************************************************************
//
// DirectX12のデバイス管理
// このクラスはEngine.hで作成されるため、インスタンスを作成する必要はありません。
//
// DirectXDevice.h
//
// T_Takeuchi
//
// 2026/5/24
//
//*****************************************************************************

#pragma once

#include "..\\CoreMinimal.h"
#include "..\\FrameBuffer.h"
#include "..\\EngineDefine.h"

#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <Windows.h>
#include <vector>

namespace KaguyaEngine
{
	namespace Core
	{
		class DescriptorHeap;
		struct CopyDescriptor;
	}

	class Camera;
}

class DirectXDevice
{
public:
	DirectXDevice(HWND hwnd);
	~DirectXDevice();

	// デバイスを作成
	bool CreateDevice(DirectX::XMUINT2 windowSize);

	void UpdateRenderTargets(KaguyaEngine::Core::CopyDescriptor* pCopyDescriptor, const std::vector<KaguyaEngine::Camera*>& pCameraList, ID3D12Resource* pUIResource);

	// 3D描画、UIなどを合わせて最終描画を行う
	void FinalRender();

	void BeginFrame();
	void ExecuteCommandList();

public:
	inline ID3D12Device6* GetDevice() const { return m_pDevice.Get(); }
	inline ID3D12CommandQueue* GetQueue() const { return m_pQueue.Get(); }
	inline ID3D12GraphicsCommandList4* GetCommandList() const { return m_frameResources[m_currentBackBufferIndex].m_pCommandList.Get(); }
	inline FrameGPUBuffer* GetFrameResources() { return m_frameResources; }
	inline ID3D12Resource* GetRtvResource(int index) const { return m_pRenderTargets[index].Get(); }
	inline KaguyaEngine::Core::DescriptorHeap* GetRtvHeap() const { return m_pRtvHeap.get(); }
	inline KaguyaEngine::Core::DescriptorHeap* GetDsvHeap() const { return m_pDsvHeap.get(); }
	inline ID3D12Resource* GetVertexBuffer() const { return m_vertexBuffer.Get(); }
	inline ID3D12Resource* GetIndexBuffer() const { return m_indexBuffer.Get(); }
	inline D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() { return m_vertexBufferView; }
	inline D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() { return m_indexBufferView; }
	inline D3D12_VIEWPORT& GetRenderViewport() { return m_renderViewport; }
	inline D3D12_RECT& GetRenderScissor() { return m_renderScissor; }
	inline const int* GetCurrentBackBufferIndex() const { return &m_currentBackBufferIndex; }

private:
	struct UIVertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT2 uv;
	};

	struct CameraInfo
	{
		DirectX::XMFLOAT4 cameras[5]; // 将来の拡張用に少し余裕を持たせる
		uint32 cameraCount;
	};

private:	// -----DirectX関連の初期化-----
	// デバイスを生成
	bool CreateDevice();
	// コマンドキューを生成
	bool CreateCommandQueue();
	// スワップチェインを生成 (トリプルバッファリング : 画面のちらつきを抑える)
	bool CreateSwapChain();
	// コマンドリストとコマンドアロケーターを生成
	bool CreateCommandList();
	// フェンスを生成
	bool CreateFence();
	// ビューポートを生成
	void CreateViewScissor();
	// デプスステンシルバッファを生成
	bool CreateDepthStencil();
	// レンダーターゲットを生成
	bool CreateRenderTarget();

	void CreateVertexBuffer();
	void CreateIndexBuffer();

	bool CreateCameraPipeline();

private:	// -----レンダリングに関連する関数-----
	void WaitRender(bool bWaitAllFrame, bool bResetCommandList);

private:	// -----エンジン関連の変数-----
	ComPtr<ID3D12Device6> m_pDevice;			//デバイス
	ComPtr<ID3D12CommandQueue> m_pQueue;		//コマンドキュー
	ComPtr<ID3D12CommandQueue> m_pComputeQueue;	//Compute用のコマンドキュー

	ComPtr<IDXGISwapChain3> m_pSwapChain;	//スワップチェイン

	ComPtr<IDXGIFactory6> m_pDxgiFactory;	//ファクトリ(GPUの解析)

	ComPtr<ID3D12CommandAllocator> m_pComputeAllocator;			//Compute用のアロケーター
	ComPtr<ID3D12GraphicsCommandList> m_pComputeCommandList;	//Compute用のコマンドリスト

	ObjectPtr<KaguyaEngine::Core::DescriptorHeap> m_pRtvHeap;	//RTV用のディスクリプタヒープ
	ObjectPtr<KaguyaEngine::Core::DescriptorHeap> m_pDsvHeap;	//DSV用のディスクリプタヒープ

	ComPtr<ID3D12Debug1> m_pDebugController;		//デバッグコントローラー

	ComPtr<ID3D12Fence> m_pFence;			//フェンス
	ComPtr<ID3D12Fence> m_pComputeFence;	//Computeシェーダー用フェンス

	FrameGPUBuffer m_frameResources[FRAME_BUFFER_COUNT];

private:	// -----レンダリングに関連する変数-----
	ComPtr<ID3D12Resource> m_pRenderTargets[FRAME_BUFFER_COUNT];	//レンダーターゲット

private:  // -----ウィンドウに関連する変数-----
	HWND m_hWnd;

	ComPtr<ID3D12Resource> m_vertexBuffer;
	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;      //頂点バッファのデータ内容とサイズを保持
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;        //インデックスバッファのデータ内容とサイズを保持

	ID3D12RootSignature* m_pFinalRootSignature;
	ID3D12PipelineState* m_pFinalPipelineState;

	KaguyaEngine::Core::CopyDescriptor* m_pCopyDescriptor;
	std::vector<KaguyaEngine::Camera*> m_pCameraList; 
	ID3D12Resource* m_pUIResource;

	uint32 m_uiMainSrvIndex;

	HANDLE m_fenceEvent = nullptr;				//フェンスで使うイベント
	HANDLE m_computeFenceEvent = nullptr;		//Compute用のフェンスで使うイベント
	uint64 m_globalFenceValue;					//フェンスの値
	uint64 m_computeFenceValue;					//Compute用のフェンスの値
	D3D12_VIEWPORT m_renderViewport;			//3Dビューポート
	D3D12_RECT m_renderScissor;					//シザー矩形

	DirectX::XMUINT2 m_nowWindowSize;

	int m_currentBackBufferIndex;				//現在のバックバッファのインデックス
};