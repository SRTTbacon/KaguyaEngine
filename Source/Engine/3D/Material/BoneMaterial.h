//*****************************************************************************
//
// ボーンが存在するモデル用のマテリアル
//
// BoneMaterial.h
//
// T_Takeuchi
//
// 2026/6/22
//
//*****************************************************************************

#pragma once

#include "Core\\MaterialBase.h"
#include "..\..\Core\FrameBuffer.h"
#include "..\..\Core\CoreMinimal.h"

#include <string>

namespace KaguyaEngine::Model
{
	class Mesh;
}

namespace KaguyaEngine::Material
{
	class BoneMaterial : public MaterialBase
	{
	public:
		BoneMaterial(FrameResource* pFrameResource);
		BoneMaterial(FrameResource* pFrameResource, std::string vertexShaderPath, std::string pixelShaderPath, uint32 textureCount);

		virtual void LateUpdate();
		virtual void DrawMesh(class KaguyaEngine::Model::Mesh* pMesh) override;

	protected:
		virtual void CreatePipeline() override;
		virtual void CreateUniquePSBuffer() override;

	protected:
		struct ShapeInfo
		{
			uint32 shapeCount;
		};

	protected:
		UniquePSBuffer m_materialBuffer;

		// このマテリアル専用のルートパラメーターの開始インデックス
		uint8 m_rootParameterIndex;
	};
}
