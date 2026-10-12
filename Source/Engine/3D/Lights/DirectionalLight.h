//*****************************************************************************
//
// ディレクショナルライト(環境光)を管理するクラス
//
// DirectionalLight.h
//
// T_Takeuchi
//
// 2026/6/20
//
//*****************************************************************************

#pragma once

#include "..\\Transform\\Transform.h"

#include <DirectXMath.h>

namespace KaguyaEngine::Light
{
	class DirectionalLight : public Transform
	{
	public:
		DirectionalLight();

		void LateUpdate();

	public:
		inline DirectX::XMMATRIX GetViewProj() const { return m_lightViewProj; }
		DirectX::XMFLOAT4 GetLightDirection() const;

	public:
		DirectX::XMFLOAT4 m_diffuseColor;		// ライトカラー
		DirectX::XMFLOAT4 m_ambientColor;		// 影カラー
		DirectX::XMFLOAT4 m_fogColor;			// フォグカラー
		DirectX::XMFLOAT2 m_fogStartEnd;		// フォグの開始距離、終了距離
		float m_shadowDistance;
		float m_shadowScale;

	private:
		//影の位置を更新する間隔 (常に更新するとセルフシャドウがちらつくため)
		static const float SHADOW_GRID_SIZE;

		DirectX::XMMATRIX m_lightViewProj;	// ディレクショナルライトのビューマトリクス
	};
}
