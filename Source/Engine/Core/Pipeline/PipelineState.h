//*****************************************************************************
//
// PipelineStateの作成/管理
//
// PipelineState.h
//
// T_Takeuchi
//
// 2026/5/30
//
//*****************************************************************************

#pragma once

#include "..\\CoreMinimal.h"
#include "RootSignature.h"
#include "PipelineDefine.h"

#include <d3d12.h>
#include <unordered_map>

namespace KaguyaEngine::Core
{
    class PipelineState
    {
    public:
        PipelineState();

        static ID3D12PipelineState* CreatePipelineState(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, uint64 rootSignatureHash, PsoDescKey& desc);
        static ID3D12PipelineState* CreatePipelineState(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, uint64 rootSignatureHash, PsoDescKey& desc, uint64& outHashID);
        static ID3D12PipelineState* CreatePipelineState(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, ID3D12RootSignature* pRootSignature, PsoDescKey& desc);
        static ID3D12PipelineState* CreatePipelineState(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, ID3D12RootSignature* pRootSignature, PsoDescKey& desc, uint64& outHashID);
        static ID3D12PipelineState* CreatePipelineState(ID3D12Device* pDevice, ID3D12RootSignature* pRootSignature, PsoDescKey& desc);
        static ID3D12PipelineState* CreatePipelineState(ID3D12Device* pDevice, ID3D12RootSignature* pRootSignature, PsoDescKey& desc, uint64& outHashID);

		void Initialize(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, uint64 rootSignatureHash, PsoDescKey& desc);
		void Initialize(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, ID3D12RootSignature *pRootSignature, PsoDescKey& desc);
		void Initialize(ID3D12Device* pDevice, ID3D12RootSignature *pRootSignature, PsoDescKey& desc);

        void Release();

		ID3D12PipelineState* GetPipelineState() const { return m_pPipelineState; }
		PsoDescKey& GetDesc() const { return g_pipelineStateCache[m_hashKey].desc; }

    private:
		ID3D12PipelineState* m_pPipelineState; // パイプラインステートのCOMポインタ
		uint64 m_hashKey;

        void SetAlphaBlend(D3D12_BLEND_DESC& desc);
    };
}
