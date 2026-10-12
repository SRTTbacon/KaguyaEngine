#include "Transform.h"

#include <DirectXMath.h>
#include <DirectXMathMatrix.inl>
#include <DirectXMathConvert.inl>
#include <DirectXMathMisc.inl>

using namespace DirectX;

namespace KaguyaEngine
{
	Transform::Transform()
		: m_position(XMFLOAT3(0.0f, 0.0f, 0.0f))
		, m_rotation(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f))
		, m_scale(XMFLOAT3(1.0f, 1.0f, 1.0f))
		, m_pUserData(nullptr)
		, m_bVisible(true)
		, m_bInCamera(true)
	{
	}

	void Transform::SetRotation(float x, float y, float z)
	{
		m_rotation.x = XMConvertToRadians(x);
		m_rotation.y = XMConvertToRadians(y);
		m_rotation.z = XMConvertToRadians(z);
	}

	void Transform::SetScale(float scale)
	{
		m_scale = XMFLOAT3(scale, scale, scale);
	}

	const XMFLOAT3 Transform::GetDegreeRotation() const
	{
		return XMFLOAT3(XMConvertToDegrees(m_rotation.x), XMConvertToDegrees(m_rotation.y), XMConvertToDegrees(m_rotation.z));
	}

	const XMFLOAT4 Transform::GetQuaternion() const
	{
		XMFLOAT4 result{};
		XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
		XMStoreFloat4(&result, quaternion);
		return result;
	}

	void Transform::AddTranslation(float x, float y, float z)
	{
		m_position.x += x;
		m_position.y += y;
		m_position.z += z;
	}

	void Transform::AddRotationX(float x)
	{
		m_rotation.x += XMConvertToRadians(x);
	}
	void Transform::AddRotationY(float y)
	{
		m_rotation.y += XMConvertToRadians(y);
	}
	void Transform::AddRotationZ(float z)
	{
		m_rotation.z += XMConvertToRadians(z);
	}

	XMFLOAT3 Transform::GetForward() const
	{
		XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
		XMFLOAT3 result = XMFLOAT3(rotation.r[2].m128_f32[0], rotation.r[2].m128_f32[1], rotation.r[2].m128_f32[2]);
		return result;
	}

	XMFLOAT3 Transform::GetRight() const
	{
		XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
		XMFLOAT3 result = XMFLOAT3(rotation.r[0].m128_f32[0], rotation.r[0].m128_f32[1], rotation.r[0].m128_f32[2]);
		return result;
	}

	XMFLOAT3 Transform::GetUp() const
	{
		XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
		XMFLOAT3 result = XMFLOAT3(rotation.r[1].m128_f32[0], rotation.r[1].m128_f32[1], rotation.r[1].m128_f32[2]);
		return result;
	}
}
