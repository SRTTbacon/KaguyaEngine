#include "DepthOfField.h"
#include "Core\\PostProcessSampling.h"
#include "..\\Camera\\Camera.h"
#include "..\\..\\Core\\DescriptorHeap\\DescriptorHeap.h"
#include "..\\..\\Core\\Pipeline\\PipelineState.h"
#include "..\\..\\Core\\EngineKeyString.h"

#include <d3dx12.h>

using namespace DirectX;
using namespace KaguyaEngine::Core;

namespace KaguyaEngine::PostProcess
{
	PDepthOfField::PDepthOfField(uint32 postProcessID, const KaguyaEngine::Camera* pCamera, KaguyaEngine::Core::DescriptorHeap* pRtvHeap, ID3D12RootSignature* pRootSignature)
		: PostProcessBase(postProcessID, pCamera, pRtvHeap, pRootSignature)
		, m_blurParams(BlurParams())
		, m_pCoCPipelineState(nullptr)
		, m_pDoFBlurPipelineState(nullptr)
		, m_pDoFPipelineState(nullptr)
	{
		// 初期設定
		m_dofSettings.focusDistance = 23.0f;
		m_dofSettings.focusRange = 1000.0f;
		m_dofSettings.maxCoC = 12.0f;
		m_dofSettings.nearZ = 0.04f;
		m_dofSettings.farZ = 1000.0f;

		m_pPostProcessSampling = CreateObjectPtr<PostProcessSampling>();

		m_rtvStartIndex = m_pRtvHeap->GetNextAvailableDescriptor(4U);
	}

	void PDepthOfField::Initialize(bool bFirstInit)
	{
		m_pPostProcessSampling->SetFrameResource(this);
		m_pPostProcessSampling->Initialize(DXGI_FORMAT_R16G16B16A16_FLOAT, m_pBaseRootSignature);

		// m_pDescriptorはコンストラクタではまだnullptrなので、ここで初めてSRVの開始位置を取得する
		if (bFirstInit)
		{
			m_srvStartIndex = m_pDescriptor->pMainHeap->GetNextAvailableDescriptor(5U);
		}

		//リソースの作成位置
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvHeap->GetCpuDescriptorHandle(m_rtvStartIndex);

		//レンダリングに必要なリソースを作成
		XMFLOAT4 clearColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		CreateRenderTexture(clearColor, m_mainRenderTarget, rtvHandle, DXGI_FORMAT_R16G16B16A16_FLOAT, 1.0f);
		CreateRenderTexture(clearColor, m_cocRenderTarget, rtvHandle, DXGI_FORMAT_R16_FLOAT, 1.0f);
		CreateRenderTexture(clearColor, m_downScaleRenderTarget, rtvHandle, DXGI_FORMAT_R16G16B16A16_FLOAT, 2.0f);
		CreateRenderTexture(clearColor, m_blurRenderTarget, rtvHandle, DXGI_FORMAT_R16G16B16A16_FLOAT, 2.0f);

		//リソースをSRVに登録 (0番目はメインレンダー)
		SetSRVResource(m_mainRenderTarget, m_srvStartIndex + 0);

		//合成用
		SetSRVResource(m_cocRenderTarget, m_srvStartIndex + 1);
		SetSRVResource(m_blurRenderTarget, m_srvStartIndex + 2);

		//ブラー用
		SetSRVResource(m_downScaleRenderTarget, m_srvStartIndex + 3);
		SetSRVResource(m_cocRenderTarget, m_srvStartIndex + 4);

		m_blurParams.texelSize = XMFLOAT2(1.0f / m_pCamera->m_renderViewport.Width, 1.0f / m_pCamera->m_renderViewport.Height);
		m_blurParams.blurScale = 0.25f;

		if (bFirstInit)
		{
			const std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements = GetBasicVertexLayout();

			PsoDescKey desc{};
			desc.cullMode = D3D12_CULL_MODE_BACK;
			desc.vertexShaderPath = KeyString::SHADER_MAINRENDER_VS;
			desc.pixelShaderPath = KeyString::SHADER_POSTPROCESS_DOF_COC_PS;
			desc.numRenderTargets = 1;
			desc.rtvFormats[0] = DXGI_FORMAT_R16_FLOAT;
			desc.depthStencilPreset = DepthStencilPreset_UI;

			// CoCの作成
			m_pCoCPipelineState = PipelineState::CreatePipelineState(m_pDevice, inputElements.data(), static_cast<uint32>(inputElements.size()), m_pBaseRootSignature, desc);

			// ブラー
			desc.pixelShaderPath = KeyString::SHADER_POSTPROCESS_DOF_BLUR_PS;
			desc.rtvFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			m_pDoFBlurPipelineState = PipelineState::CreatePipelineState(m_pDevice, inputElements.data(), static_cast<uint32>(inputElements.size()), m_pBaseRootSignature, desc);

			// DoF
			desc.pixelShaderPath = KeyString::SHADER_POSTPROCESS_DOF_MAIN_PS;
			desc.rtvFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			m_pDoFPipelineState = PipelineState::CreatePipelineState(m_pDevice, inputElements.data(), static_cast<uint32>(inputElements.size()), m_pBaseRootSignature, desc);
		}
	}

	void PDepthOfField::Render(RenderResource& mainRenderTarget)
	{
		// リソースバリアをレンダーターゲットに変更
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		ChangeBarrier(m_cocRenderTarget.pResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, barriers);
		ChangeBarrier(m_blurRenderTarget.pResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, barriers);
		ChangeBarrier(m_mainRenderTarget.pResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, barriers);

		GetCommandList()->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
		barriers.clear();
		
		//Circle of Confusion (ぼけの強さを計算)
		//ビューポートとシザー矩形を設定
		CD3DX12_VIEWPORT viewPort = CD3DX12_VIEWPORT(m_cocRenderTarget.pResource.Get());
		GetCommandList()->RSSetViewports(1, &viewPort);

		D3D12_RECT rect{ 0, 0, static_cast<int>(viewPort.Width), static_cast<int>(viewPort.Height) };
		GetCommandList()->RSSetScissorRects(1, &rect);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvHeap->GetCpuDescriptorHandle(m_rtvStartIndex + 1);	// m_rtvStartIndex + 1 = CoC (m_cocRenderTarget.rtvHandleはm_srvStartIndex + 4になっている)
		GetCommandList()->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
		GetCommandList()->SetGraphicsRootSignature(m_pBaseRootSignature);
		GetCommandList()->SetPipelineState(m_pCoCPipelineState);
		GetCommandList()->SetGraphicsRoot32BitConstants(0, sizeof(DoFSettings) / 4, &m_dofSettings, 0);

		// 深度テクスチャのみ使用
		PostProcessBase::Draw(m_pCameraResource->depthResource.copySrvHandleGPU);
		
		ChangeBarrier(m_cocRenderTarget.pResource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		//---------------低解像度化---------------
		
		m_pPostProcessSampling->BeginDownSample();
		rtvHandle = m_downScaleRenderTarget.rtvHandle;
		m_pPostProcessSampling->DrawDownSample(rtvHandle, m_downScaleRenderTarget.pResource.Get(), mainRenderTarget.copySrvHandleGPU, m_pCamera->GetRenderSize());
		
		//----------------ブラー----------------
		
		GetCommandList()->SetPipelineState(m_pDoFBlurPipelineState);

		//ビューポートとシザー矩形を設定
		viewPort = CD3DX12_VIEWPORT(m_blurRenderTarget.pResource.Get());
		GetCommandList()->RSSetViewports(1, &viewPort);

		rect = RECT{ 0, 0, static_cast<int>(viewPort.Width), static_cast<int>(viewPort.Height) };
		GetCommandList()->RSSetScissorRects(1, &rect);

		rtvHandle = m_blurRenderTarget.rtvHandle;
		GetCommandList()->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

		GetCommandList()->SetGraphicsRoot32BitConstants(0, sizeof(BlurParams) / 4, &m_blurParams, 0);

		// 低解像度化したテクスチャとCoCテクスチャをセット
		uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor(2U);
		CopyDescriptors(2, m_downScaleRenderTarget.mainSrvIndex, copyIndex);

		//ブラーを描画
		PostProcessBase::Draw(m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyIndex));
		
		ChangeBarrier(m_blurRenderTarget.pResource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		//-----Depth of Field(合成)-----
		
		GetCommandList()->SetPipelineState(m_pDoFPipelineState);

		//ビューポートとシザー矩形を設定
		viewPort = CD3DX12_VIEWPORT(m_mainRenderTarget.pResource.Get());
		GetCommandList()->RSSetViewports(1, &viewPort);

		rect = RECT{ 0, 0, static_cast<int>(viewPort.Width), static_cast<int>(viewPort.Height) };
		GetCommandList()->RSSetScissorRects(1, &rect);

		rtvHandle = m_mainRenderTarget.rtvHandle;
		GetCommandList()->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

		copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor(3U);
		CopyDescriptors(1, mainRenderTarget.mainSrvHandleCPU, copyIndex);
		CopyDescriptors(2, m_srvStartIndex + 1, copyIndex + 1);		// m_srvStartIndex + 1 = CoC, m_srvStartIndex + 2 = Blur

		//最終結果を描画
		PostProcessBase::Draw(m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyIndex));
		
		ChangeBarrier(m_mainRenderTarget.pResource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	inline RenderResource PDepthOfField::GetResultRenderTarget() const
	{
#if 0
		RenderResource outResource{};
		outResource.pResource = m_downScaleRenderTarget.pResource.Get();
		outResource.mainSrvHandleCPU = m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(m_downScaleRenderTarget.mainSrvIndex);
		outResource.mainSrvHandleGPU = m_pDescriptor->pMainHeap->GetGpuDescriptorHandle(m_downScaleRenderTarget.mainSrvIndex);
		uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor();
		CopyDescriptors(1, outResource.mainSrvHandleCPU, copyIndex);
		outResource.copySrvHandleCPU = m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex);
		outResource.copySrvHandleGPU = m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyIndex);

		return outResource;
#else
		return PostProcessBase::GetResultRenderTarget();
#endif
	}
}
