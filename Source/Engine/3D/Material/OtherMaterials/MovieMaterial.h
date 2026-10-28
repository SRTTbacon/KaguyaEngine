//*****************************************************************************
//
// NV12テクスチャを描画するマテリアル
//
// MovieMaterial.h
//
// T_Takeuchi
//
// 2026/10/14
//
//*****************************************************************************

#pragma once

#include "..\\Core\\MaterialBase.h"

namespace KaguyaEngine::Material
{
	class MovieMaterial : public MaterialBase
	{
	public:
		MovieMaterial(FrameResource* pFrameResource);
		MovieMaterial(FrameResource* pFrameResource, std::string vertexShaderPath, std::string pixelShaderPath, uint8 textureCount);

		// マテリアルの初期化。 ユーザーが必ず実行させる
		// 引数 : cullMode 両面描画または片面描画, bTransparentMode 半透明をサポートするかどうか
		virtual void Initialize(D3D12_CULL_MODE cullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_FRONT, bool bTransparentMode = false) override;

		virtual void SetNV12Texture(ID3D12Resource* pNV12Texture);

		virtual void LateUpdate() override;
		virtual void Draw() override;

	public:
		inline uint32 GetMovieTextureMainSrvIndex() const { return movieTextureMainSrvIndex; }

	protected:
		virtual void CreatePipeline() override;
		virtual void CreateUniquePSBuffer() override;

	protected:
		UniquePSBuffer m_materialBuffer;

		ID3D12Resource* m_pNV12Texture;

		uint32 movieTextureMainSrvIndex;
	};
}
