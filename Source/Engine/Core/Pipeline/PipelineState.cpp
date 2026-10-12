#include "PipelineState.h"
#include "RootSignature.h"
#include "PipelineDefine.h"
#include "..\\CoreMinimal.h"
#include "..\\Main\\Utility.h"

#include <d3dcompiler.h>
#include <d3dcommon.h>
#include <d3dx12.h>
#include <cstdio>
#include <dxgiformat.h>
#include <Windows.h>
#include <d3d12.h>
#include <unordered_map>
#include <cstdint>

namespace KaguyaEngine::Core
{
	// --------------------------PipelineState-------------------------

    PipelineState::PipelineState()
        : m_hashKey(0)
        , m_pPipelineState(nullptr)
    {
    }

    ID3D12PipelineState* PipelineState::CreatePipelineState(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, uint64 rootSignatureHash, PsoDescKey& desc)
    {
        uint64 tempHash;
        return CreatePipelineState(pDevice, pInputElementDescs, elementCount, rootSignatureHash, desc, tempHash);
    }

    ID3D12PipelineState* PipelineState::CreatePipelineState(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, uint64 rootSignatureHash, PsoDescKey& desc, uint64& outHashID)
    {
        PipelineState pipeline;
        pipeline.Initialize(pDevice, pInputElementDescs, elementCount, rootSignatureHash, desc);
        outHashID = pipeline.m_hashKey;
        return pipeline.GetPipelineState();
    }

    ID3D12PipelineState* PipelineState::CreatePipelineState(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, ID3D12RootSignature* pRootSignature, PsoDescKey& desc)
    {
        uint64 tempHash;
        return CreatePipelineState(pDevice, pInputElementDescs, elementCount, pRootSignature, desc, tempHash);
    }

    ID3D12PipelineState* PipelineState::CreatePipelineState(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, ID3D12RootSignature* pRootSignature, PsoDescKey& desc, uint64& outHashID)
    {
        PipelineState pipeline;
        pipeline.Initialize(pDevice, pInputElementDescs, elementCount, pRootSignature, desc);
        outHashID = pipeline.m_hashKey;
        return pipeline.GetPipelineState();
    }

	ID3D12PipelineState* PipelineState::CreatePipelineState(ID3D12Device* pDevice, ID3D12RootSignature* pRootSignature, PsoDescKey& desc)
	{
		uint64 tempHash;
		return CreatePipelineState(pDevice, pRootSignature, desc, tempHash);
	}

    ID3D12PipelineState* PipelineState::CreatePipelineState(ID3D12Device* pDevice, ID3D12RootSignature* pRootSignature, PsoDescKey& desc, uint64& outHashID)
    {
        PipelineState pipeline;
        pipeline.Initialize(pDevice, pRootSignature, desc);
        outHashID = pipeline.m_hashKey;
        return pipeline.GetPipelineState();
    }

    void PipelineState::Initialize(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, uint64 rootSignatureHash, PsoDescKey& desc)
    {
        if (g_rootSignatureCache.find(rootSignatureHash) == g_rootSignatureCache.end())
        {
            printf("RootSignatureが無効です。\n");
            return;
        }

        Initialize(pDevice, pInputElementDescs, elementCount, g_rootSignatureCache[rootSignatureHash].Get(), desc);
    }

    void PipelineState::Initialize(ID3D12Device* pDevice, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs, uint32 elementCount, ID3D12RootSignature* pRootSignature, PsoDescKey& desc)
    {
        uint64 hash = desc.GetHash();
        m_hashKey = hash;
        if (g_pipelineStateCache.find(hash) != g_pipelineStateCache.end()) {
            m_pPipelineState = g_pipelineStateCache[hash].pPipelineState.Get();
            return;
        }

        //パイプラインステートオブジェクトの作成
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

        // 入力レイアウトの定義
        psoDesc.InputLayout = { pInputElementDescs, elementCount };

        psoDesc.pRootSignature = pRootSignature;     // ルートシグネチャを指定

        // シェーダーを設定
        ComPtr<ID3DBlob> pVsBlob, pPsBlob;
        psoDesc.VS = { nullptr, 0 };
        psoDesc.PS = { nullptr, 0 };
        if (!desc.vertexShaderPath.empty())
        {
            HRESULT hr = 0;
            hr = D3DReadFileToBlob(GetWideString(desc.vertexShaderPath).c_str(), &pVsBlob);

            if (FAILED(hr) || !pVsBlob)
            {
                printf("頂点シェーダーの読み込みに失敗。エラーコード:%lx : %s\n", hr, desc.vertexShaderPath.c_str());
                return;
            }
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(pVsBlob.Get());

            if (!desc.pixelShaderPath.empty())
            {
                hr = D3DReadFileToBlob(GetWideString(desc.pixelShaderPath).c_str(), &pPsBlob);
                if (FAILED(hr) || !pPsBlob)
                {
                    printf("ピクセルシェーダーの読み込みに失敗。エラーコード:%lx : %s\n", hr, desc.pixelShaderPath.c_str());
                    return;
                }
                psoDesc.PS = CD3DX12_SHADER_BYTECODE(pPsBlob.Get());
            }
        }

        ComPtr<ID3DBlob> pHsBlob;
        ComPtr<ID3DBlob> pDsBlob;

        // 海の場合、ハルシェーダーとドメインシェーダーも読み込む
        if (!desc.domainShaderPath.empty() && !desc.hullShaderPath.empty()) {
            HRESULT hr = D3DReadFileToBlob(GetWideString(desc.hullShaderPath).c_str(), &pHsBlob);
            HRESULT hr2 = D3DReadFileToBlob(GetWideString(desc.domainShaderPath).c_str(), &pDsBlob);
            if (FAILED(hr) || FAILED(hr2) || !pHsBlob || !pDsBlob) {
                printf("ハルシェーダー / ドメインシェーダーの読み込みに失敗。エラーコード:%lx / %lx\n", hr, hr2);
                return;
            }
            psoDesc.HS = CD3DX12_SHADER_BYTECODE(pHsBlob.Get());
            psoDesc.DS = CD3DX12_SHADER_BYTECODE(pDsBlob.Get());
        }

        // ラスタライザーステート（デフォルト）
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.CullMode = desc.cullMode;

        // ブレンドステート（デフォルト）
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

        for (uint32 i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
            psoDesc.BlendState.RenderTarget[i] = desc.blendDesc;
        }

        // ブレンド状態の設定
        if (desc.bAlphaBlend) {
            D3D12_BLEND_DESC& blendDesc = psoDesc.BlendState;
            psoDesc.BlendState.AlphaToCoverageEnable = TRUE;
            psoDesc.BlendState.IndependentBlendEnable = TRUE;

            blendDesc.AlphaToCoverageEnable = FALSE;
            blendDesc.IndependentBlendEnable = FALSE;

            auto& rtBlend = blendDesc.RenderTarget[0];
            rtBlend.BlendEnable = TRUE;
            rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
            rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
            rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
            rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // 深度・ステンシルステート（デフォルト）
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

        // スカイボックスは深度バッファを書き込まないように
        if (desc.depthStencilPreset == DepthStencilPreset_Skybox) {
            psoDesc.DepthStencilState.DepthEnable = true;
            psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        }
        else if (desc.depthStencilPreset == DepthStencilPreset_UI) {
            // 常に手前
            psoDesc.DepthStencilState.DepthEnable = false;
            psoDesc.DepthStencilState.StencilEnable = false;

            if (desc.bAlphaBlend) {
                // UIは半透明対応
                SetAlphaBlend(psoDesc.BlendState);
            }
        }
        else if (desc.bDisableDepth) {
            psoDesc.DepthStencilState.DepthEnable = false;      // 深度テストを無効化
            psoDesc.DepthStencilState.StencilEnable = false;    // ステンシルを無効化
            psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        }

        psoDesc.SampleMask = UINT32_MAX;
        psoDesc.PrimitiveTopologyType = desc.primitiveTopologyType;

        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.NumRenderTargets = desc.numRenderTargets;
        for (uint32 i = 0; i < desc.numRenderTargets; ++i) {
            psoDesc.RTVFormats[i] = desc.rtvFormats[i];
        }

        // マルチサンプリングの設定
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        PsoInfo psoInfo{};
        psoInfo.desc = desc;
        psoInfo.rootSignatureHash = 0;
        psoInfo.pRootSignature = pRootSignature;

		for (const auto& pair : g_rootSignatureCache)
        {
			if (pair.second.Get() == pRootSignature)
            {
				psoInfo.rootSignatureHash = pair.first;
				break;
			}
		}

        // パイプラインステートの作成
        HRESULT hr = pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&psoInfo.pPipelineState));
        if (FAILED(hr)) {
            printf("パイプラインステートの作成に失敗しました。エラーコード:%lx vs:%s ps:%s\n", hr, desc.vertexShaderPath.c_str(), desc.pixelShaderPath.c_str());
        }

        m_pPipelineState = psoInfo.pPipelineState.Get();

        g_pipelineStateCache.emplace(hash, psoInfo);
    }

    void PipelineState::Initialize(ID3D12Device* pDevice, ID3D12RootSignature* pRootSignature, PsoDescKey& desc)
    {
        uint64 hash = desc.GetHash();
        m_hashKey = hash;
        if (g_pipelineStateCache.find(hash) != g_pipelineStateCache.end())
        {
            m_pPipelineState = g_pipelineStateCache[hash].pPipelineState.Get();
            return;
        }

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};

        ComPtr<ID3DBlob> pCSBlob;
        HRESULT hr = D3DReadFileToBlob(GetWideString(desc.csShaderPath).c_str(), &pCSBlob);
        psoDesc.CS = CD3DX12_SHADER_BYTECODE(pCSBlob.Get());

        psoDesc.pRootSignature = pRootSignature;

        PsoInfo psoInfo{};
        psoInfo.rootSignatureHash = 0;
        psoInfo.pRootSignature = pRootSignature;

        for (const auto& pair : g_rootSignatureCache)
        {
            if (pair.second.Get() == pRootSignature)
            {
                psoInfo.rootSignatureHash = pair.first;
                break;
            }
        }

        // パイプラインステートの作成
        hr = pDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&psoInfo.pPipelineState));
        if (FAILED(hr)) {
            printf("パイプラインステートの作成に失敗しました。エラーコード:%lx cs:%s\n", hr, desc.csShaderPath.c_str());
        }

        m_pPipelineState = psoInfo.pPipelineState.Get();

        g_pipelineStateCache.emplace(hash, psoInfo);
    }

    void PipelineState::Release()
    {
        DeletePipelineState(m_pPipelineState);
        m_pPipelineState = nullptr;
        m_hashKey = 0;
    }

    void PipelineState::SetAlphaBlend(D3D12_BLEND_DESC& desc)
    {
        //ブレンドを有効にする
        desc.RenderTarget[0].BlendEnable = TRUE;
        //ピクセルシェーダーが出力するRGB値に対してαを乗算する(SRCrgb ＊ SRCα)
        desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        //レンダーターゲットの現在のRGB値に対して1-αを乗算する(DESTrgb ＊ (1 ー SRCα))
        desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

        desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    }
}