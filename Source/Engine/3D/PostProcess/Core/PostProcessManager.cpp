#include "PostProcessManager.h"
#include "..\\..\\..\\Core\\Pipeline\\RootSignature.h"
#include "..\\..\\Camera\\Camera.h"
#include "..\\..\\..\\Engine.h"

#include <d3dx12.h>

using namespace DirectX;
using namespace KaguyaEngine;
using namespace KaguyaEngine::Core;

namespace KaguyaEngine::PostProcess
{
    const UINT PostProcessManager::MAX_POSTPROCESS_COUNT = 100;

    PostProcessManager::PostProcessManager(const Camera* pCamera, KaguyaEngine::Core::DescriptorHeap* pRtvHeap)
        : m_pCamera(pCamera)
        , m_pRtvHeap(pRtvHeap)
        , m_nextPostProcessID(0)
        , m_lastRenderResource(RenderResource())
        , m_pBaseRootSignature(nullptr)
    {
        for (UINT i = 0; i < MAX_POSTPROCESS_COUNT; i++) {
            m_bFreePostProcessIDs.push_back(true);
        }
    }

    PostProcessManager::~PostProcessManager()
    {
    }

    void PostProcessManager::Initialize()
    {
        // ベースとなるルートシグネチャを作成
        // ルートパラメーター
        CD3DX12_ROOT_PARAMETER rootParameters[2]{};
        rootParameters[0].InitAsConstants(32, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
        //テクスチャ用をスロットt0に設定
        CD3DX12_DESCRIPTOR_RANGE discriptorRange{};
        discriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);	//シェーダーリソースビュー(1個)
        rootParameters[1].InitAsDescriptorTable(1, &discriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);	//t0

        // サンプラー
        CD3DX12_STATIC_SAMPLER_DESC samplerDesc(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        m_pBaseRootSignature = RootSignature::CreateRootSignature(m_pDevice, 19191919, rootParameters, 2, &samplerDesc, 1);
    }

    void PostProcessManager::RemovePostProcess(void* p)
    {
        auto it = std::find_if(m_processes.begin(), m_processes.end(),
            [p](const std::shared_ptr<PostProcessBase>& process) {
                return process.get() == p;
            });
        if (it != m_processes.end()) {
            m_bFreePostProcessIDs[(*it)->GetPostProcessID()] = true;
            m_processes.erase(it);
        }
    }

    void PostProcessManager::RemoveAllPostProcess()
    {
        m_processes.clear();
        for (UINT i = 0; i < MAX_POSTPROCESS_COUNT; i++) {
            m_bFreePostProcessIDs[i] = true;
        }
        m_nextPostProcessID = 0;
    }

    void PostProcessManager::ChangeRenderSize()
    {
        for (std::shared_ptr<PostProcessBase> postProcess : m_processes) {
            postProcess->Initialize(false);
        }
    }

    const RenderResource& PostProcessManager::Render(const RenderResource& renderResource)
    {
        GetCommandList()->IASetVertexBuffers(0, 1, &g_Engine->GetDirectXDevice()->GetVertexBufferView());      //頂点情報を送信
        GetCommandList()->IASetIndexBuffer(&g_Engine->GetDirectXDevice()->GetIndexBufferView());               //インデックス情報を送信

        GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_lastRenderResource = renderResource;

        for (std::shared_ptr<PostProcessBase> postProcess : m_processes) \
        {
            if (postProcess->m_bEnable)
            {
                postProcess->Render(m_lastRenderResource);
                m_lastRenderResource = postProcess->GetResultRenderTarget();
            }
        }

        return m_lastRenderResource;
    }

    uint32 PostProcessManager::GetEnableCount()
    {
        uint32 count = 0;
        for (const ObjectPtr<PostProcessBase>& pPostProcess : m_processes)
        {
            if (pPostProcess->m_bEnable) {
                count++;
            }
        }

        return count;
    }

    uint32 PostProcessManager::GetNextPostProcessID()
    {
        bool bLooping = false;
        for (uint32 i = m_nextPostProcessID; i < MAX_POSTPROCESS_COUNT; i++) {
            if (m_bFreePostProcessIDs[i])
            {
                m_nextPostProcessID = i;
                m_bFreePostProcessIDs[i] = false;
                break;
            }
            if (i >= MAX_POSTPROCESS_COUNT - 1)
            {
                //ポストプロセスの最大数を超える場合はエラー
                if (bLooping)
                {
                    FatalError("ポストプロセスが規定数を超えているため作成できません。");
                    break;
                }
                bLooping = true;
                i = -1;
                continue;
            }
        }

        return m_nextPostProcessID;
    }
}
