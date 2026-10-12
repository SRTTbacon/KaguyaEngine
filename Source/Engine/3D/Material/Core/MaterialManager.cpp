#include "MaterialManager.h"
#include "MaterialBase.h"
#include "..\\..\\Lights\\DirectionalLight.h"
#include "..\\..\\Camera\\Camera.h"
#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\FrameBuffer.h"
#include "..\\..\\..\\Core\\EngineDefine.h"
#include "..\\..\\..\\Core\\Pipeline\\PipelineDefine.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <Windows.h>
#include <vector>
#include <algorithm>
#include <utility>
#include <array>
#include <string.h>

using namespace KaguyaEngine::Core;
using namespace DirectX;

namespace KaguyaEngine::Material
{
	MaterialManager::MaterialManager(FrameResource* pFrameResource)
		: FrameResource(pFrameResource)
		, m_commonVSBuffer(CommonVSBuffer())
		, m_commonPSBuffer(CommonPSBuffer())
	{
		// 全マテリアル共通バッファを作成
		D3D12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

		m_pCommonVSBuffers.resize(1);
		m_pCommonVSBufferMapped.resize(1);
		m_pCommonPSBuffers.resize(1);
		m_pCommonPSBufferMapped.resize(1);

		// 頂点シェーダー用
		CD3DX12_RESOURCE_DESC constantVSData = CD3DX12_RESOURCE_DESC::Buffer((sizeof(CommonVSBuffer) + 255) & ~255);
		for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
			CreateConstantBuffer(constantVSData, m_pCommonVSBuffers[0][i], &m_pCommonVSBufferMapped[0][i]);
		}

		// ピクセルシェーダー用
		CD3DX12_RESOURCE_DESC constantPSData = CD3DX12_RESOURCE_DESC::Buffer((sizeof(CommonPSBuffer) + 255) & ~255);
		for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
			CreateConstantBuffer(constantPSData, m_pCommonPSBuffers[0][i], &m_pCommonPSBufferMapped[0][i]);
		}
	}

	MaterialManager::~MaterialManager()
	{
		for (size_t i = 0; i < m_pCommonVSBuffers.size(); i++)
		{
			for (int j = 0; j < FRAME_BUFFER_COUNT; j++)
			{
				if (m_pCommonVSBuffers[i][j])
				{
					m_pCommonVSBuffers[i][j]->Unmap(0, nullptr);
					m_pCommonVSBufferMapped[i][j] = nullptr;
				}
				if (m_pCommonPSBuffers[i][j])
				{
					m_pCommonPSBuffers[i][j]->Unmap(0, nullptr);
					m_pCommonPSBufferMapped[i][j] = nullptr;
				}
			}
		}

		// すべてのパイプラインを解放
		g_pipelineStateCache.clear();
		g_rootSignatureCache.clear();
	}

	void MaterialManager::UpdateCameraCount(uint32 cameraCount)
	{
		size_t nowCameraCount = m_pCommonVSBuffers.size();

		m_pCommonVSBuffers.resize(cameraCount);
		m_pCommonVSBufferMapped.resize(cameraCount);
		m_pCommonPSBuffers.resize(cameraCount);
		m_pCommonPSBufferMapped.resize(cameraCount);

		CD3DX12_RESOURCE_DESC constantVSData = CD3DX12_RESOURCE_DESC::Buffer(GetAlignedSize(sizeof(CommonVSBuffer)));
		CD3DX12_RESOURCE_DESC constantPSData = CD3DX12_RESOURCE_DESC::Buffer(GetAlignedSize(sizeof(CommonPSBuffer)));

		// 今のバッファ数よりも大きい場合は増やす
		if (nowCameraCount < cameraCount)
		{
			for (size_t i = nowCameraCount; i < cameraCount; i++)
			{
				for (int j = 0; j < FRAME_BUFFER_COUNT; j++)
				{
					CreateConstantBuffer(constantVSData, m_pCommonVSBuffers[i][j], &m_pCommonVSBufferMapped[i][j]);
					CreateConstantBuffer(constantPSData, m_pCommonPSBuffers[i][j], &m_pCommonPSBufferMapped[i][j]);
				}
			}
		}
	}

	void MaterialManager::LateUpdate(Camera* pCamera, KaguyaEngine::Light::DirectionalLight* pLight, uint32 cameraIndex)
	{
		if (cameraIndex >= m_pCommonVSBuffers.size()) {
			return;
		}

		m_commonVSBuffer.viewMatrixPrev = pCamera->GetViewMatrixPrev();
		m_commonVSBuffer.viewMatrix = pCamera->GetViewMatrix();
		m_commonVSBuffer.projectionMatrixPrev = pCamera->GetProjMatrixPrev();
		m_commonVSBuffer.projectionMatrix = pCamera->GetProjMatrix();
		m_commonVSBuffer.cameraPos = XMFLOAT4(pCamera->GetEyePosFloat().x, pCamera->GetEyePosFloat().y, pCamera->GetEyePosFloat().z, 0.0f);
		m_commonVSBuffer.cameraJitters = XMFLOAT4(pCamera->GetJitter().x, pCamera->GetJitter().y, pCamera->GetJitterPrev().x, pCamera->GetJitterPrev().y);

		m_commonPSBuffer.renderSize = pCamera->GetRenderSize();

		memcpy(m_pCommonVSBufferMapped[cameraIndex][*m_pCurrentBackBufferIndex], &m_commonVSBuffer, sizeof(CommonVSBuffer));
		memcpy(m_pCommonPSBufferMapped[cameraIndex][*m_pCurrentBackBufferIndex], &m_commonPSBuffer, sizeof(CommonPSBuffer));

		for (const std::pair<uint64, std::vector<MaterialBase*>> opaquePair : m_sharedOpaquePipelines)
		{
			for (MaterialBase* pMaterial : opaquePair.second)
			{
				pMaterial->LateUpdate();
			}
		}
	}

	void MaterialManager::Draw(uint32 cameraIndex)
	{
		if (cameraIndex >= m_pCommonVSBuffers.size()) {
			return;
		}

		for (const std::pair<uint64, std::vector<MaterialBase*>> opaquePair : m_sharedOpaquePipelines)
		{
			// 恐らくありえないが、パイプラインが存在しなければ無視
			if (g_pipelineStateCache.find(opaquePair.first) == g_pipelineStateCache.end()) {
				continue;
			}

			GetCommandList()->SetGraphicsRootSignature(g_pipelineStateCache[opaquePair.first].pRootSignature);
			GetCommandList()->SetPipelineState(g_pipelineStateCache[opaquePair.first].pPipelineState.Get());

			// 共通バッファを指定 (0, 1番目を使用)
			GetCommandList()->SetGraphicsRootConstantBufferView(0, m_pCommonVSBuffers[cameraIndex][*m_pCurrentBackBufferIndex]->GetGPUVirtualAddress());
			GetCommandList()->SetGraphicsRootConstantBufferView(1, m_pCommonPSBuffers[cameraIndex][*m_pCurrentBackBufferIndex]->GetGPUVirtualAddress());

			// このパイプラインに登録されているマテリアルをすべて描画
			for (MaterialBase* pMaterial : opaquePair.second) {
				if (pMaterial->IsCanDraw()) {
					pMaterial->Draw();
				}
			}
		}
	}

	void MaterialManager::OnPipelineChanged(MaterialBase* pMaterial, uint64 beforeHashID, uint64 afterHashID)
	{
		if (!pMaterial) {
			return;
		}

		// もともと配置されていたパイプラインから削除
		if (beforeHashID != 0){
			if (m_sharedOpaquePipelines.find(beforeHashID) != m_sharedOpaquePipelines.end()) {
				std::erase(m_sharedOpaquePipelines[beforeHashID], pMaterial);
			}
			else if (m_sharedTransparentPipelines.find(beforeHashID) != m_sharedTransparentPipelines.end()) {
				std::erase(m_sharedTransparentPipelines[beforeHashID], pMaterial);
			}
		}

		// 新しいパイプラインに配置
		if (pMaterial->GetEnableTransparent()) {
			std::vector<MaterialBase*>& sharedPipeline = m_sharedTransparentPipelines[afterHashID];
			if (std::find(sharedPipeline.begin(), sharedPipeline.end(), pMaterial) == sharedPipeline.end()) {
				sharedPipeline.push_back(pMaterial);
			}
		}
		else {
			std::vector<MaterialBase*>& sharedPipeline = m_sharedOpaquePipelines[afterHashID];
			if (std::find(sharedPipeline.begin(), sharedPipeline.end(), pMaterial) == sharedPipeline.end()) {
				sharedPipeline.push_back(pMaterial);
			}
		}
	}

	void MaterialManager::CreateConstantBuffer(D3D12_RESOURCE_DESC& desc, ComPtr<ID3D12Resource>& pOutResource, void** pOutMapped)
	{
		D3D12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(pOutResource.ReleaseAndGetAddressOf()));

		if (FAILED(hr)) {
			FatalError("コンスタントバッファの生成に失敗");
		}

		pOutResource->Map(0, nullptr, pOutMapped);
		pOutResource->SetName(L"meshContantBuffer");
	}
}