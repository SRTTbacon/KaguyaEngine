#include "MaterialBase.h"

#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\FrameBuffer.h"
#include "..\\..\\..\\Core\\EngineDefine.h"
#include "..\\..\\..\\Core\\Main\\Utility.h"
#include "..\\..\\..\\Core\\Pipeline\\RootSignature.h"
#include "..\\..\\..\\Core\\Pipeline\\PipelineState.h"
#include "..\\..\\..\\Core\\Pipeline\\PipelineDefine.h"
#include "..\\..\\..\\Core\\DescriptorHeap\\DescriptorHeap.h"
#include "..\\..\\..\\Core\\Texture2D\\Texture2D.h"
#include "..\\..\\Model\\Core\\Mesh.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgiformat.h>
#include <string.h>
#include <Windows.h>
#include <vector>
#include <algorithm>
#include <utility>
#include <functional>
#include <string>
#include <cstdint>

using namespace KaguyaEngine;
using namespace KaguyaEngine::Core;
using namespace KaguyaEngine::Model;
using namespace KaguyaEngine::Texture;

namespace KaguyaEngine::Material
{
	MaterialBase::MaterialBase(FrameResource* pFrameResource, uint8 textureCount)
		: FrameResource(pFrameResource)
		, m_uniqueBufferSize(0)
		, m_pipelineHashID(0)
		, m_textureRootIndex(-1)
		, m_textureCount(textureCount)
		, m_bTransparent(true)
		, m_pUniquePSData(nullptr)
		, m_cullMode(D3D12_CULL_MODE_FRONT)
	{
		for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
			m_pUniquePSBufferMapped[i] = nullptr;
		}
		m_textures.resize(textureCount);
	}

	MaterialBase::~MaterialBase()
	{
		for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
		{
			if (m_pUniquePSBuffer[i])
			{
				m_pUniquePSBuffer[i]->Unmap(0, nullptr);
				m_pUniquePSBufferMapped[i] = nullptr;
			}
		}

		if (m_dummyTexture.pTexture)
		{
			m_dummyTexture.pTexture->SetOnLoaded(nullptr);
		}
	}

	void MaterialBase::SetOnPipelineChanged(std::function<void(MaterialBase* pThis, uint64 beforeHashID, uint64 afterHashID)> callback)
	{
		m_onPipelineChanged = std::move(callback);
	}

	void MaterialBase::Initialize(D3D12_CULL_MODE cullMode, bool bTransparentMode)
	{
		m_cullMode = cullMode;
		m_bTransparent = bTransparentMode;

		// 真っ白のテクスチャをセット
		m_dummyTexture.pTexture = CreateObjectPtr<Texture2D>();
		// ダミーテクスチャのロードが完了したら
		m_dummyTexture.pTexture->SetOnLoaded([this]()
			{
				m_dummyTexture.mainDescriptorIndex = GetDescriptor()->pMainHeap->SetResource(m_dummyTexture.pTexture->GetResource());
			});
		m_dummyTexture.pTexture->LoadTexture(1.0f, 1.0f, 1.0f);

		CreateUniquePSBuffer();
		CreatePipeline();
	}

	void MaterialBase::LateUpdate()
	{
		// 固有バッファにマテリアルデータをコピー
		if (m_pUniquePSBufferMapped[*m_pCurrentBackBufferIndex]) {
			memcpy(m_pUniquePSBufferMapped[*m_pCurrentBackBufferIndex], m_pUniquePSData, m_uniqueBufferSize);
		}

		if (m_dummyTexture.mainDescriptorIndex != UINT32_MAX)
		{
			// テクスチャスロットをセット
			for (int i = 0; i < m_textureCount; i++)
			{
				uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor();
				// テクスチャが設定されていればそれをコピーし、設定されていなければダミーテクスチャをコピー
				if (m_textures[i].mainDescriptorIndex != UINT32_MAX && m_textures[i].pTexture && m_textures[i].pTexture->IsValid())
				{
					m_pDevice->CopyDescriptorsSimple(1, m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex), m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(m_textures[i].mainDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					m_textures[i].copyDescriptorIndex = copyIndex;
				}
				else
				{
					m_pDevice->CopyDescriptorsSimple(1, m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex), m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(m_dummyTexture.mainDescriptorIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					if (i == 0) {
						m_dummyTexture.copyDescriptorIndex = copyIndex;
					}
				}
			}
		}
	}

	void MaterialBase::Draw()
	{
		if (m_dummyTexture.copyDescriptorIndex == UINT32_MAX) {
			return;
		}

		// UniquePSBufferはメッシュに左右されないため先に実行
		GetCommandList()->SetGraphicsRootConstantBufferView(3, m_pUniquePSBuffer[*m_pCurrentBackBufferIndex]->GetGPUVirtualAddress());
		if (m_textureRootIndex >= 0)
		{
			if (m_textures[0].copyDescriptorIndex != UINT32_MAX) {
				GetCommandList()->SetGraphicsRootDescriptorTable(m_textureRootIndex, m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(m_textures[0].copyDescriptorIndex));
			}
			else {
				GetCommandList()->SetGraphicsRootDescriptorTable(m_textureRootIndex, m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(m_dummyTexture.copyDescriptorIndex));
			}
		}

		for (Mesh* pMesh : m_pMeshes)
		{
			DrawMesh(pMesh);
		}
	}

	void MaterialBase::DrawMesh(Mesh* pMesh)
	{
		GetCommandList()->SetGraphicsRootConstantBufferView(2, pMesh->GetMeshConstantBuffer()->GetGPUVirtualAddress());
		pMesh->Draw();
	}

	void MaterialBase::AddMesh(Mesh* pMesh)
	{
		if (!pMesh) {
			return;
		}

		auto it = std::find(m_pMeshes.begin(), m_pMeshes.end(), pMesh);

		if (it == m_pMeshes.end()) {
			m_pMeshes.push_back(pMesh);
		}
	}
	void MaterialBase::RemoveMesh(Mesh* pMesh)
	{
		std::erase(m_pMeshes, pMesh);
	}

	void MaterialBase::SetTexture(uint32 index, const std::string& textureFilePath)
	{
		if (index >= m_textureCount) {
			return;
		}

		bool bDDSFile = GetFileExtension(textureFilePath) == ".dds";

		ResetTexture(index);
		m_textures[index].pTexture->SetOnLoaded([this, index]()
			{
				SetTexture(index, m_textures[index].pTexture);
			});
		m_textures[index].pTexture->LoadTexture(textureFilePath, bDDSFile);
	}

	void MaterialBase::SetTexture(uint32 index, const std::string& textureFilePath, const D3D12_SHADER_RESOURCE_VIEW_DESC& shaderResource)
	{
		if (index >= m_textureCount) {
			return;
		}

		bool bDDSFile = GetFileExtension(textureFilePath) == ".dds";

		ResetTexture(index);
		m_textures[index].pTexture->SetOnLoaded([this, index, shaderResource]()
			{
				SetTexture(index, m_textures[index].pTexture, shaderResource);
			});
		m_textures[index].pTexture->LoadTexture(textureFilePath, bDDSFile);
	}

	void MaterialBase::SetTexture(uint32 index, const char* data, size_t size, bool bDDSFile)
	{
		if (index >= m_textureCount) {
			return;
		}

		ResetTexture(index);
		m_textures[index].pTexture->SetOnLoaded([this, index]()
			{
				SetTexture(index, m_textures[index].pTexture);
			});
		m_textures[index].pTexture->LoadTexture(data, size, bDDSFile);
	}

	void MaterialBase::SetTexture(uint32 index, const char* data, size_t size, bool bDDSFile, const D3D12_SHADER_RESOURCE_VIEW_DESC& shaderResource)
	{
		if (index >= m_textureCount) {
			return;
		}

		ResetTexture(index);
		m_textures[index].pTexture->SetOnLoaded([this, index, shaderResource]()
			{
				SetTexture(index, m_textures[index].pTexture, shaderResource);
			});
		m_textures[index].pTexture->LoadTexture(data, size, bDDSFile);
	}

	void MaterialBase::SetTexture(uint32 index, ObjectPtr<KaguyaEngine::Texture::Texture2D> pTexture)
	{
		if (!pTexture || index >= m_textureCount) {
			return;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Format = pTexture->GetResource()->GetDesc().Format;
		desc.Texture2D.MipLevels = 1;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.ResourceMinLODClamp = 0.0f;
		SetTexture(index, pTexture, desc);
	}

	void MaterialBase::SetTexture(uint32 index, ObjectPtr<Texture2D> pTexture, const D3D12_SHADER_RESOURCE_VIEW_DESC& shaderResource)
	{
		if (!pTexture || index >= m_textureCount) {
			return;
		}

		if (m_textures[index].mainDescriptorIndex != UINT32_MAX)
		{
			m_pDescriptor->pMainHeap->ClearDescriptor(m_textures[index].mainDescriptorIndex);
			m_textures[index].mainDescriptorIndex = UINT32_MAX;
		}

		m_textures[index].pTexture = pTexture;
		m_textures[index].mainDescriptorIndex = m_pDescriptor->pMainHeap->SetResource(pTexture->GetResource(), shaderResource);
	}

	void MaterialBase::RemoveTexture(uint32 index)
	{
		if (m_textures[index].mainDescriptorIndex != UINT32_MAX)
		{
			m_pDescriptor->pMainHeap->ClearDescriptor(m_textures[index].mainDescriptorIndex);
			m_textures[index].mainDescriptorIndex = UINT32_MAX;
		}
	}

	void MaterialBase::SetEnableTransparent(bool bEnable)
	{
		if (m_bTransparent != bEnable)
		{
			m_bTransparent = bEnable;
			CreatePipeline();
		}
	}

	void MaterialBase::SetMaterialName(std::string materialName)
	{
		m_materialName = materialName;
	}

	void MaterialBase::CreatePipeline(std::vector<D3D12_ROOT_PARAMETER>& rootParameters, std::vector<D3D12_INPUT_ELEMENT_DESC>& inputElements, std::vector<D3D12_STATIC_SAMPLER_DESC>& samplers, PsoDescKey& desc)
	{
		uint64 hash = m_pipelineHashID;

		uint64 rootSignatureHash = GetShaderHash(m_vertexShaderPath, m_pixelShaderPath);
		ID3D12RootSignature* pRootSignature = RootSignature::CreateRootSignature(m_pDevice, rootSignatureHash, rootParameters.data(), static_cast<uint32>(rootParameters.size()), samplers.data(), static_cast<uint32>(samplers.size()));
		PipelineState::CreatePipelineState(m_pDevice, inputElements.data(), static_cast<uint32>(inputElements.size()), rootSignatureHash, desc, m_pipelineHashID);

		if (m_onPipelineChanged) {
			m_onPipelineChanged(this, hash, m_pipelineHashID);
		}
	}

	void MaterialBase::GetDefaultInputElements(std::vector<D3D12_INPUT_ELEMENT_DESC>& inputElements)
	{
		inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		inputElements.push_back(D3D12_INPUT_ELEMENT_DESC{ "VERTEXID", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
	}

	void MaterialBase::GetDefaultRootParameters(std::vector<D3D12_ROOT_PARAMETER>& rootParameters)
	{
		CD3DX12_ROOT_PARAMETER param{};
		// 以下2つは共通バッファのため必ず必要かつ、ルートパラメーターの [0, 1] 番目でなければいけない
		param.InitAsConstantBufferView(0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters.push_back(param);
		param.InitAsConstantBufferView(0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters.push_back(param);

		// 各マテリアル固有のパラメーター
		param.InitAsConstantBufferView(1, 1, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters.push_back(param);
		param.InitAsConstantBufferView(1, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters.push_back(param);
	}

	void MaterialBase::GetDefaultSamplers(std::vector<D3D12_STATIC_SAMPLER_DESC>& samplers)
	{
		CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		samplers.push_back(sampler);
	}

	void MaterialBase::GetDefailtPsoDesc(PsoDescKey& psoDesc)
	{
		// PsoDescKeyの設定
		psoDesc.cullMode = m_cullMode;
		psoDesc.textureAddressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		psoDesc.vertexShaderPath = m_vertexShaderPath;
		psoDesc.pixelShaderPath = m_pixelShaderPath;
		psoDesc.depthStencilPreset = DepthStencilPreset_None;
		psoDesc.bAlphaBlend = m_bTransparent;
	}

	void MaterialBase::CreateUniquePSBuffer(UniquePSBuffer* pBuffer, uint32 bufferSize)
	{
		m_pUniquePSData = pBuffer;
		m_uniqueBufferSize = bufferSize;

		for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
			D3D12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

			CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
			HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pUniquePSBuffer[i]));
			if (FAILED(hr)) {
				FatalError("マテリアル固有バッファの作成に失敗しました。");
			}
			m_pUniquePSBuffer[i]->SetName(L"MaterialBase::CreateUniquePSBuffer");

			hr = m_pUniquePSBuffer[i]->Map(0, nullptr, &m_pUniquePSBufferMapped[i]);
			if (FAILED(hr)) {
				FatalError("バッファのマップに失敗しました。");
			}
		}
	}
	void MaterialBase::ResetTexture(uint32 index)
	{
		if (!m_textures[index].pTexture) {
			m_textures[index].pTexture = CreateObjectPtr<Texture2D>();
		}
		if (m_textures[index].mainDescriptorIndex != UINT32_MAX)
		{
			GetDescriptor()->pMainHeap->ClearDescriptor(m_textures[index].mainDescriptorIndex);
			m_textures[index].mainDescriptorIndex = UINT32_MAX;
		}
	}
}
