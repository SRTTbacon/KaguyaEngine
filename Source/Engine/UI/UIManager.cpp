#include "UIManager.h"
#include "Core\\UITexture.h"
#include "..\\Engine.h"
#include "..\\Core\\CoreMinimal.h"
#include "..\\Core\\EngineDefine.h"
#include "..\\Core\\EngineKeyString.h"
#include "..\\Core\\DirectXCore\\DirectXDevice.h"
#include "..\\Core\\DescriptorHeap\\DescriptorHeap.h"
#include "..\\Core\\Pipeline\\RootSignature.h"
#include "..\\Core\\Pipeline\\PipelineState.h"
#include "..\\Core\\Pipeline\\PipelineDefine.h"

#include <d3dx12.h>
#include <dxgitype.h>
#include <d3dcommon.h>
#include <dxgiformat.h>
#include <DirectXTex.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <string.h>
#include <wincodec.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <Windows.h>

using namespace KaguyaEngine::AsyncTask;
using namespace KaguyaEngine::Core;

using namespace DirectX;

namespace KaguyaEngine::UI
{
    UIManager::UIManager(DescriptorHeap* pRtvDescriptorHeap, CopyDescriptor* pCopyDescriptor, const XMUINT2* pWindowSize)
        : m_pCopyDescriptor(pCopyDescriptor)
        , m_pRtvDescriptorHeap(pRtvDescriptorHeap)
        , m_pWindowSize(pWindowSize)
        , m_rtvDescriptorHeapIndex(0)
        , m_bExistReleaseObject(false)
    {}

    void UIManager::Initialize()
    {
        CreatePipeline();

        m_rtvDescriptorHeapIndex = m_pRtvDescriptorHeap->GetNextAvailableDescriptor();
        ReBuildRenderTexture();
    }

    void UIManager::ReBuildRenderTexture()
    {
        //中間テクスチャをフレーム数分作成
        D3D12_RESOURCE_DESC desc2 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_pWindowSize->x, m_pWindowSize->y);
        desc2.SampleDesc.Count = 1;
        desc2.MipLevels = 1;
        desc2.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        for (uint32 i = 0; i < 4; i++) {
            clearValue.Color[i] = 0.0f;
        }

        CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        HRESULT hr = m_pDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &desc2,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(m_pUIRenderTarget.ReleaseAndGetAddressOf())
        );
        if (FAILED(hr)) {
            return;
        }
        m_pUIRenderTarget->SetName(L"UIManager::ReBuildRenderTexture");

        //中間テクスチャ用のRTVを作成
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        m_pDevice->CreateRenderTargetView(m_pUIRenderTarget.Get(), &rtvDesc, m_pRtvDescriptorHeap->GetCpuDescriptorHandle(m_rtvDescriptorHeapIndex));
    }

    UITextureRef UIManager::AddUITexture()
    {
        UITextureRef uiTexture = CreateObjectPtr<UITexture>(m_pCopyDescriptor, m_pWindowSize, &m_bExistReleaseObject);
        uiTexture->SetFrameResource(this);
        DirectXDevice* pDirectXDevice = g_Engine->GetDirectXDevice();
        uiTexture->Initialize(pDirectXDevice->GetVertexBuffer(), pDirectXDevice->GetIndexBuffer(), &pDirectXDevice->GetVertexBufferView(), &pDirectXDevice->GetIndexBufferView());

        m_uiElements.push_back(uiTexture);

        return uiTexture;
    }

    UITextureRef UIManager::AddUITexture(std::string strFilePath, bool bDDSFile)
    {
        UITextureRef pUITexture = AddUITexture();
        pUITexture->SetTexture(strFilePath, bDDSFile);
        pUITexture->m_size = pUITexture->GetTextureSize();
        return pUITexture;
    }

    UITextureRef UIManager::AddUITexture(std::vector<char>& data, bool bDDSFile)
    {
        UITextureRef pUITexture = AddUITexture();
        pUITexture->SetTexture(data, bDDSFile);
        pUITexture->m_size = pUITexture->GetTextureSize();
        return pUITexture;
    }

    UITextureRef UIManager::AddUITexture(const char* data, size_t size, bool bDDSFile)
    {
        UITextureRef pUITexture = AddUITexture();
        pUITexture->SetTexture(data, size, bDDSFile);
        pUITexture->m_size = pUITexture->GetTextureSize();
        return pUITexture;
    }

    UITextureRef UIManager::AddUITexture(DXGI_RGBA color)
    {
        UITextureRef pUITexture = AddUITexture();
        pUITexture->SetTexture(color);
        return pUITexture;
    }

    UITextureRef UIManager::AddUITextureCircle(DXGI_RGBA color, int size)
    {
        UITextureRef pUITexture = AddUITexture();

        ScratchImage img;
        img.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, size, size, 1, 1);

        uint8* pixels = img.GetPixels();
        size_t pitch = img.GetImages()->rowPitch;

        float cx = size / 2.0f;
        float cy = size / 2.0f;
        float radius = 28.0f;

        auto ToByte = [](float v) -> uint8
            {
                v = std::clamp(v, 0.0f, 1.0f);
                return static_cast<uint8>(v * 255.0f + 0.5f);
            };

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                float dx = x - cx;
                float dy = y - cy;
                float dist = sqrtf(dx * dx + dy * dy);

                uint8* p = pixels + y * pitch + x * 4;

                float edgeWidth = 2.0f;

                float alpha = 1.0f - std::clamp((dist - radius) / edgeWidth, 0.0f, 1.0f);

                p[0] = ToByte(color.r);
                p[1] = ToByte(color.g);
                p[2] = ToByte(color.b);
                p[3] = ToByte(color.a * alpha);
            }
        }

        // PNG形式のメモリに変換
        Blob blob;
        HRESULT hr = SaveToWICMemory(*img.GetImage(0, 0, 0), WIC_FLAGS_NONE, GUID_ContainerFormatPng, blob);

        std::vector<char> buffer;

        if (SUCCEEDED(hr))
        {
            buffer.resize(blob.GetBufferSize());
            memcpy(buffer.data(), blob.GetBufferPointer(), blob.GetBufferSize());
            pUITexture->SetTexture(buffer);
            pUITexture->m_size = pUITexture->GetTextureSize();
        }

        return pUITexture;
    }

    void UIManager::LateUpdate()
    {
		// 解放フラグが立っているUI要素を削除
        if (m_bExistReleaseObject)
        {
            std::erase_if(m_uiElements, [&](const UIElementRef& element) {
                return element->GetIsReleased();
                });
        }
    }

    void UIManager::BeginRender()
    {
        if (m_uiElements.size() <= 0) {
            return;
        }

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pUIRenderTarget.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        GetCommandList()->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvDescriptorHeap->GetCpuDescriptorHandle(m_rtvDescriptorHeapIndex);
        float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        GetCommandList()->ClearRenderTargetView(rtvHandle, color, 0, nullptr);
        GetCommandList()->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

        ID3D12PipelineState* pState = m_pPipelineState->GetPipelineState();

        GetCommandList()->SetGraphicsRootSignature(m_pRootSignature->GetRootSignature());   // ルートシグネチャを設定
        GetCommandList()->SetPipelineState(m_pPipelineState->GetPipelineState());           // パイプラインステートを設定

        DirectXDevice* pDirectXDevice = g_Engine->GetDirectXDevice();
        GetCommandList()->IASetVertexBuffers(0, 1, &pDirectXDevice->GetVertexBufferView()); // 頂点情報を送信
        GetCommandList()->IASetIndexBuffer(&pDirectXDevice->GetIndexBufferView());          // インデックス情報を送信
        GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);      // 3角ポリゴンのみ
    }

    void UIManager::EndRender()
    {
        if (m_uiElements.size() <= 0) {
            return;
        }

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pUIRenderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        GetCommandList()->ResourceBarrier(1, &barrier);
    }

    void UIManager::CreatePipeline()
    {
        // RootSignatureの設定
        const uint32 rootParamCount = 3;
        CD3DX12_ROOT_PARAMETER rootParameters[rootParamCount]{};
        // UIConstants(b0)を設定。 XMFLOAT2が6つ、XMMATRIXが2つあるため48
        rootParameters[0].InitAsConstants(48, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
        // テクスチャ用をスロットt0を設定
		CD3DX12_DESCRIPTOR_RANGE descriptorRange[2]{};
        descriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
		rootParameters[1].InitAsDescriptorTable(1, descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
        // UIBuffer(b0) ※ピクセルシェーダー。色情報+モードのため'5'を設定
        rootParameters[2].InitAsConstants(5, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

        // PipelineStateの設定
		const uint32 inputElementCount = 2;
        D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount]{};
        inputElements[0] = D3D12_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
        inputElements[1] = D3D12_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
        CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // PsoDescKeyの設定
        PsoDescKey desc{};
        desc.cullMode = D3D12_CULL_MODE_FRONT;
        desc.textureAddressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.vertexShaderPath = KeyString::SHADER_UI_TEXTURE_VS;
        desc.pixelShaderPath = KeyString::SHADER_UI_TEXTURE_PS;
		desc.depthStencilPreset = DepthStencilPreset_UI;
        desc.numRenderTargets = 1;
		desc.rtvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.bAlphaBlend = true;

        uint64 shaderHashID = GetShaderHash(desc.vertexShaderPath, desc.pixelShaderPath);

		m_pRootSignature = CreateObjectPtr<RootSignature>();
		m_pPipelineState = CreateObjectPtr<PipelineState>();
		m_pRootSignature->Initialize(m_pDevice, shaderHashID, rootParameters, rootParamCount, &sampler, 1);
		m_pPipelineState->Initialize(m_pDevice, inputElements, inputElementCount, shaderHashID, desc);
    }
}