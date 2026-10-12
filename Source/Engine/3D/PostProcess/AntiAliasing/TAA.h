//*****************************************************************************
//
// アンチエイリアシング : TAA (Temporal Anti-Aliasing)
//
// TAA.h
//
// T_Takeuchi
//
// 2026/9/27
//
//*****************************************************************************

#pragma once

#include "..\\Core\\PostProcessBase.h"

namespace KaguyaEngine::PostProcess
{
	class PTAA : public PostProcessBase
	{
	public:
		PTAA(uint32 postProcessID, const KaguyaEngine::Camera* pCamera, KaguyaEngine::Core::DescriptorHeap* pRtvHeap, ID3D12RootSignature* pRootSignature);

		// ポストプロセスを初期化
		// カメラの解像度が変わったときにも実行される
		virtual void Initialize(bool bFirstInit) override;

		// ポストプロセスを実行
		// 引数 : 1つ前のレンダリング結果
		virtual void Render(RenderResource& mainRenderTarget) override;
		virtual RenderResource GetResultRenderTarget() const override;

	protected:
		/*
		struct CBTAAResolve
		{
			DirectX::XMFLOAT2 Resolution; // 1/width, 1/height
			DirectX::XMFLOAT4 Jitter;	// CurrentJitter, PreviousJitter
			uint32 FrameNumber;
			uint32 DebugFlags; // AllowLongestVelocityVector | AllowNeighbourhoodSampling | AllowYCoCg | AllowVarianceClipping | AllowBicubicFilter | AllowDepthThreshold | MarkNoHistoryPixels
		};
		*/
		struct CBTAAResolve
		{
			DirectX::XMFLOAT4 Resolution; // width, height, 1/width, 1/height
			DirectX::XMFLOAT2 Jitter;
			uint32 FrameNumber;
			uint32 DebugFlags; // AllowLongestVelocityVector | AllowNeighbourhoodSampling | AllowYCoCg | AllowVarianceClipping | AllowBicubicFilter | AllowDepthThreshold | MarkNoHistoryPixels
		};

	protected:
		PPResource m_historyRenderTarget[2];

		CBTAAResolve m_cbTAAResolve;

		ID3D12RootSignature* m_pTAARootSignature;
		ID3D12PipelineState* m_pTAAPipelineState;

		uint8 m_currentHistoryIndex;
	};
}
