//*****************************************************************************
//
// UIを管理するためのクラス
//
// UIManager.h
//
// T_Takeuchi
//
// 2026/5/27
//
//*****************************************************************************

#pragma once

#include "..\\Core\\CoreMinimal.h"
#include "..\\Core\\FrameBuffer.h"
#include "Core\\UIElement.h"
#include "Core\\UITexture.h"

#include <DirectXMath.h>
#include <dxgitype.h>
#include <string>
#include <vector>
#include <d3d12.h>

namespace KaguyaEngine::Core
{
	class RootSignature;
	class PipelineState;
	class DescriptorHeap;
	struct CopyDescriptor;
}

namespace KaguyaEngine::UI
{
	using UITextureRef = ObjectPtr<UITexture>;
	using UIElementRef = ObjectPtr<UIElement>;

	class UIManager : public FrameResource
	{
	public:
		UIManager(Core::DescriptorHeap* pRtvDescriptorHeap, KaguyaEngine::Core::CopyDescriptor* pCopyDescriptor, const DirectX::XMUINT2* pWindowSize);
		void Initialize();

		void ReBuildRenderTexture();

		// 純粋なUITextureを追加
		UITextureRef AddUITexture();
		// ファイルパスからUITextureを追加
		UITextureRef AddUITexture(std::string strFilePath, bool bDDSFile = false);
		// メモリからUITextureを追加
		// この関数実行後はstd::vector dataは無効になります。
		UITextureRef AddUITexture(std::vector<char>& data, bool bDDSFile = false);
		// メモリからデータをコピーしてUITextureを追加
		UITextureRef AddUITexture(const char* data, size_t size, bool bDDSFile = false);
		UITextureRef AddUITexture(DXGI_RGBA color);
		UITextureRef AddUITextureCircle(DXGI_RGBA color = DXGI_RGBA(1.0f, 1.0f, 1.0f, 1.0f), int size = 64);

		void LateUpdate();

		void BeginRender();
		void EndRender();

	public:
		inline ID3D12Resource* GetUIRenderTarget() const { return m_pUIRenderTarget.Get(); }

	private:
		std::vector<UIElementRef> m_uiElements;

		KaguyaEngine::Core::CopyDescriptor* m_pCopyDescriptor;

		ObjectPtr<KaguyaEngine::Core::RootSignature> m_pRootSignature;
		ObjectPtr<KaguyaEngine::Core::PipelineState> m_pPipelineState;
		ComPtr<ID3D12Resource> m_pUIRenderTarget;

		KaguyaEngine::Core::DescriptorHeap* m_pRtvDescriptorHeap;

		const DirectX::XMUINT2* m_pWindowSize;

		uint32 m_rtvDescriptorHeapIndex;

		bool m_bExistReleaseObject;

		void CreatePipeline();
	};
}
