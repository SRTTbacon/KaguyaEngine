#include "DirectionalLight.h"

#include <DirectXMathMatrix.inl>
#include <DirectXMath.h>
#include <DirectXMathConvert.inl>
#include <DirectXMathVector.inl>
#include <cmath>

using namespace DirectX;

namespace KaguyaEngine::Light
{
	const float DirectionalLight::SHADOW_GRID_SIZE = 1.0f; //影の安定化のためのグリッドサイズ

	DirectionalLight::DirectionalLight()
		: m_lightViewProj(XMMatrixIdentity())
		, m_diffuseColor(1.0f, 1.0f, 1.0f, 1.0f)
		, m_ambientColor(0.1f, 0.1f, 0.1f, 1.0f)
		, m_fogColor(0.5f, 0.7f, 1.0f, 1.0f)
		, m_fogStartEnd(50.0f, 200.0f)
		, m_shadowDistance(200.0f)
		, m_shadowScale(75.0f)
	{
		SetRotation(50.0f, -30.0f, 0.0f);
		m_position.x = m_shadowScale / 4.0f;
		m_position.y = 1.0f;
		m_position.z = m_shadowScale / 4.0f;
	}

	void DirectionalLight::LateUpdate()
	{
		DirectX::XMFLOAT3 storeVector = GetForward();
		DirectX::XMVECTOR lightDirection = DirectX::XMLoadFloat3(&storeVector);
		XMFLOAT3 temp = m_position;
		temp.x = round(m_position.x / SHADOW_GRID_SIZE) * SHADOW_GRID_SIZE;
		temp.z = round(m_position.z / SHADOW_GRID_SIZE) * SHADOW_GRID_SIZE;
		DirectX::XMVECTOR sceneCenter = DirectX::XMVectorSet(temp.x, temp.y, temp.z, 1.0f);
		DirectX::XMVECTOR lightPosition = DirectX::XMVectorMultiplyAdd(lightDirection, XMVectorReplicate(-m_shadowDistance / 2.0f), sceneCenter);

		XMFLOAT3 forward = GetForward();
		XMFLOAT3 up = GetUp();

		XMVECTOR forwardVector = XMLoadFloat3(&forward);

		XMVECTOR right = XMVector3Normalize(XMVector3Cross({ 0, 0, 1 }, forwardVector));
		XMVECTOR up2 = XMVector3Cross(forwardVector, right); //交差して直交するUpを計算

		XMMATRIX result = XMMatrixLookToLH(lightPosition, lightDirection, up2);

		m_lightViewProj = result * XMMatrixOrthographicLH(m_shadowScale, m_shadowScale, 0.1f, m_shadowDistance);
	}

	DirectX::XMFLOAT4 DirectionalLight::GetLightDirection() const
	{
		XMFLOAT4 lightDir{};
		XMFLOAT3 forward = GetForward();
		lightDir = XMFLOAT4(forward.x, forward.y, forward.z, 0.0f);
		return lightDir;
	}
}
