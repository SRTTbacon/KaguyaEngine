#pragma once

#include "..\\Camera\\Camera.h"
#include "..\\..\\Core\\FrameBuffer.h"
#include "..\\..\\Core\\EngineDefine.h"
#include "..\..\Core\CoreMinimal.h"

#include <string>
#include <DirectXMath.h>
#include <d3d12.h>
#include <memory>
#include <vector>
#include <array>

namespace KaguyaEngine::Material
{
    class MaterialManager;
}
namespace KaguyaEngine::Texture
{
    class Texture2D;
}

struct SkyboxVertex
{
    DirectX::XMFLOAT3 position;
};

namespace KaguyaEngine
{
    class SkyBox : public FrameResource
    {
    public:
        SkyBox();
        ~SkyBox();

        void Initialize();

        // カメラの数が変更された際にバッファ数を更新
        // エンジンから呼び出される
        void UpdateCameraCount(uint32 cameraCount);

        void SetSkyTexture(std::string ddsFile);

        void LateUpdate(const Camera* pCamera, uint32 cameraIndex = 0);

        void Draw(uint32 cameraIndex = 0);

    public:
        inline Texture::Texture2D* GetSkyTexture() const { return m_skyTexture.pTexture.get(); }

    private:
        struct SkyBoxContentBuffer
        {
            DirectX::XMMATRIX viewProj;
        };

        static const SkyboxVertex skyboxVertices[];
        static const uint16 skyboxIndices[];

        //DDSデータを格納する変数
		MaterialTexture m_skyTexture;
        std::vector<std::array<ComPtr<ID3D12Resource>, FRAME_BUFFER_COUNT>> m_projBuffers;        // コンスタントバッファ
        std::vector<std::array<void*, FRAME_BUFFER_COUNT>> m_pProjBuffersMapped;
        ComPtr<ID3D12Resource> m_vertexBuffer;
        ComPtr<ID3D12Resource> m_indexBuffer;

        ComPtr<ID3D12RootSignature> m_pRootSignature;            // ルートシグネチャ
        ComPtr<ID3D12PipelineState> m_pPipelineState;            // パイプラインステート

        D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
        D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

        // パイプラインを作成
        void CreatePipeline();

        //頂点バッファの作成
        void CreateVertexBuffer();
        //インデックスバッファの作成
        void CreateIndexBuffer();
    };
}
