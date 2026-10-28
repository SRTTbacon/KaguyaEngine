//*****************************************************************************
//
// アンチエイリアシング : TAA (Temporal Anti-Aliasing)
//
// TAA.cpp
//
// T_Takeuchi
//
// 2026/9/27
//
//*****************************************************************************

#include "TAA.h"
#include "..\\Core\\PostProcessBase.h"
#include "..\\..\\Camera\\Camera.h"
#include "..\\..\\..\\Core\\DescriptorHeap\\DescriptorHeap.h"
#include "..\\..\\..\\Core\\Pipeline\\RootSignature.h"
#include "..\\..\\..\\Core\\Pipeline\\PipelineState.h"

#include <d3dx12.h>

using namespace KaguyaEngine::Core;
using namespace DirectX;

namespace KaguyaEngine::PostProcess
{
	PTAA::PTAA(uint32 postProcessID, const KaguyaEngine::Camera* pCamera, KaguyaEngine::Core::DescriptorHeap* pRtvHeap, ID3D12RootSignature* pRootSignature)
		: PostProcessBase(postProcessID, pCamera, pRtvHeap, pRootSignature)
		, m_currentHistoryIndex(0)
		, m_pTAARootSignature(nullptr)
		, m_pTAAPipelineState(nullptr)
	{
		m_rtvStartIndex = m_pRtvHeap->GetNextAvailableDescriptor(static_cast<uint32>(_countof(m_historyRenderTarget)));
	}

	void PTAA::Initialize(bool bFirstInit)
	{
		uint32 renderTargetCount = static_cast<uint32>(_countof(m_historyRenderTarget));

		// m_pDescriptorはコンストラクタではまだnullptrなので、ここでSRVの開始位置を取得する
		if (bFirstInit) {
			m_srvStartIndex = m_pDescriptor->pMainHeap->GetNextAvailableDescriptor(renderTargetCount * 2);
		}

		//リソースの作成位置
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvHeap->GetCpuDescriptorHandle(m_rtvStartIndex);

		XMFLOAT4 clearColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		for (uint32 i = 0; i < renderTargetCount; i++)
		{
			CreateRenderTexture(clearColor, m_historyRenderTarget[i], rtvHandle, DXGI_FORMAT_R16G16B16A16_FLOAT, 1.0f, false, std::to_string(i));
			SetSRVResource(m_historyRenderTarget[i], m_srvStartIndex + i * 2);
			SetUAVResource(m_historyRenderTarget[i], m_srvStartIndex + i * 2 + 1);
		}

		if (bFirstInit)
		{ 
			// RootSignatureの作成
			CD3DX12_DESCRIPTOR_RANGE srvRange{};
			srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0); // t0 - t2
			CD3DX12_DESCRIPTOR_RANGE uavRange{};
			uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

			CD3DX12_ROOT_PARAMETER rootParams[2]{};

			rootParams[0].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_ALL);
			rootParams[1].InitAsDescriptorTable(1, &uavRange, D3D12_SHADER_VISIBILITY_ALL);

			CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

			PsoDescKey desc{};
			desc.csShaderPath = GetShaderPath("TAACS.cso");

			uint64 ShaderHash = GetShaderHash(desc.csShaderPath, "");
			m_pTAARootSignature = RootSignature::CreateRootSignature(m_pDevice, ShaderHash, rootParams, _countof(rootParams), &sampler, 1);
			m_pTAAPipelineState = PipelineState::CreatePipelineState(m_pDevice, m_pTAARootSignature, desc);
		}
	}

	void PTAA::Render(RenderResource& mainRenderTarget)
	{
		uint8 prevHistoryIndex = m_currentHistoryIndex;
		m_currentHistoryIndex = static_cast<uint8>((m_currentHistoryIndex + 1)) % _countof(m_historyRenderTarget);

		const PPResource& prevResource = m_historyRenderTarget[prevHistoryIndex];
		const PPResource& currResource = m_historyRenderTarget[m_currentHistoryIndex];

		ID3D12Resource* pResources[] = {
			mainRenderTarget.pResource,
			prevResource.pResource.Get(),
			m_pCameraResource->velocityResource.pResource,
		};

		// リソースバリアをTAA用に変更
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		for (int i = 0; i < _countof(pResources); i++) {
			ChangeBarrier(pResources[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, barriers);
		}
		ChangeBarrier(currResource.pResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, barriers);
		GetCommandList()->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
		barriers.clear();

		// SRVをコピー
		uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor(4U);
		uint32 uavCopyIndex = copyIndex + 3;
		CopyDescriptors(1, mainRenderTarget.mainSrvIndex, copyIndex + 0);
		CopyDescriptors(1, prevResource.mainSrvIndex, copyIndex + 1);
		CopyDescriptors(1, m_pCameraResource->velocityResource.mainSrvIndex, copyIndex + 2);
		CopyDescriptors(1, currResource.mainUavIndex, uavCopyIndex);

		// パイプラインを設定
		GetCommandList()->SetPipelineState(m_pTAAPipelineState);
		GetCommandList()->SetComputeRootSignature(m_pTAARootSignature);
		GetCommandList()->SetComputeRootDescriptorTable(0, m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyIndex));
		GetCommandList()->SetComputeRootDescriptorTable(1, m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(uavCopyIndex));

		// 描画
		const uint32 width = m_pCamera->GetRenderSize().x;
		const uint32 height = m_pCamera->GetRenderSize().y;
		GetCommandList()->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

		// リソースバリアを戻す
		for (int i = 0; i < _countof(pResources); i++) {
			ChangeBarrier(pResources[i], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, barriers);
		}
		ChangeBarrier(currResource.pResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, barriers);
		GetCommandList()->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
	}

	RenderResource PTAA::GetResultRenderTarget() const
	{
		RenderResource outResource{};
		const PPResource& inResource = m_historyRenderTarget[m_currentHistoryIndex];

		// PPResourceをRenderResourceに変換
		outResource.pResource = inResource.pResource.Get();
		outResource.mainSrvHandleCPU = m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(inResource.mainSrvIndex);
		outResource.mainSrvHandleGPU = m_pDescriptor->pMainHeap->GetGpuDescriptorHandle(inResource.mainSrvIndex);
		uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor();
		CopyDescriptors(1, outResource.mainSrvHandleCPU, copyIndex);
		outResource.copySrvHandleCPU = m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex);
		outResource.copySrvHandleGPU = m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyIndex);
		outResource.mainSrvIndex = inResource.mainSrvIndex;

		return outResource;
	}
}
