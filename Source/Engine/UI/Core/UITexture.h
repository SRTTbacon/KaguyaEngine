//*****************************************************************************
//
// UI画像を描画するためのクラス
//
// UITexture.h
//
// T_Takeuchi
//
// 2026/5/26
//
//*****************************************************************************

#pragma once

#include "UIElement.h"
#include "..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\Core\\Texture2D\\Texture2D.h"

#include <dxgitype.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <vector>
#include <string>

namespace KaguyaEngine::Core
{
	struct CopyDescriptor;
}

namespace KaguyaEngine::UI
{
	class UITexture : public UIElement
	{
	public:
        UITexture(KaguyaEngine::Core::CopyDescriptor* pCopyDescriptor, const DirectX::XMUINT2* pUIWindowSize, bool* pReleaseCallBack);
        ~UITexture();

        // 指定したファイルからテクスチャをセット
        void SetTexture(const std::string& strFilePath, bool bDDSFile = false);

        // 配列データからテクスチャをセット
        // 注意 : std::vector<char>& dataはstd::moveされるため、使用後は無効になる
        void SetTexture(std::vector<char>& data, bool bDDSFile = false);
        // 配列データからテクスチャをセット
        // dataはコピーされる
        void SetTexture(const char* data, size_t size, bool bDDSFile = false);

        // 単色のテクスチャをセット
        void SetTexture(DXGI_RGBA color);

        void SetTexture(KaguyaEngine::Texture::Texture2D* pTexture);

        virtual void Draw();

    public:
        inline ID3D12Resource* GetTextureResource() const { return m_pTexture ? m_pTexture->GetResource() : nullptr; }
        DirectX::XMFLOAT2 GetTextureSize() const;

    private:
        ObjectPtr<KaguyaEngine::Texture::Texture2D> m_pTexture;

		DirectX::XMFLOAT2 m_textureSize;    // テクスチャのサイズ (ピクセル単位)
		uint32 m_textureDescriptorIndex;


        void RemoveTextureRegister();
	};
}
