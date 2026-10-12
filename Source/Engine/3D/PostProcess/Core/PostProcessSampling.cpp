#include "PostProcessSampling.h"
#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\EngineDefine.h"
#include "..\\..\\..\\Core\\FrameBuffer.h"
#include "..\\..\\..\\Core\\EngineKeyString.h"
#include "..\\..\\..\\Core\\DescriptorHeap\\DescriptorHeap.h"
#include "..\\..\\..\\Core\\Pipeline\\PipelineState.h"
#include "..\\..\\..\\Core\\Pipeline\\RootSignature.h"

using namespace DirectX;
using namespace KaguyaEngine;
using namespace KaguyaEngine::Core;

namespace KaguyaEngine::PostProcess
{
    PostProcessSampling::PostProcessSampling()
        : m_pUpSamplePipelineState(nullptr)
        , m_pDownSamplePipelineState(nullptr)
        , m_rtvDescriptorSize(0)
        , m_tempRenderIndex(0)
    {}

    PostProcessSampling::~PostProcessSampling()
    {
    }

    void PostProcessSampling::Initialize(DXGI_FORMAT rtvFormat, ID3D12RootSignature* pRootSignature)
    {
        m_rtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        if (m_pUpSamplePipelineState) {
            DeletePipelineState(m_pUpSamplePipelineState);
        }
        if (m_pDownSamplePipelineState) {
			DeletePipelineState(m_pDownSamplePipelineState);
        }

        const std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements = GetBasicVertexLayout();

        uint64 tempOutHashID;

        PsoDescKey psoDesc{};
        psoDesc.depthStencilPreset = DepthStencilPreset_UI;
        psoDesc.numRenderTargets = 1;
        psoDesc.rtvFormats[0] = rtvFormat;
        psoDesc.cullMode = D3D12_CULL_MODE_BACK;
        psoDesc.vertexShaderPath = KeyString::SHADER_MAINRENDER_VS;
        psoDesc.pixelShaderPath = KeyString::SHADER_POSTPROCESS_UPSAMPLER_PS;
        m_pUpSamplePipelineState = PipelineState::CreatePipelineState(m_pDevice, inputElements.data(), static_cast<uint32>(inputElements.size()), pRootSignature, psoDesc, tempOutHashID);

        psoDesc.pixelShaderPath = KeyString::SHADER_POSTPROCESS_DOWNSAMPLER_PS;
        m_pDownSamplePipelineState = PipelineState::CreatePipelineState(m_pDevice, inputElements.data(), static_cast<uint32>(inputElements.size()), pRootSignature, psoDesc, tempOutHashID);
    }

    void PostProcessSampling::ResetRenderIndex(UINT initIndex)
    {
        m_tempRenderIndex = initIndex;
    }

    void PostProcessSampling::BeginUpSample(float intensity)
    {
        GetCommandList()->SetPipelineState(m_pUpSamplePipelineState);
        GetCommandList()->SetGraphicsRoot32BitConstant(0, *reinterpret_cast<UINT*>(&intensity), 0);
    }

    void PostProcessSampling::DrawUpSample(D3D12_CPU_DESCRIPTOR_HANDLE& handle, ID3D12Resource* pRenderTarget, D3D12_CPU_DESCRIPTOR_HANDLE& lowTextureMainSrvHandle, D3D12_CPU_DESCRIPTOR_HANDLE& highTextureMainSrvHandle)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            pRenderTarget,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        GetCommandList()->ResourceBarrier(1, &barrier);

        CD3DX12_VIEWPORT viewPort = CD3DX12_VIEWPORT(pRenderTarget);
        GetCommandList()->RSSetViewports(1, &viewPort);

        D3D12_RECT rect{ 0, 0, static_cast<int>(viewPort.Width), static_cast<int>(viewPort.Height) };
        GetCommandList()->RSSetScissorRects(1, &rect);

        GetCommandList()->OMSetRenderTargets(1, &handle, false, nullptr);

		uint32 copyIndex1 = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor();
		uint32 copyIndex2 = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor();
		m_pDevice->CopyDescriptorsSimple(1, m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex1), lowTextureMainSrvHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		m_pDevice->CopyDescriptorsSimple(1, m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex2), highTextureMainSrvHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        GetCommandList()->SetGraphicsRootDescriptorTable(1, m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyIndex1));

        GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            pRenderTarget,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        GetCommandList()->ResourceBarrier(1, &barrier);

        m_tempRenderIndex += 2;
    }

    void PostProcessSampling::BeginDownSample()
    {
        GetCommandList()->SetPipelineState(m_pDownSamplePipelineState);
    }

    void PostProcessSampling::DrawDownSample(D3D12_CPU_DESCRIPTOR_HANDLE& handle, ID3D12Resource* pRenderTarget, D3D12_GPU_DESCRIPTOR_HANDLE& highTextureCopySrvHandle, DirectX::XMUINT2 renderSize)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            pRenderTarget,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        GetCommandList()->ResourceBarrier(1, &barrier);

        CD3DX12_VIEWPORT viewPort = CD3DX12_VIEWPORT(pRenderTarget);
        GetCommandList()->RSSetViewports(1, &viewPort);

        D3D12_RECT rect{ 0, 0, static_cast<int>(viewPort.Width), static_cast<int>(viewPort.Height) };
        GetCommandList()->RSSetScissorRects(1, &rect);

        GetCommandList()->OMSetRenderTargets(1, &handle, false, nullptr);

        DownSampleStruct buffer{};
        buffer.texelSize.x = 1.0f / renderSize.x;
        buffer.texelSize.y = 1.0f / renderSize.y;

        GetCommandList()->SetGraphicsRoot32BitConstants(0, sizeof(buffer) / 4, &buffer, 0);

        GetCommandList()->SetGraphicsRootDescriptorTable(1, highTextureCopySrvHandle);

        GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            pRenderTarget,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        GetCommandList()->ResourceBarrier(1, &barrier);

        m_tempRenderIndex++;
    }
}
