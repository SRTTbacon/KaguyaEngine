#include "PostProcessBase.h"
#include "..\\..\\..\\Core\\EngineDefine.h"
#include "..\\..\\..\\Core\\DescriptorHeap\\DescriptorHeap.h"
#include "..\\..\\Camera\\Camera.h"

#include <d3dx12.h>

using namespace DirectX;
using namespace KaguyaEngine;

namespace KaguyaEngine::PostProcess
{
    PostProcessBase::PostProcessBase(uint32 postProcessID, const KaguyaEngine::Camera* pCamera, KaguyaEngine::Core::DescriptorHeap* pRtvHeap, ID3D12RootSignature* pBaseRootSignature)
        : m_postProcessID(postProcessID)
        , m_pCamera(pCamera)
        , m_pRtvHeap(pRtvHeap)
        , m_pBaseRootSignature(pBaseRootSignature)
        , m_copyDescriptorFirstIndex(0)
        , m_rtvStartIndex(UINT32_MAX)
        , m_srvStartIndex(UINT32_MAX)
        , m_depth(0)
        , m_bEnable(true)
    {
		m_pCameraResource = &m_pCamera->GetCameraResource();
    }

    PostProcessBase::~PostProcessBase()
    {
    }

    inline RenderResource PostProcessBase::GetResultRenderTarget() const
    {
        RenderResource outResource{};
        outResource.pResource = m_mainRenderTarget.pResource.Get();
        outResource.mainSrvHandleCPU = m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(m_mainRenderTarget.mainSrvIndex);
        outResource.mainSrvHandleGPU = m_pDescriptor->pMainHeap->GetGpuDescriptorHandle(m_mainRenderTarget.mainSrvIndex);
        uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor();
        CopyDescriptors(1, outResource.mainSrvHandleCPU, copyIndex);
        outResource.copySrvHandleCPU = m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex);
        outResource.copySrvHandleGPU = m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyIndex);
        outResource.mainSrvIndex = m_mainRenderTarget.mainSrvIndex;

        return outResource;
    }

    void PostProcessBase::SetSRVResource(PPResource& resource, uint32 srvMainIndex)
    {
        m_pDescriptor->pMainHeap->SetResource(resource.pResource.Get(), srvMainIndex);
		resource.mainSrvIndex = srvMainIndex;
    }

    void PostProcessBase::SetUAVResource(PPResource& resource, uint32 uavMainIndex)
    {
        m_pDescriptor->pMainHeap->SetUAVResource(resource.pResource.Get(), uavMainIndex);
        resource.mainUavIndex = uavMainIndex;
    }

    void PostProcessBase::Draw(const D3D12_GPU_DESCRIPTOR_HANDLE& srvCopyGPUHandle)
    {
        if (srvCopyGPUHandle.ptr) {
            GetCommandList()->SetGraphicsRootDescriptorTable(1, srvCopyGPUHandle);
        }

        GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
    }

    void PostProcessBase::CreateRenderTexture(const DirectX::XMFLOAT4& clearColor, PPResource& resource, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle, DXGI_FORMAT format, float division, 
        bool bCreateRTV, std::string name)
    {
		DirectX::XMINT2 renderSize(m_pCamera->m_renderScissor.right - m_pCamera->m_renderScissor.left, m_pCamera->m_renderScissor.bottom - m_pCamera->m_renderScissor.top);

        //中間テクスチャをフレーム数分作成
        D3D12_RESOURCE_DESC desc2 = CD3DX12_RESOURCE_DESC::Tex2D(format, static_cast<UINT64>(renderSize.x / division), static_cast<UINT>(renderSize.y / division));
        desc2.SampleDesc.Count = 1;
        desc2.MipLevels = 1;
        desc2.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = format;
        clearValue.Color[0] = clearColor.x;
        clearValue.Color[1] = clearColor.y;
        clearValue.Color[2] = clearColor.z;
        clearValue.Color[3] = clearColor.w;

        CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        HRESULT hr = GetDevice()->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &desc2,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(resource.pResource.ReleaseAndGetAddressOf())
        );
        if (FAILED(hr)) {
            return;
        }

        resource.pResource->SetName(GetWideString("PostProcessBase::CreateRenderTexture" + name).c_str());

        if (bCreateRTV)
        {
            //中間テクスチャ用のRTVを作成
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = format;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            GetDevice()->CreateRenderTargetView(resource.pResource.Get(), &rtvDesc, rtvHandle);
            resource.rtvHandle = rtvHandle;
            rtvHandle.ptr += m_pDescriptor->pMainHeap->GetHeapSize();
        }
    }

    void PostProcessBase::ChangeBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            pResource,
            before,
            after
        );
        GetCommandList()->ResourceBarrier(1, &barrier);
    }

    void PostProcessBase::ChangeBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, std::vector<D3D12_RESOURCE_BARRIER>& barriers)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            pResource,
            before,
            after
        );

        barriers.push_back(barrier);
    }

    void PostProcessBase::SetViewportAndScissor(const DirectX::XMUINT2& viewportSize)
    {
        //ビューポートとシザー矩形を設定
        CD3DX12_VIEWPORT viewPort{};
        viewPort.TopLeftX = 0.0f;
        viewPort.TopLeftY = 0.0f;
        viewPort.Width = static_cast<float>(viewportSize.x);
        viewPort.Height = static_cast<float>(viewportSize.y);
        GetCommandList()->RSSetViewports(1, &viewPort);
        CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(viewportSize.x), static_cast<LONG>(viewportSize.y));
        GetCommandList()->RSSetScissorRects(1, &scissorRect);
    }
    void PostProcessBase::CopyDescriptors(uint32 count, uint32 srcIndex, uint32 destIndex) const
    {
        m_pDevice->CopyDescriptorsSimple(count, m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(destIndex), m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(srcIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    void PostProcessBase::CopyDescriptors(uint32 count, D3D12_CPU_DESCRIPTOR_HANDLE& src, uint32 destIndex) const
    {
        m_pDevice->CopyDescriptorsSimple(count, m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(destIndex), src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
}