//*****************************************************************************
//
// UI画像やフォントなどの基底クラス
//
// UIElement.h
//
// T_Takeuchi
//
// 2026/5/26
//
//*****************************************************************************

#pragma once

#include "..\..\Core\CoreMinimal.h"
#include "..\\..\\Core\\FrameBuffer.h"

#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXMathMatrix.inl>
#include <dxgitype.h>

namespace KaguyaEngine::Core
{
    struct CopyDescriptor;
}

namespace KaguyaEngine::UI
{
    class UIElement : public FrameResource
    {
    public:
        UIElement(KaguyaEngine::Core::CopyDescriptor* pCopyDescriptor, const DirectX::XMUINT2* pUIWindowSize, bool* pReleaseCallBack);

        virtual void Initialize(const ID3D12Resource* pVertexBuffer, const ID3D12Resource* pIndexBuffer, const D3D12_VERTEX_BUFFER_VIEW* pVertexBufferView, const D3D12_INDEX_BUFFER_VIEW* pIndexBufferView);

        virtual void Draw();

        virtual void Release();

    public:
        //色 (Color::WHITEなどでも指定可)
        DXGI_RGBA m_color;
        //クライアント座標系 (x, y)
        DirectX::XMFLOAT2 m_position;
        //回転 (ラジアン角)
        DirectX::XMFLOAT3 m_rotation;
        //サイズ (テキストの場合は1文字の大きさ)
        DirectX::XMFLOAT2 m_size;

        //原点を中心にするかどうか
        //デフォルト : true
        bool m_bCenterPosition;

        //原点を中心にして回転するかどうか
        //デフォルト : true
        bool m_bCenterRotation;

    public:
        inline bool GetIsReleased() const { return m_bReleased; }

    protected:
        //頂点シェーダー用
        struct UIVertexBuffer
        {
            DirectX::XMFLOAT2 position;             //左上の座標 (ピクセル単位)
            DirectX::XMFLOAT2 size;                 //幅と高さ (ピクセル単位)
            DirectX::XMFLOAT2 screenSize;           //画面サイズ
            DirectX::XMFLOAT2 texCoordMin;          //uv座標の開始位置
            DirectX::XMFLOAT2 texCoordMax;          //uv座標の終了位置
            DirectX::XMMATRIX parentRotationMatrix; //回転用マトリックス
            DirectX::XMFLOAT2 parentRotationCenter; //回転中心座標
            DirectX::XMMATRIX childRotationMatrix;  //回転用マトリックス

			UIVertexBuffer()
				: position(0.0f, 0.0f)
				, size(0.0f, 0.0f)
				, screenSize(0.0f, 0.0f)
				, texCoordMin(0.0f, 0.0f)
				, texCoordMax(1.0f, 1.0f)
				, parentRotationMatrix(DirectX::XMMatrixIdentity())
				, parentRotationCenter(0.0f, 0.0f)
				, childRotationMatrix(DirectX::XMMatrixIdentity())
			{}

        };
        //ピクセルシェーダー用
        struct UIPixelBuffer
        {
            DirectX::XMFLOAT4 color;
            uint32 mode;

			UIPixelBuffer()
				: color(1.0f, 1.0f, 1.0f, 1.0f)
				, mode(0)
            {}
        };

    protected:
        UIVertexBuffer m_vertexConstantBuffer;
        UIPixelBuffer m_pixelConstantBuffer;

        KaguyaEngine::Core::CopyDescriptor* m_pCopyDescriptor;

        bool* m_pReleaseCallBack;
        bool m_bReleased;

    private:
        // 描画に必要な頂点リソース
        const ID3D12Resource* m_pVertexBuffer;
        const ID3D12Resource* m_pIndexBuffer;
        const D3D12_VERTEX_BUFFER_VIEW* m_pVertexBufferView;
        const D3D12_INDEX_BUFFER_VIEW* m_pIndexBufferView;

        const DirectX::XMUINT2* m_pUIWindowSize;
    };
}