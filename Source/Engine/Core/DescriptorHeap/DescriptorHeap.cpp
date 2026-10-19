#include "..\CoreMinimal.h"
#include "DescriptorHeap.h"

#include <d3d12.h>
#include <dxgiformat.h>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <Windows.h>

namespace KaguyaEngine::Core
{
    DescriptorHeap::DescriptorHeap()
        : m_pDevice(nullptr)
        , m_descriptorSize(0)
        , m_nowDescriptorCount(0)
        , m_heapType(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    {}

    DescriptorHeap::~DescriptorHeap()
    {}

    void DescriptorHeap::Initialize(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_FLAGS flag, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32 heapSize)
    {
        m_pDevice = pDevice;

        //各マテリアルのテクスチャを収納するディスクリプタヒープを作成
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = heapSize;
        heapDesc.Type = heapType;
        heapDesc.Flags = flag;

        pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_pDescriptorHeap));

        m_descriptorUsage.resize(heapSize, false);

        //1つのヒープサイズ
        m_descriptorSize = pDevice->GetDescriptorHandleIncrementSize(heapType);

        m_heapType = heapType;
    }

    uint32 DescriptorHeap::SetResource(ID3D12Resource* pResource, uint32 offset)
    {
        if (!pResource) {
			return UINT32_MAX;
        }

        return SetResource(pResource, pResource->GetDesc().Format, offset);
    }

    uint32 DescriptorHeap::SetUAVResource(ID3D12Resource* pResource, uint32 offset)
    {
        if (!pResource) {
            return UINT32_MAX;
        }

        return SetUAVResource(pResource, pResource->GetDesc().Format, offset);
    }

    uint32 DescriptorHeap::SetResource(ID3D12Resource* pResource, DXGI_FORMAT format, uint32 offset)
    {
        if (!pResource) {
            return UINT32_MAX;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Format = format;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        return SetResource(pResource, srvDesc, offset);
    }

    uint32 DescriptorHeap::SetUAVResource(ID3D12Resource* pResource, DXGI_FORMAT format, uint32 offset)
    {
        if (!pResource) {
            return UINT32_MAX;
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = format;
        uavDesc.Texture2D.MipSlice = 0;
        return SetUAVResource(pResource, uavDesc, offset);
    }

    uint32 DescriptorHeap::SetResource(ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC& shaderResource, uint32 offset)
    {
        if (!pResource) {
            return UINT32_MAX;
        }
        if (offset >= m_descriptorUsage.size()) {
            offset = UINT32_MAX;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        uint32 offsetIndex = offset == UINT32_MAX ? GetNextAvailableDescriptor() : offset;
        cpuDescriptorHandle.ptr += m_descriptorSize * static_cast<unsigned long long>(offsetIndex);
        m_pDevice->CreateShaderResourceView(pResource, &shaderResource, cpuDescriptorHandle);
        return offsetIndex;
    }

    uint32 DescriptorHeap::SetUAVResource(ID3D12Resource* pResource, D3D12_UNORDERED_ACCESS_VIEW_DESC& shaderResource, uint32 offset)
    {
        if (!pResource) {
            return UINT32_MAX;
        }
        if (offset >= m_descriptorUsage.size()) {
            offset = UINT32_MAX;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        uint32 offsetIndex = offset == UINT32_MAX ? GetNextAvailableDescriptor() : offset;
        cpuDescriptorHandle.ptr += m_descriptorSize * static_cast<unsigned long long>(offsetIndex);
        m_pDevice->CreateUnorderedAccessView(pResource, nullptr, &shaderResource, cpuDescriptorHandle);
        return offsetIndex;
    }

    void DescriptorHeap::ClearDescriptor(uint32 offset)
    {
        if (offset >= m_descriptorUsage.size()) {
            return;
        }
        m_descriptorUsage[offset] = false;
    }

    void DescriptorHeap::ClearAllDescriptors()
    {
        std::fill(m_descriptorUsage.begin(), m_descriptorUsage.end(), false);
        m_nowDescriptorCount = 0;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCpuDescriptorHandle(uint32 offset)
    {
        //offsetの位置にあるリソースを取得
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        cpuDescriptorHandle.ptr += m_descriptorSize * (static_cast<uint64>(offset));
        return cpuDescriptorHandle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGpuDescriptorHandle(uint32 offset)
    {
        //offsetの位置にあるリソースを取得
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        gpuDescriptorHandle.ptr += m_descriptorSize * (static_cast<uint64>(offset));
        return gpuDescriptorHandle;
    }

    ID3D12DescriptorHeap* DescriptorHeap::GetHeap() const
    {
        return m_pDescriptorHeap.Get();
    }

    uint32 DescriptorHeap::GetNextAvailableDescriptor(bool bLoop)
    {
		return GetNextAvailableDescriptor(1, UINT32_MAX, bLoop);
    }

    uint32 DescriptorHeap::GetNextAvailableDescriptor(uint32 continueCount, uint32 startIndex, bool bLoop)
    {
        uint32 index = startIndex;
        if (index == UINT32_MAX) {
            index = m_nowDescriptorCount;
        }

        for (uint32 i = index; i < static_cast<uint32>(m_descriptorUsage.size()); i++)
        {
            if (!m_descriptorUsage[i])
            {
				bool bAvailable = true;
                for (uint32 j = 0; j < continueCount; j++)
                {
                    uint32 count = i + j;
                    if (count >= static_cast<uint32>(m_descriptorUsage.size()) || m_descriptorUsage[count])
                    {
                        bAvailable = false;
                        break;
                    }
                }
                if (!bAvailable)
                {
					i += continueCount - 1; // 次の連続した空き領域を探すためにインデックスを進める
                    continue;
                }

                for (uint32 j = 0; j < continueCount; j++) {
                    m_descriptorUsage[static_cast<size_t>(i + j)] = true;
                }
                m_nowDescriptorCount = i + continueCount; // 次回の検索開始位置を更新

                return i;
            }
        }
        if (!bLoop)
        {
            m_nowDescriptorCount = 0;
            return GetNextAvailableDescriptor(index, false); // インデックスが最後までいった場合、最初から検索する。それでもない場合は強制クラッシュ (仕方なし)
        }

        // すべてのディスクリプタが使用中の場合はエラー
        ENGINE_ASSERT("致命的なエラー : ディスクリプタヒープの空きがありません！");
        return 0;
    }
}
