#include "UIElement.h"
#include "..\..\Core\DescriptorHeap\DescriptorHeap.h"

#include <d3d12.h>
#include <DirectXMath.h>
#include <dxgitype.h>

using namespace DirectX;

KaguyaEngine::UI::UIElement::UIElement(KaguyaEngine::Core::CopyDescriptor* pCopyDescriptor, const DirectX::XMUINT2* pUIWindowSize, bool* pReleaseCallBack)
	: m_pVertexBuffer(nullptr)
	, m_pIndexBuffer(nullptr)
	, m_pVertexBufferView(nullptr)
	, m_pIndexBufferView(nullptr)
	, m_pCopyDescriptor(pCopyDescriptor)
	, m_pUIWindowSize(pUIWindowSize)
	, m_pReleaseCallBack(pReleaseCallBack)
	, m_vertexConstantBuffer()
	, m_pixelConstantBuffer()
	, m_position(XMFLOAT2(0.0f, 0.0f))
	, m_rotation(XMFLOAT3(0.0f, 0.0f, 0.0f))
	, m_size(0.0f, 0.0f)
	, m_color(DXGI_RGBA(1.0f, 1.0f, 1.0f, 1.0f))
	, m_bCenterPosition(true)
	, m_bCenterRotation(true)
	, m_bReleased(false)
{}

void KaguyaEngine::UI::UIElement::Initialize(const ID3D12Resource* pVertexBuffer, const ID3D12Resource* pIndexBuffer, const D3D12_VERTEX_BUFFER_VIEW* pVertexBufferView, const D3D12_INDEX_BUFFER_VIEW* pIndexBufferView)
{
	m_pVertexBuffer = pVertexBuffer;
	m_pIndexBuffer = pIndexBuffer;
	m_pVertexBufferView = pVertexBufferView;
	m_pIndexBufferView = pIndexBufferView;
}

void KaguyaEngine::UI::UIElement::Draw()
{
	if (!m_pVertexBuffer || !m_pIndexBuffer || !m_pUIWindowSize) {
		return;
	}

	m_vertexConstantBuffer.screenSize.x = static_cast<float>(m_pUIWindowSize->x);
	m_vertexConstantBuffer.screenSize.y = static_cast<float>(m_pUIWindowSize->y);

	//頂点シェーダー用の変数をGPUに送信
	GetCommandList()->SetGraphicsRoot32BitConstants(0, sizeof(UIVertexBuffer) / 4, &m_vertexConstantBuffer, 0);

	//ピクセルシェーダー用の変数をGPUに送信
	GetCommandList()->SetGraphicsRoot32BitConstants(2, sizeof(UIPixelBuffer) / 4, &m_pixelConstantBuffer, 0);

	GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void KaguyaEngine::UI::UIElement::Release()
{
	m_bReleased = true;
	if (m_pReleaseCallBack) {
		*m_pReleaseCallBack = true;
	}
}
