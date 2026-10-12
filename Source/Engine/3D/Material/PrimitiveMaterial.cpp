#include "PrimitiveMaterial.h"
#include "Core\\MaterialBase.h"
#include "..\\..\\Core\\Pipeline\\PipelineDefine.h"
#include "..\\..\\Core\\FrameBuffer.h"
#include "..\\..\\Core\\EngineDefine.h"
#include "..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\Core\\EngineKeyString.h"

#include <d3d12.h>
#include <vector>
#include <string>

using namespace KaguyaEngine::Core;

namespace KaguyaEngine::Material
{
    PrimitiveMaterial::PrimitiveMaterial(FrameResource* pFrameResource)
        : MaterialBase(pFrameResource, 1)
        , m_materialBuffer(UniquePSBuffer())
    {
        m_vertexShaderPath = KeyString::SHADER_MODEL_PRIMITIVE_VS;
        m_pixelShaderPath = KeyString::SHADER_MODEL_PRIMITIVE_PS;
    }

    PrimitiveMaterial::PrimitiveMaterial(FrameResource* pFrameResource, std::string vertexShaderPath, std::string pixelShaderPath, uint8 textureCount)
        : MaterialBase(pFrameResource, textureCount)
        , m_materialBuffer(UniquePSBuffer())
    {
        m_vertexShaderPath = vertexShaderPath;
        m_pixelShaderPath = pixelShaderPath;
    }

    void PrimitiveMaterial::CreatePipeline()
    {
        // RootSignatureの設定
        std::vector<D3D12_ROOT_PARAMETER> rootParameters;
        GetDefaultRootParameters(rootParameters);

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

    void PrimitiveMaterial::CreateUniquePSBuffer()
    {
        MaterialBase::CreateUniquePSBuffer(&m_materialBuffer, sizeof(m_materialBuffer));
    }
}
