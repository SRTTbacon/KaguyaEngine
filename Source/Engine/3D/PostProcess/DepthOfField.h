//*****************************************************************************
//
// 被写体深度のポストプロセス
//
// DepthOfField.h
//
// T_Takeuchi
//
// 2026/8/18
//
//*****************************************************************************

#pragma once

#include "..\\..\\Core\\CoreMinimal.h"
#include "Core\\PostProcessBase.h"

namespace KaguyaEngine::PostProcess
{
	struct DoFSettings
	{
		float focusDistance;	// ピントを合わせたい距離（リニアZ）
		float focusRange;		// どれだけボケるかの許容範囲
		float maxCoC;			// 最大ボケ半径（ピクセル単位）
		float nearZ;
		float farZ;
	};

	class PDepthOfField : public PostProcessBase
	{
	public:
		PDepthOfField(uint32 postProcessID, const KaguyaEngine::Camera* pCamera, KaguyaEngine::Core::DescriptorHeap* pRtvHeap, ID3D12RootSignature* pRootSignature);

		// ポストプロセスを初期化
		// カメラの解像度が変わったときにも実行される
		virtual void Initialize(bool bFirstInit) override;

		// ポストプロセスを実行
		// 引数 : 1つ前のレンダリング結果
		virtual void Render(RenderResource& mainRenderTarget) override;

		virtual inline RenderResource GetResultRenderTarget() const override;

	public:
		DoFSettings m_dofSettings;

	protected:
		struct BlurParams
		{
			DirectX::XMFLOAT2 texelSize;
			float blurScale;
		};

		BlurParams m_blurParams;

		PPResource m_cocRenderTarget;
		PPResource m_downScaleRenderTarget;
		PPResource m_blurRenderTarget;

		ObjectPtr<class PostProcessSampling> m_pPostProcessSampling;
		ID3D12PipelineState* m_pCoCPipelineState;
		ID3D12PipelineState* m_pDoFBlurPipelineState;
		ID3D12PipelineState* m_pDoFPipelineState;
	};
}
