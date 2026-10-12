#include "UITexture.h"
#include "UIElement.h"
#include "..\..\Core\CoreMinimal.h"
#include "..\..\Core\Texture2D\Texture2D.h"
#include "..\..\Core\DescriptorHeap\DescriptorHeap.h"
#include "..\\..\\Core\\Exception\\DxSystemException.h"

#include <DirectXMath.h>
#include <filesystem>
#include <dxgitype.h>
#include <DirectXMathMatrix.inl>
#include <vector>
#include <string>
#include <d3d12.h>
#include <cstdint>

using namespace KaguyaEngine::Core;
using namespace KaguyaEngine::Texture;
using namespace DirectX;

namespace KaguyaEngine::UI
{
	UITexture::UITexture(CopyDescriptor* pCopyDescriptor, const XMUINT2* pUIWindowSize, bool* pReleaseCallBack)
		: UIElement(pCopyDescriptor, pUIWindowSize, pReleaseCallBack)
		, m_textureSize(0.0f, 0.0f)
		, m_textureDescriptorIndex(UINT32_MAX)
	{
		m_pTexture = CreateObjectPtr<Texture2D>();
		m_pTexture->LoadTexture(1.0f, 1.0f, 1.0f);

		m_vertexConstantBuffer.texCoordMin = XMFLOAT2(0.0f, 0.0f);
		m_vertexConstantBuffer.texCoordMax = XMFLOAT2(1.0f, 1.0f);

		m_pixelConstantBuffer.mode = 0;
	}

	UITexture::~UITexture()
	{
		RemoveTextureRegister();
	}

	void UITexture::SetTexture(const std::string& strFilePath, bool bDDSFile)
	{
		printf("%s - Start %dms\n", strFilePath.c_str(), timeGetTime());

		if (!std::filesystem::exists(strFilePath)) {
			throw DxSystemException::OM_TEXTUREMANAGE_NOTFOUND_ERROR;
		}

		m_pTexture->LoadTexture(strFilePath, bDDSFile);
		RemoveTextureRegister();
	}

	void UITexture::SetTexture(std::vector<char>& data, bool bDDSFile)
	{
		m_pTexture->LoadTexture(data, bDDSFile);
		RemoveTextureRegister();
	}

	void UITexture::SetTexture(const char* data, size_t size, bool bDDSFile)
	{
		m_pTexture->LoadTexture(data, size, bDDSFile);
		RemoveTextureRegister();
	}

	void UITexture::SetTexture(DXGI_RGBA color)
	{
		m_pTexture->LoadTexture(color.r, color.g, color.b, color.a);
		RemoveTextureRegister();
	}

	void UITexture::SetTexture(Texture2D* pTexture)
	{
		if (!pTexture || !pTexture->IsValid()) {
			return;
		}

		m_textureDescriptorIndex = m_pCopyDescriptor->pMainHeap->SetResource(pTexture->GetResource());
	}

	void UITexture::Draw()
	{
		if (m_textureDescriptorIndex == UINT32_MAX && m_pTexture && m_pTexture->IsValid()) {
			m_textureDescriptorIndex = m_pCopyDescriptor->pMainHeap->SetResource(m_pTexture->GetResource());
			m_textureSize = GetTextureSize();
			printf("%s - Loaded %dms\n", m_pTexture->GetTextureInfo()->filePath.c_str(), timeGetTime());
		}

		if (m_textureDescriptorIndex == UINT32_MAX) {
			return;
		}

		//シェーダーに送信する変数
		m_vertexConstantBuffer.position = m_position;
		if (m_bCenterPosition) {
			m_vertexConstantBuffer.position -= m_textureSize * m_size / 2.0f;
		}

		XMMATRIX rotX = XMMatrixRotationX(m_rotation.x);
		XMMATRIX rotY = XMMatrixRotationY(m_rotation.y);
		XMMATRIX rotZ = XMMatrixRotationZ(m_rotation.z);
		m_vertexConstantBuffer.parentRotationMatrix = rotX * rotY * rotZ;

		if (!m_bCenterRotation) {
			m_vertexConstantBuffer.parentRotationCenter -= m_textureSize * m_size / 2.0f;
		}

		m_vertexConstantBuffer.size = m_textureSize * m_size;                //幅と高さ
		m_pixelConstantBuffer.color = XMFLOAT4(m_color.r, m_color.g, m_color.b, m_color.a);

		uint32 copyOffsetIndex = m_pCopyDescriptor->pCopyHeap->GetNextAvailableDescriptor();

		// テクスチャのディスクリプタをコピー用ヒープにコピー
		GetDevice()->CopyDescriptorsSimple(1, m_pCopyDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyOffsetIndex), 
			m_pCopyDescriptor->pMainHeap->GetCpuDescriptorHandle(m_textureDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		GetCommandList()->SetGraphicsRootDescriptorTable(1, m_pCopyDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyOffsetIndex));        //テクスチャ

		UIElement::Draw();
	}
	DirectX::XMFLOAT2 UITexture::GetTextureSize() const
	{
		if (m_pTexture && m_pTexture->GetResource()) {
			return XMFLOAT2(static_cast<float>(m_pTexture->GetResource()->GetDesc().Width), static_cast<float>(m_pTexture->GetResource()->GetDesc().Height));
		}

		return XMFLOAT2(0.0f, 0.0f);
	}
	void UITexture::RemoveTextureRegister()
	{
		if (m_textureDescriptorIndex != UINT32_MAX) {
			m_pCopyDescriptor->pMainHeap->ClearDescriptor(m_textureDescriptorIndex);
			m_textureDescriptorIndex = UINT32_MAX;
		}
	}
}