#include "SkyBox.h"
#include "..\\Camera\\Camera.h"
#include "..\\Material\\Core\\MaterialManager.h"
#include "..\\..\Core\\CoreMinimal.h"
#include "..\\..\\Core\\EngineDefine.h"
#include "..\\..\\Core\\Texture2D\\Texture2D.h"
#include "..\\..\\Core\\DescriptorHeap\\DescriptorHeap.h"
#include "..\\..\\Core\\DirectXCore\\ResourceStatics.h"
#include "..\\..\\Core\\Thread\\ResourceCopyThread.h"
#include "..\\..\\Core\\Pipeline\\PipelineState.h"
#include "..\\..\\Core\\Pipeline\\RootSignature.h"
#include "..\\..\\Core\\Pipeline\\PipelineDefine.h"
#include "..\\..\\Core\\EngineKeyString.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <d3dcommon.h>
#include <dxgiformat.h>
#include <DirectXMath.h>
#include <DirectXMathMatrix.inl>
#include <string>
#include <string.h>
#include <utility>
#include <cstdio>
#include <iterator>
#include <Windows.h>

using namespace DirectX;
using namespace KaguyaEngine::Core;
using namespace KaguyaEngine::Texture;
using namespace KaguyaEngine::Material;
using namespace KaguyaEngine::AsyncTask;

namespace KaguyaEngine
{
    // 立方体の頂点データ（大きな立方体）
    const SkyboxVertex SkyBox::skyboxVertices[] =
    {
        { {-1.0f,  1.0f, -1.0f} }, // 0: 左上前
        { { 1.0f,  1.0f, -1.0f} }, // 1: 右上前
        { {-1.0f, -1.0f, -1.0f} }, // 2: 左下前
        { { 1.0f, -1.0f, -1.0f} }, // 3: 右下前
        { {-1.0f,  1.0f,  1.0f} }, // 4: 左上奥
        { { 1.0f,  1.0f,  1.0f} }, // 5: 右上奥
        { {-1.0f, -1.0f,  1.0f} }, // 6: 左下奥
        { { 1.0f, -1.0f,  1.0f} }, // 7: 右下奥
    };
    // インデックスデータ（スカイボックス用）
    const uint16 SkyBox::skyboxIndices[] =
    {
        // 前面
        0, 1, 2,
        2, 1, 3,
        // 背面
        5, 4, 7,
        7, 4, 6,
        // 左面
        4, 0, 6,
        6, 0, 2,
        // 右面
        1, 5, 3,
        3, 5, 7,
        // 上面
        4, 5, 0,
        0, 5, 1,
        // 下面
        2, 3, 6,
        6, 3, 7,
    };

    SkyBox::SkyBox()
        : m_pRootSignature(nullptr)
        , m_pPipelineState(nullptr)
        , m_vertexBufferView(D3D12_VERTEX_BUFFER_VIEW())
        , m_indexBufferView(D3D12_INDEX_BUFFER_VIEW())
    {
    }

    SkyBox::~SkyBox()
    {
        for (size_t i = 0; i < m_pProjBuffersMapped.size(); i++)
        {
            for (int j = 0; j < FRAME_BUFFER_COUNT; j++)
            {
                if (!m_pProjBuffersMapped[i][j])
                {
                    m_projBuffers[i][j]->Unmap(0, nullptr);
                    m_pProjBuffersMapped[i][j] = nullptr;
                }
            }
        }
    }

    void SkyBox::Initialize()
    {
        // 点シェーダー(b0)用のリソースを作成
        D3D12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC constantData = CD3DX12_RESOURCE_DESC::Buffer(GetAlignedSize(sizeof(SkyBoxContentBuffer)));
        m_projBuffers.resize(1);
        m_pProjBuffersMapped.resize(1);
        for (uint32 i = 0; i < FRAME_BUFFER_COUNT; i++)
        {
            HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &constantData, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_projBuffers[0][i]));

            if (FAILED(hr))
            {
                printf("コンスタントバッファの生成に失敗:エラーコード%d\n", hr);
                return;
            }
            m_projBuffers[0][i]->Map(0, nullptr, &m_pProjBuffersMapped[0][i]);
        }

        CreatePipeline();
        CreateVertexBuffer();
        CreateIndexBuffer();
    }

    void SkyBox::UpdateCameraCount(uint32 cameraCount)
    {
        size_t nowCameraCount = m_projBuffers.size();

        m_projBuffers.resize(cameraCount);
        m_pProjBuffersMapped.resize(cameraCount);

        D3D12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferSize = CD3DX12_RESOURCE_DESC::Buffer(GetAlignedSize(sizeof(SkyBoxContentBuffer)));

        // 今のバッファ数よりも大きい場合は増やす
        if (nowCameraCount < cameraCount)
        {
            for (size_t i = nowCameraCount; i < cameraCount; i++)
            {
                for (int j = 0; j < FRAME_BUFFER_COUNT; j++)
                {
                    HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferSize, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_projBuffers[i][j]));

                    if (FAILED(hr))
                    {
                        printf("コンスタントバッファの生成に失敗:エラーコード%d\n", hr);
                        return;
                    }
                    m_projBuffers[i][j]->Map(0, nullptr, &m_pProjBuffersMapped[i][j]);
                }
            }
        }
    }

    void SkyBox::SetSkyTexture(std::string ddsFile)
    {
        m_skyTexture.pTexture = CreateObjectPtr<Texture2D>();

        m_skyTexture.pTexture->SetOnLoaded([this](){
            D3D12_RESOURCE_DESC texDesc = m_skyTexture.pTexture->GetResource()->GetDesc();
            if (texDesc.DepthOrArraySize != 6 || texDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
                printf("キューブマップテクスチャをロードできませんでした。\n");
                return;
            }

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = m_skyTexture.pTexture->GetResource()->GetDesc().Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MostDetailedMip = 0;
            srvDesc.TextureCube.MipLevels = 1;

            m_skyTexture.mainDescriptorIndex = GetDescriptor()->pMainHeap->SetResource(m_skyTexture.pTexture->GetResource(), srvDesc);
			});

        m_skyTexture.pTexture->LoadTexture(ddsFile, true);
    }

    void SkyBox::LateUpdate(const Camera* pCamera, uint32 cameraIndex)
    {
        if (!m_skyTexture.pTexture || !m_skyTexture.pTexture->IsValid()) {
            return;
        }

        // カメラのビュー行列から平行移動成分を除去
        SkyBoxContentBuffer buffer{};
        XMMATRIX viewMatrix = pCamera->GetViewMatrix();
        viewMatrix.r[3].m128_f32[0] = 0.0f;
        viewMatrix.r[3].m128_f32[1] = 0.0f;
        viewMatrix.r[3].m128_f32[2] = 0.0f;

        // VP行列を更新
        buffer.viewProj = XMMatrixMultiply(viewMatrix, pCamera->GetProjMatrix());

        // 定数バッファにデータを書き込む
        memcpy(m_pProjBuffersMapped[cameraIndex][*m_pCurrentBackBufferIndex], &buffer, sizeof(SkyBoxContentBuffer));

        //  テクスチャのディスクリプタを描画用にコピー
        uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor();
        m_pDevice->CopyDescriptorsSimple(1, m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex), m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(m_skyTexture.mainDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_skyTexture.copyDescriptorIndex = copyIndex;
    }

    void SkyBox::Draw(uint32 cameraIndex)
    {
        if (!m_skyTexture.pTexture || !m_skyTexture.pTexture->IsValid()) {
            return;
        }

        GetCommandList()->SetGraphicsRootSignature(m_pRootSignature.Get());   //ルートシグネチャを設定
        GetCommandList()->SetPipelineState(m_pPipelineState.Get());           //パイプラインステートを設定

        GetCommandList()->SetGraphicsRootConstantBufferView(0, m_projBuffers[cameraIndex][*m_pCurrentBackBufferIndex]->GetGPUVirtualAddress());

        GetCommandList()->SetGraphicsRootDescriptorTable(1, GetDescriptor()->pCopyHeap->GetGpuDescriptorHandle(m_skyTexture.copyDescriptorIndex));

        GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);    //3角ポリゴンのみ
        GetCommandList()->IASetVertexBuffers(0, 1, &m_vertexBufferView);                  //頂点情報を送信
        GetCommandList()->IASetIndexBuffer(&m_indexBufferView);                           //インデックス情報を送信

        GetCommandList()->DrawIndexedInstanced(static_cast<uint32>(std::size(skyboxIndices)), 1, 0, 0, 0);     //描画
    }

    void SkyBox::CreatePipeline()
    {
        // サンプラー設定
        CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
        samplerDesc.MaxAnisotropy = 0;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// ルートパラメータ設定
        CD3DX12_ROOT_PARAMETER rootParameters[2]{};
        rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);	//頂点シェーダーのb0の定数バッファを設定
        //テクスチャ用をスロットt0に設定
        CD3DX12_DESCRIPTOR_RANGE descriptorRange{};;
        descriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);	//シェーダーリソースビュー(1個)
        rootParameters[1].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);	//t0

		// 入力レイアウト設定
        D3D12_INPUT_ELEMENT_DESC inputElements = D3D12_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

        // シェーダー設定
        PsoDescKey psoDesc{};
		psoDesc.depthStencilPreset = DepthStencilPreset::DepthStencilPreset_Skybox;
        psoDesc.vertexShaderPath = KeyString::SHADER_SKYBOX_VS;
		psoDesc.pixelShaderPath = KeyString::SHADER_SKYBOX_PS;
		psoDesc.rtvFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        psoDesc.numRenderTargets = 1;

        // パイプライン作成
        uint64 rootSignatureHash = GetShaderHash(psoDesc.vertexShaderPath, psoDesc.pixelShaderPath);
        uint64 m_pipelineHashID;
        m_pRootSignature = RootSignature::CreateRootSignature(m_pDevice, rootSignatureHash, rootParameters, 2, &samplerDesc, 1);
        m_pPipelineState = PipelineState::CreatePipelineState(m_pDevice, &inputElements, 1, rootSignatureHash, psoDesc, m_pipelineHashID);
    }

    // 頂点バッファの作成
    void SkyBox::CreateVertexBuffer()
    {
        // 頂点バッファサイズ
        const uint32 vertexBufferSize = static_cast<uint32>(std::size(skyboxVertices) * sizeof(SkyboxVertex));

        // DEFAULTヒープ（GPU専用）
        CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

        HRESULT hr = m_pDevice->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &vertexBufferDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)
        );

        if (FAILED(hr)) {
            printf("DEFAULT 頂点バッファの生成に失敗しました。%1xl\n", hr);
            return;
        }

        // 一時UPLOADバッファ
        ID3D12Resource* pUploadBuffer = ResourceStatics::CreateUploadResource(vertexBufferSize);

        // UPLOADバッファに書き込み
        void* vertexDataBegin = nullptr;
        pUploadBuffer->Map(0, nullptr, &vertexDataBegin);
        memcpy(vertexDataBegin, skyboxVertices, vertexBufferSize);
        pUploadBuffer->Unmap(0, nullptr);

        CopyJob copyJob{};
        copyJob.func = [this, pUploadBuffer, vertexBufferSize](ID3D12GraphicsCommandList* pCommandList) {
            D3D12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
            pCommandList->ResourceBarrier(1, &barrier2);

            // コマンドでDEFAULTにコピー
            pCommandList->CopyBufferRegion(m_vertexBuffer.Get(), 0, pUploadBuffer, 0, vertexBufferSize);
            };

		copyJob.OnComplete = [this, pUploadBuffer]() {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
            );
            GetCommandList()->ResourceBarrier(1, &barrier);

            // VertexBufferView設定
            m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
            m_vertexBufferView.StrideInBytes = sizeof(SkyboxVertex);
            m_vertexBufferView.SizeInBytes = vertexBufferSize;
			pUploadBuffer->Release();
			};

        LockMutex mutex(g_copyJobManager->m_mutex);
        g_copyJobManager->m_jobs.push_back(std::move(copyJob));
        g_copyJobManager->m_cv.notify_one();
    }

    // インデックスバッファの作成
    void SkyBox::CreateIndexBuffer()
    {
        // インデックスバッファサイズ
        const uint32 indexBufferSize = static_cast<uint32>(std::size(skyboxIndices) * sizeof(uint16));

        //DEFAULTヒープ（GPU専用）
        CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

        HRESULT hr = m_pDevice->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &indexBufferDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_indexBuffer)
        );

        if (FAILED(hr))
        {
            printf("DEFAULT インデックスバッファの生成に失敗しました。\n");
            return;
        }

        // 一時UPLOAD
        ID3D12Resource* pUploadBuffer = ResourceStatics::CreateUploadResource(indexBufferSize);

        // UPLOADバッファへデータを書き込み
        void* mappedData = nullptr;
        pUploadBuffer->Map(0, nullptr, &mappedData);
        memcpy(mappedData, skyboxIndices, indexBufferSize);
        pUploadBuffer->Unmap(0, nullptr);

        CopyJob copyJob{};
		copyJob.func = [this, pUploadBuffer, indexBufferSize](ID3D12GraphicsCommandList* pCommandList) {
            D3D12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
            pCommandList->ResourceBarrier(1, &barrier2);

            // DEFAULTヒープへコピー
            pCommandList->CopyBufferRegion(m_indexBuffer.Get(), 0, pUploadBuffer, 0, indexBufferSize);
			};
		copyJob.OnComplete = [this, pUploadBuffer]() {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
            GetCommandList()->ResourceBarrier(1, &barrier);

            // IndexBufferView設定
            m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
            m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
            m_indexBufferView.SizeInBytes = indexBufferSize;
			pUploadBuffer->Release();
			};

        LockMutex mutex(g_copyJobManager->m_mutex);
        g_copyJobManager->m_jobs.push_back(std::move(copyJob));
		g_copyJobManager->m_cv.notify_one();
    }
}
