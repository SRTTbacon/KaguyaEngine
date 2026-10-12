//*****************************************************************************
//
// ポストプロセスのアップサンプリング / ダウンサンプリングを行うクラス
//
// PostProcessSampling.h
//
// T_Takeuchi
//
// 2026/8/16
//
//*****************************************************************************

#pragma once

#include "..\\..\\..\\Core\\FrameBuffer.h"

#include <d3dx12.h>
#include <DirectXMath.h>

namespace KaguyaEngine::PostProcess
{
	class PostProcessSampling : public FrameResource
	{
	public:
		PostProcessSampling();
		~PostProcessSampling();

		void Initialize(DXGI_FORMAT rtvFormat, ID3D12RootSignature* pRootSignature);

		void ResetRenderIndex(UINT initIndex);

		void BeginUpSample(float intensity);
		void DrawUpSample(D3D12_CPU_DESCRIPTOR_HANDLE& handle, ID3D12Resource* pRenderTarget, D3D12_CPU_DESCRIPTOR_HANDLE& lowTextureMainSrvHandle, D3D12_CPU_DESCRIPTOR_HANDLE& highTextureMainSrvHandle);

		void BeginDownSample();
		void DrawDownSample(D3D12_CPU_DESCRIPTOR_HANDLE& handle, ID3D12Resource* pRenderTarget, D3D12_GPU_DESCRIPTOR_HANDLE& highTextureCopySrvHandle, DirectX::XMUINT2 renderSize);

	public:
		UINT m_rtvDescriptorSize;

	private:
		struct DownSampleStruct
		{
			DirectX::XMFLOAT2 texelSize;
		};

		ID3D12PipelineState* m_pUpSamplePipelineState;
		ID3D12PipelineState* m_pDownSamplePipelineState;

		UINT m_tempRenderIndex;
	};
}
