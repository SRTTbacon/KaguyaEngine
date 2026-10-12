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

		//m_cbTAAResolve.Resolution = XMFLOAT2(0.0f, 0.0f);
		m_cbTAAResolve.Resolution = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		//m_cbTAAResolve.Jitter = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		m_cbTAAResolve.Jitter = XMFLOAT2(0.0f, 0.0f);
		m_cbTAAResolve.FrameNumber = 0;
		m_cbTAAResolve.DebugFlags = 0; // AllowLongestVelocityVector | AllowNeighbourhoodSampling | AllowYCoCg | AllowVarianceClipping | AllowBicubicFilter | AllowDepthThreshold | MarkNoHistoryPixels
	}

	void PTAA::Initialize(bool bFirstInit)
	{
		uint32 renderTargetCount = static_cast<uint32>(_countof(m_historyRenderTarget));

		// m_pDescriptorはコンストラクタではまだnullptrなので、ここで初めてSRVの開始位置を取得する
		if (bFirstInit)
		{
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

		// CBを初期化
		XMUINT2 renderSize = m_pCamera->GetRenderSize();
		//m_cbTAAResolve.Resolution = XMFLOAT2(1.0f / static_cast<float>(renderSize.x), 1.0f / static_cast<float>(renderSize.y));
		m_cbTAAResolve.Resolution = XMFLOAT4(static_cast<float>(renderSize.x), static_cast<float>(renderSize.y), 1.0f / static_cast<float>(renderSize.x), 1.0f / static_cast<float>(renderSize.y));

		if (bFirstInit)
		{ 
			// RootSignatureの作成
			CD3DX12_DESCRIPTOR_RANGE srvRange{};
			srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0); // t0 - t2
			//srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0); // t0 - t4
			CD3DX12_DESCRIPTOR_RANGE uavRange{};
			uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

			CD3DX12_ROOT_PARAMETER rootParams[2]{};

			rootParams[0].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_ALL);
			rootParams[1].InitAsDescriptorTable(1, &uavRange, D3D12_SHADER_VISIBILITY_ALL);
			//rootParams[2].InitAsConstants(sizeof(CBTAAResolve) / sizeof(uint32), 0, 0, D3D12_SHADER_VISIBILITY_ALL);

			CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

			PsoDescKey desc{};
			desc.csShaderPath = GetShaderPath("TAATestCS.cso");

			uint64 ShaderHash = GetShaderHash(desc.csShaderPath, "");
			m_pTAARootSignature = RootSignature::CreateRootSignature(m_pDevice, ShaderHash, rootParams, _countof(rootParams), &sampler, 1);
			m_pTAAPipelineState = PipelineState::CreatePipelineState(m_pDevice, m_pTAARootSignature, desc);
		}
	}

	void PTAA::Render(RenderResource& mainRenderTarget)
	{
		// CBを更新
		m_cbTAAResolve.Jitter.x = m_pCamera->GetJitter().x;
		m_cbTAAResolve.Jitter.y = m_pCamera->GetJitter().y;
		//m_cbTAAResolve.Jitter.z = m_pCamera->GetJitterPrev().x;
		//m_cbTAAResolve.Jitter.w = m_pCamera->GetJitterPrev().y;

		m_cbTAAResolve.FrameNumber++;

		uint8 prevHistoryIndex = m_currentHistoryIndex;
		m_currentHistoryIndex = (m_currentHistoryIndex + 1) % _countof(m_historyRenderTarget);

		ID3D12Resource* pResources[] = {
			mainRenderTarget.pResource,
			m_historyRenderTarget[prevHistoryIndex].pResource.Get(),
			m_pCameraResource->velocityResource.pResource,
			//m_pCameraResource->depthResource.pResource,
			//m_pCameraResource->prevDepthResource.pResource
		};

		// リソースバリアをTAA用に変更
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		for (int i = 0; i < _countof(pResources); i++) {
			ChangeBarrier(pResources[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, barriers);
		}
		ChangeBarrier(m_historyRenderTarget[m_currentHistoryIndex].pResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, barriers);
		GetCommandList()->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
		barriers.clear();

		// SRVをコピー
		//uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor(4U);
		uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor(6U);
		//uint32 uavCopyIndex = copyIndex + 3;
		uint32 uavCopyIndex = copyIndex + 5;
		CopyDescriptors(1, mainRenderTarget.mainSrvIndex, copyIndex + 0);
		CopyDescriptors(1, m_historyRenderTarget[prevHistoryIndex].mainSrvIndex, copyIndex + 1);
		CopyDescriptors(1, m_pCameraResource->velocityResource.mainSrvIndex, copyIndex + 2);
		//CopyDescriptors(1, m_pCameraResource->depthResource.mainSrvIndex, copyIndex + 3);
		//CopyDescriptors(1, m_pCameraResource->prevDepthResource.mainSrvIndex, copyIndex + 4);
		CopyDescriptors(1, m_historyRenderTarget[m_currentHistoryIndex].mainUavIndex, uavCopyIndex);

		// パイプラインを設定
		GetCommandList()->SetPipelineState(m_pTAAPipelineState);
		GetCommandList()->SetComputeRootSignature(m_pTAARootSignature);
		GetCommandList()->SetComputeRootDescriptorTable(0, m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyIndex));
		GetCommandList()->SetComputeRootDescriptorTable(1, m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(uavCopyIndex));
		//GetCommandList()->SetComputeRoot32BitConstants(2, sizeof(CBTAAResolve) / sizeof(uint32), &m_cbTAAResolve, 0);

		// ディスパッチ
		const UINT width = m_pCamera->GetRenderSize().x;
		const UINT height = m_pCamera->GetRenderSize().y;

		GetCommandList()->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

		// リソースバリアを戻す
		for (int i = 0; i < _countof(pResources); i++) {
			ChangeBarrier(pResources[i], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, barriers);
		}
		ChangeBarrier(m_historyRenderTarget[m_currentHistoryIndex].pResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, barriers);
		GetCommandList()->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
		barriers.clear();
	}

	RenderResource PTAA::GetResultRenderTarget() const
	{
		RenderResource outResource{};

		outResource.pResource = m_historyRenderTarget[m_currentHistoryIndex].pResource.Get();
		outResource.mainSrvHandleCPU = m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(m_historyRenderTarget[m_currentHistoryIndex].mainSrvIndex);
		outResource.mainSrvHandleGPU = m_pDescriptor->pMainHeap->GetGpuDescriptorHandle(m_historyRenderTarget[m_currentHistoryIndex].mainSrvIndex);
		uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor();
		CopyDescriptors(1, outResource.mainSrvHandleCPU, copyIndex);
		outResource.copySrvHandleCPU = m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex);
		outResource.copySrvHandleGPU = m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyIndex);
		outResource.mainSrvIndex = m_historyRenderTarget[m_currentHistoryIndex].mainSrvIndex;

		return outResource;
	}
}
