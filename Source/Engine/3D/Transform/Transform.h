//*****************************************************************************
//
// 3DTransformクラス
// 3D関係のオブジェクトの基底クラス
//
// Transform.h
//
// T_Takeuchi
//
// 2026/6/2
//
//*****************************************************************************

#pragma once

#include "..\\..\\Core\\FrameBuffer.h"

#include <DirectXMath.h>

namespace KaguyaEngine
{
	class Transform : public FrameResource
	{
	public:
		Transform();
		virtual ~Transform() = default;		//dynamic_cast用の仮想関数

		//デグリー角で角度を指定
		void SetRotation(float x, float y, float z);

		// スケール(x,y,z)を変更
		void SetScale(float scale);

		const DirectX::XMFLOAT3 GetDegreeRotation() const;
		const DirectX::XMFLOAT4 GetQuaternion() const;

		void AddTranslation(float x, float y, float z);
		void AddRotationX(float x);
		void AddRotationY(float y);
		void AddRotationZ(float z);

		DirectX::XMFLOAT3 GetForward() const;
		DirectX::XMFLOAT3 GetRight() const;
		DirectX::XMFLOAT3 GetUp() const;

	public:
		DirectX::XMFLOAT3 m_position;	//位置
		DirectX::XMFLOAT4 m_rotation;	//ラジアン角
		DirectX::XMFLOAT3 m_scale;		//スケール

		void* m_pUserData;

		bool m_bVisible;        //描画するかどうか
		bool m_bInCamera;		//カメラに映っているかどうか (ModelManager.cppで制御)
	};
}
