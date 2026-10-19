//*****************************************************************************
//
// NV12テクスチャを描画するマテリアル
//
// MovieMaterial.cpp
//
// T_Takeuchi
//
// 2026/10/14
//
//*****************************************************************************

#include "MovieMaterial.h"
#include "..\\Core\\MaterialBase.h"
#include "..\\..\\Model\\Core\\Mesh.h"
#include "..\\..\\..\\Core\\Pipeline\\PipelineDefine.h"
#include "..\\..\\..\\Core\\Texture2D\\Texture2D.h"
#include "..\\..\\..\\Core\\DescriptorHeap\\DescriptorHeap.h"
#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\EngineKeyString.h"

#include <d3dx12.h>

using namespace KaguyaEngine::Core;
using namespace KaguyaEngine::Model;

namespace KaguyaEngine::Material
{
    MovieMaterial::MovieMaterial(FrameResource* pFrameResource)
        : MaterialBase(pFrameResource, 1)
        , m_materialBuffer(UniquePSBuffer())
		, movieTextureMainSrvIndex(UINT32_MAX)
        , m_pNV12Texture(nullptr)
    {
        m_vertexShaderPath = KeyString::SHADER_MODEL_PRIMITIVE_VS;
        m_pixelShaderPath = KeyString::SHADER_MODEL_MOVIE_PS;
    }

    MovieMaterial::MovieMaterial(FrameResource* pFrameResource, std::string vertexShaderPath, std::string pixelShaderPath, uint8 textureCount)
        : MaterialBase(pFrameResource, textureCount)
        , m_materialBuffer(UniquePSBuffer())
		, movieTextureMainSrvIndex(UINT32_MAX)
        , m_pNV12Texture(nullptr)
    {
        m_vertexShaderPath = vertexShaderPath;
        m_pixelShaderPath = pixelShaderPath;
    }

    void MovieMaterial::Initialize(D3D12_CULL_MODE cullMode, bool bTransparentMode)
    {
        MaterialBase::Initialize(cullMode, bTransparentMode);

		movieTextureMainSrvIndex = m_pDescriptor->pMainHeap->GetNextAvailableDescriptor(2U);

        SetNV12Texture(m_dummyTexture.pTexture->GetResource());
    }

    void MovieMaterial::SetNV12Texture(ID3D12Resource* pNV12Texture)
    {
		m_pNV12Texture = pNV12Texture;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDescY = {};
        srvDescY.Format = DXGI_FORMAT_R8_UNORM;
        srvDescY.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDescY.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDescY.Texture2D.MipLevels = 1;
        srvDescY.Texture2D.MostDetailedMip = 0;
        srvDescY.Texture2D.PlaneSlice = 0; // Y面
        m_pDescriptor->pMainHeap->SetResource(pNV12Texture, srvDescY, movieTextureMainSrvIndex);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDescUV = {};
        srvDescUV.Format = DXGI_FORMAT_R8G8_UNORM;
        srvDescUV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDescUV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDescUV.Texture2D.MipLevels = 1;
        srvDescUV.Texture2D.MostDetailedMip = 0;
        srvDescUV.Texture2D.PlaneSlice = 1; // UV面
		m_pDescriptor->pMainHeap->SetResource(pNV12Texture, srvDescUV, movieTextureMainSrvIndex + 1);
    }

    void MovieMaterial::LateUpdate()
    {
        // 固有バッファにマテリアルデータをコピー
        if (m_pUniquePSBufferMapped[*m_pCurrentBackBufferIndex]) {
            memcpy(m_pUniquePSBufferMapped[*m_pCurrentBackBufferIndex], m_pUniquePSData, m_uniqueBufferSize);
        }
    }

    void MovieMaterial::Draw()
    {
        uint32 movieTextureCopySrvIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor(2U);
        m_pDevice->CopyDescriptorsSimple(2, m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(movieTextureCopySrvIndex), m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(movieTextureMainSrvIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        GetCommandList()->SetGraphicsRootDescriptorTable(m_textureRootIndex, m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(movieTextureCopySrvIndex));

        for (Mesh* pMesh : m_pMeshes)
        {
            DrawMesh(pMesh);
        }
    }

    void MovieMaterial::CreatePipeline()
    {
        // RootSignatureの設定
        std::vector<D3D12_ROOT_PARAMETER> rootParameters;
        GetDefaultRootParameters(rootParameters);

        m_textureRootIndex = static_cast<int>(rootParameters.size());
        // 頂点シェーダー (t0～)
        CD3DX12_DESCRIPTOR_RANGE heapRange{};
		heapRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0); // Y/UVの2つのSRVを設定
        CD3DX12_ROOT_PARAMETER param{};
        param.InitAsDescriptorTable(1, &heapRange, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters.push_back(param);

        // PipelineStateの設定
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
        GetDefaultInputElements(inputElements);

        // Samplerの設定
        std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
        GetDefaultSamplers(samplers);

        // PsoDescKeyの設定
        PsoDescKey desc{};
        GetDefailtPsoDesc(desc);

        MaterialBase::CreatePipeline(rootParameters, inputElements, samplers, desc);
    }

    void MovieMaterial::CreateUniquePSBuffer()
    {
        MaterialBase::CreateUniquePSBuffer(&m_materialBuffer, sizeof(m_materialBuffer));
    }
}
