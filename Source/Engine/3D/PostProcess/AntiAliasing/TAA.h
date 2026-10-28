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
		PPResource m_historyRenderTarget[2];

		ID3D12RootSignature* m_pTAARootSignature;
		ID3D12PipelineState* m_pTAAPipelineState;

		uint8 m_currentHistoryIndex;
	};
}
