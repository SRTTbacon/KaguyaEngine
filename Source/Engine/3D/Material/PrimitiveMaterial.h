//*****************************************************************************
//
// 普通の3Dオブジェクト用のマテリアルクラス
// 独自のシェーダーを用いる場合は別のマテリアルクラスを用意するのが良い
//
// PrimitiveMaterial.h
//
// T_Takeuchi
//
// 2026/6/10
//
//*****************************************************************************

#pragma once

#include "Core\\MaterialBase.h"
#include "..\..\Core\FrameBuffer.h"
#include "..\..\Core\CoreMinimal.h"

#include <string>

namespace KaguyaEngine::Material
{
	class PrimitiveMaterial : public MaterialBase
	{
	public:
		PrimitiveMaterial(FrameResource* pFrameResource);
		PrimitiveMaterial(FrameResource* pFrameResource, std::string vertexShaderPath, std::string pixelShaderPath, uint8 textureCount);

	protected:
		virtual void CreatePipeline() override;
		virtual void CreateUniquePSBuffer() override;

	protected:
		UniquePSBuffer m_materialBuffer;
	};
}
