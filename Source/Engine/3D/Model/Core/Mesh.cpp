#include "Mesh.h"
#include "ModelBase.h"
#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\EngineDefine.h"
#include "..\\..\\Material\\Core\\MaterialBase.h"

#include <d3d12.h>
#include <DirectXMathMatrix.inl>
#include <string>
#include <unordered_map>
#include <string.h>
#include <mutex>
#include <DirectXMath.h>

using namespace KaguyaEngine::Material;
using namespace DirectX;

namespace KaguyaEngine::Model
{
	std::unordered_map<std::string, MeshDataList> g_sharedMeshes;
	std::mutex g_sharedMeshesMutex;

	Mesh::Mesh(ID3D12Device* pDevice, uint32 meshIndex)
		: m_meshIndex(meshIndex)
		, m_meshMatrix(DirectX::XMMatrixIdentity())
		, m_bufferSize(0)
		, m_bDraw(true)
		, m_bDrawShadow(true)
		, m_bStaticMesh(true)
		, m_pMaterial(nullptr)
		, m_pParentModel(nullptr)
	{
		for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
			m_pMeshConstantBufferMapped[i] = nullptr;
		}

		CreateConstantBuffer();
	}

	Mesh::~Mesh()
	{
		for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
		{
			if (m_pMeshConstantBufferMapped[i])
			{
				m_pMeshConstantBufferResource[i]->Unmap(0, nullptr);
				m_pMeshConstantBufferMapped[i] = nullptr;
			}
		}

		if (m_pMaterial) {
			m_pMaterial->RemoveMesh(this);
		}
	}

	void Mesh::SetMaterial(MaterialBase* pMaterial)
	{
		if (m_pMaterial)
		{
			m_pMaterial->RemoveMesh(this);
			m_pMaterial = nullptr;
		}

		if (pMaterial)
		{
			pMaterial->AddMesh(this);
			m_pMaterial = pMaterial;
		}
	}

	void Mesh::LateUpdate()
	{
		//描画フラグが立っていない場合は更新しない
		if (!CanDraw() && !CanDrawShadow()) {
			return;
		}

		XMMATRIX scale = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
		XMMATRIX rotX = XMMatrixRotationX(m_rotation.x);
		XMMATRIX rotY = XMMatrixRotationY(m_rotation.y);
		XMMATRIX rotZ = XMMatrixRotationZ(m_rotation.z);
		XMMATRIX rot = rotX * rotY * rotZ;
		XMMATRIX pos = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
		m_meshMatrix = scale * rot * pos;

		//定数バッファにデータを書き込む
		m_pMeshBufferData->meshMatrixPrev = m_pMeshBufferData->meshMatrix;
		m_pMeshBufferData->meshMatrix = m_meshMatrix;
		XMMATRIX worldMatrix = m_meshMatrix * m_pParentModel->GetModelMatrix();
		m_pMeshBufferData->normalMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, worldMatrix));

		CopyMeshBuffer();
	}

	void Mesh::Draw(uint32 instanceCount, D3D12_PRIMITIVE_TOPOLOGY topology)
	{
		if (!CanDraw()) {
			return;
		}

		GetCommandList()->IASetVertexBuffers(0, 1, &m_pMeshData->vertexBufferView);         //頂点情報を送信
		GetCommandList()->IASetIndexBuffer(&m_pMeshData->indexBufferView);                  //インデックス情報を送信
		GetCommandList()->IASetPrimitiveTopology(topology);									//3角ポリゴンのみ
		GetCommandList()->DrawIndexedInstanced(static_cast<uint32>(m_pMeshData->indices.size()), instanceCount, 0, 0, 0);  //描画
	}

	bool Mesh::CanDraw() const
	{
		return m_pParentModel->CanDraw() && m_bVisible && m_bDraw;
	}

	bool Mesh::CanDrawShadow() const
	{
		return m_pParentModel->CanDrawShadow() && m_bVisible && m_bDrawShadow;
	}

	void Mesh::CopyMeshBuffer()
	{
		memcpy(m_pMeshConstantBufferMapped[*m_pCurrentBackBufferIndex], m_pMeshBufferData.get(), m_bufferSize);
	}

	// ------------------------------PrimitiveMesh------------------------------

	PrimitiveMesh::PrimitiveMesh(ID3D12Device* pDevice, uint32 meshIndex)
		: Mesh(pDevice, meshIndex)
	{}

	PrimitiveMesh::~PrimitiveMesh()
	{}

	void PrimitiveMesh::CreateConstantBuffer()
	{
		Mesh::CreateConstantBuffer<MeshConstantBuffer>(sizeof(MeshConstantBuffer));
	}

	void PrimitiveMesh::LateUpdate()
	{
		//描画フラグが立っていない場合は更新しない
		if (!CanDraw() && !CanDrawShadow()) {
			return;
		}

		Mesh::LateUpdate();
	}
}
