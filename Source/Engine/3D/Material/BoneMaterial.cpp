#include "BoneMaterial.h"
#include "Core\\MaterialBase.h"
#include "..\\Model\\Core\\Mesh.h"
#include "..\\Model\\CharacterModel.h"
#include "..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\Core\\FrameBuffer.h"
#include "..\\..\\Core\\EngineDefine.h"
#include "..\\..\\Core\\EngineKeyString.h"
#include "..\\..\\Core\\Pipeline\\PipelineDefine.h"
#include "..\\..\\Core\\DescriptorHeap\\DescriptorHeap.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgiformat.h>
#include <string>
#include <vector>

using namespace KaguyaEngine::Core;
using namespace KaguyaEngine::Model;

namespace KaguyaEngine::Material
{
    BoneMaterial::BoneMaterial(FrameResource* pFrameResource)
        : MaterialBase(pFrameResource, 1)
        , m_materialBuffer(UniquePSBuffer())
        , m_rootParameterIndex(0)
    {
        m_vertexShaderPath = KeyString::SHADER_MODEL_CHARACTER_VS;
        m_pixelShaderPath = KeyString::SHADER_MODEL_CHARACTER_PS;
    }

    BoneMaterial::BoneMaterial(FrameResource* pFrameResource, std::string vertexShaderPath, std::string pixelShaderPath, uint32 textureCount)
        : MaterialBase(pFrameResource, static_cast<uint8>(textureCount))
        , m_materialBuffer(UniquePSBuffer())
        , m_rootParameterIndex(0)
    {
        m_vertexShaderPath = vertexShaderPath;
        m_pixelShaderPath = pixelShaderPath;
    }

    void BoneMaterial::LateUpdate()
    {
        MaterialBase::LateUpdate();
    }

    void BoneMaterial::DrawMesh(Mesh* pMesh)
    {
        if (!pMesh->CanDraw()) {
            return;
        }

        // キャラクター専用のバッファの送信
        CharacterMesh* pCharacterMesh = static_cast<CharacterMesh*>(pMesh);
        CharacterModel* pCharacterModel = static_cast<CharacterModel*>(pMesh->m_pParentModel);
        CharacterMeshData* pMeshData = static_cast<CharacterMeshData*>(pMesh->m_pMeshData.get());
        GetCommandList()->SetGraphicsRootConstantBufferView(m_rootParameterIndex, pCharacterModel->GetBoneMatricesBuffer()->GetGPUVirtualAddress());

        if (pMeshData->shapeDeltasBuffer)
        {
            GetCommandList()->SetGraphicsRootDescriptorTable(m_rootParameterIndex + 1, m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(pMeshData->copyShapeDeltaHeapIndex));
            GetCommandList()->SetGraphicsRootShaderResourceView(m_rootParameterIndex + 2, pCharacterMesh->m_pShapeWeightsBuffer[*m_pCurrentBackBufferIndex]->GetGPUVirtualAddress());
        }

        MaterialBase::DrawMesh(pMesh);
    }

    void BoneMaterial::CreatePipeline()
    {
        // RootSignatureの設定
        std::vector<D3D12_ROOT_PARAMETER> rootParameters;
        GetDefaultRootParameters(rootParameters);
        m_rootParameterIndex = static_cast<uint8>(rootParameters.size());
        CD3DX12_ROOT_PARAMETER param{};
        param.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);	//頂点シェーダーのb0の定数バッファを設定
        rootParameters.push_back(param);
        CD3DX12_DESCRIPTOR_RANGE heapRange{};
        // 頂点シェーダー (t0)
        heapRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        param.InitAsDescriptorTable(1, &heapRange, D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters.push_back(param);
        // シェーダー (t1)
        param.InitAsShaderResourceView(1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters.push_back(param);
        if (GetTextureCount() > 0)
        {
			m_textureRootIndex = static_cast<int>(rootParameters.size());
            // 頂点シェーダー (t0～)
            CD3DX12_DESCRIPTOR_RANGE heapRangeTexture{};
            heapRangeTexture.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, GetTextureCount(), 0);
            param.InitAsDescriptorTable(1, &heapRange, D3D12_SHADER_VISIBILITY_PIXEL);
            rootParameters.push_back(param);
        }

        // PipelineStateの設定
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
        GetDefaultInputElements(inputElements);
        inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{ "BONEWEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
        inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{ "BONEIDS", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });

        // Samplerの設定
        std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
        GetDefaultSamplers(samplers);

        // PsoDescKeyの設定
        PsoDescKey desc{};
        GetDefailtPsoDesc(desc);

        MaterialBase::CreatePipeline(rootParameters, inputElements, samplers, desc);
    }

    void BoneMaterial::CreateUniquePSBuffer()
    {
        MaterialBase::CreateUniquePSBuffer(&m_materialBuffer, sizeof(m_materialBuffer));
    }
}