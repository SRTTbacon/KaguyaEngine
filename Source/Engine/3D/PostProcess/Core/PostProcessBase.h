//*****************************************************************************
//
// ポストプロセスの基底クラス
//
// PostProcessBase.h
//
// T_Takeuchi
//
// 2026/8/16
//
//*****************************************************************************

#pragma once

#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\EngineDefine.h"
#include "..\\..\\..\\Core\\FrameBuffer.h"

namespace KaguyaEngine
{
	class Camera;
	struct CameraResource;
	namespace Core
	{
		class DescriptorHeap;
		class PipelineState;
	}
}

namespace KaguyaEngine::PostProcess
{
	class PostProcessManager;

	struct PPResource
	{
		ComPtr<ID3D12Resource> pResource;		// リソース
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;	// リソースのm_pRtvHeapのRTVハンドル
		uint32 mainSrvIndex;					// リソースのpMainHeapのSRVインデックス
		uint32 mainUavIndex;					// リソースのpMainHeapのUAVインデックス

		PPResource() : rtvHandle(D3D12_CPU_DESCRIPTOR_HANDLE()), mainSrvIndex(UINT32_MAX), mainUavIndex(UINT32_MAX) {}
	};

	class PostProcessBase : public FrameResource
	{
		friend PostProcessManager;
	public:
		PostProcessBase(uint32 postProcessID, const KaguyaEngine::Camera* pCamera, KaguyaEngine::Core::DescriptorHeap* pRtvHeap, ID3D12RootSignature* pBaseRootSignature);
		~PostProcessBase();

		// ポストプロセスを初期化
		// カメラの解像度が変わったときにも実行される
		virtual void Initialize(bool bFirstInit) = 0;

		// ポストプロセスを実行
		// 引数 : 1つ前のレンダリング結果
		virtual void Render(RenderResource& mainRenderTarget) = 0;

		virtual RenderResource GetResultRenderTarget() const;

	public:
		bool m_bEnable;

		inline uint32 GetPostProcessID() const { return m_postProcessID; }
		inline const KaguyaEngine::Camera* GetCamera() const { return m_pCamera; }

	protected:
		KaguyaEngine::Core::DescriptorHeap* m_pRtvHeap;	// レンダーターゲット用のディスクリプタヒープ
		ID3D12RootSignature* m_pBaseRootSignature;		// 基本的なポストプロセス用のベースとなるルートシグネチャ

		PPResource m_mainRenderTarget;	// 最終出力用のレンダーターゲット

		const KaguyaEngine::CameraResource* m_pCameraResource;

		const KaguyaEngine::Camera* m_pCamera;

		const uint32 m_postProcessID;
		uint32 m_copyDescriptorFirstIndex;	// m_pDescriptor->pCopyHeapの中で、このポストプロセスが使用する最初のディスクリプタのインデックス

		uint32 m_rtvStartIndex;	// m_pRtvHeapの中で、このポストプロセスが使用する最初のレンダーターゲットのインデックス
		uint32 m_srvStartIndex;	// m_pDescriptor->pMainHeapの中で、このポストプロセスが使用する最初のSRVのインデックス

		uint8 m_depth;

		void CreateRenderTexture(const DirectX::XMFLOAT4& clearColor, PPResource& resource, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle, DXGI_FORMAT format, float division, 
			bool bCreateRTV = true, std::string name = "");

		void ChangeBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
		void ChangeBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, std::vector<D3D12_RESOURCE_BARRIER>& barriers);

		void SetViewportAndScissor(const DirectX::XMUINT2& viewportSize);
		void CopyDescriptors(uint32 count, uint32 srcIndex, uint32 destIndex) const;
		void CopyDescriptors(uint32 count, D3D12_CPU_DESCRIPTOR_HANDLE& src, uint32 destIndex) const;

		virtual void SetSRVResource(PPResource& resource, uint32 srvMainIndex);
		virtual void SetUAVResource(PPResource& resource, uint32 uavMainIndex);
		virtual void Draw(const D3D12_GPU_DESCRIPTOR_HANDLE& srvCopyGPUHandle = D3D12_GPU_DESCRIPTOR_HANDLE());
	};
}
