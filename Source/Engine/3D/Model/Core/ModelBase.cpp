#include "ModelBase.h"
#include "Mesh.h"
#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\Thread\\ResourceCopyThread.h"

#include <vector>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgiformat.h>
#include <DirectXMath.h>
#include <DirectXMathMatrix.inl>
#include <utility>
#include <functional>
#include <DirectXMathConvert.inl>
#include <assimp\scene.h>

using namespace KaguyaEngine::AsyncTask;
using namespace DirectX;

namespace KaguyaEngine::Model
{
    ModelBase::ModelBase()
		: m_bDraw(true)
		, m_bDrawShadow(true)
		, m_bStaticModel(true)
		, m_bEnableDirectionalShadow(true)
		, m_bLoaded(false)
        , m_needMeshCount(0)
        , m_nowCPULoadedMeshCount(0)
        , m_nowGPULoadedMeshCount(0)
		, m_modelMatrix(XMMatrixIdentity())
		, m_modelMatrixPrev(XMMatrixIdentity())
        , m_rootPosition(0.0f, 0.0f, 0.0f)
    {
    }

    ModelBase::~ModelBase()
    {
        ReleaseModel();
    }

    void ModelBase::LateUpdate()
    {
        if (!m_bLoaded.load()) {
            return;
        }

		m_modelMatrixPrev = m_modelMatrix;

        XMMATRIX scale = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
        XMMATRIX rotX = XMMatrixRotationX(m_rotation.x);
        XMMATRIX rotY = XMMatrixRotationY(m_rotation.y);
        XMMATRIX rotZ = XMMatrixRotationZ(m_rotation.z);
        XMMATRIX rot = rotX * rotY * rotZ;
        XMMATRIX pos = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
        m_modelMatrix = scale * rot * pos;

        m_rootPosition = m_position;

        // メッシュにモデル全体の情報を適応
        for (ObjectPtr<Mesh> pMesh : m_pMeshes)
        {
            Mesh* pPrimitiveMesh = pMesh.get();
            pPrimitiveMesh->m_pMeshBufferData->modelMatrix = m_modelMatrix;
            pPrimitiveMesh->m_pMeshBufferData->modelMatrixPrev = m_modelMatrixPrev;

            pPrimitiveMesh->LateUpdate();
        }
    }

    void ModelBase::SetOnLoaded(std::function<void()> callback)
    {
        m_onLoaded = std::move(callback);
    }

    void ModelBase::CreatePrimitiveQuad()
    {
        //クアッドモデル
        std::vector<XMFLOAT3> vertices =
        {
            { -1.0f,  1.0f, 0.0f}, // 左上
            {  1.0f,  1.0f, 0.0f}, // 右上
            { -1.0f, -1.0f, 0.0f}, // 左下
            {  1.0f, -1.0f, 0.0f}, // 右下
        };
        XMFLOAT2 uvs[] =
        {
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 0.0f, 1.0f },
            { 1.0f, 1.0f }
        };

        std::vector<VertexPrimitive> vertexList;
        std::vector<uint32> indices
        {
            0,1,2,  2,1,3,  2,1,0,  3,1,2
        };

        //頂点の処理
        for (size_t i = 0; i < vertices.size(); i++)
        {
            VertexPrimitive vertex{};

            vertex.position = vertices[i];
            vertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
            vertex.texCoords = uvs[i];
            vertex.tangent = XMFLOAT3(0.0f, 1.0f, 0.0f);
            vertex.bitangent = XMFLOAT3(0.0f, 1.0f, 0.0f);

            vertex.vertexID = static_cast<uint32>(i);

            vertexList.push_back(vertex);
        }

        ObjectPtr<PrimitiveMesh> pMesh = CreateObjectPtr<PrimitiveMesh>(m_pDevice, 0);
        pMesh->SetFrameResource(this);
        pMesh->CreateConstantBuffer();
        pMesh->m_meshName = "PrimitiveQuad";

        bool bExistMesh = CreateBuffer<MeshData>(pMesh.get(), vertexList, indices, sizeof(VertexPrimitive), 0);

        m_pMeshes.push_back(pMesh);

        if (!bExistMesh)
        {
            pMesh->m_pMeshData->vertices = vertices;
            pMesh->m_pMeshData->indices = indices;
        }
    }

    uint8 ModelBase::GetNeedMeshCount(const aiNode* pNode)
    {
        int count = 0;

        for (uint32 i = 0; i < pNode->mNumChildren; i++)
        {
            aiNode* childNode = pNode->mChildren[i];
            count += GetNeedMeshCount(childNode);
        }

        for (uint32 j = 0; j < pNode->mNumMeshes; j++) {
            count++;
        }

        return count;
    }

    DirectX::XMMATRIX ModelBase::GetMeshDefaultMatrix(const aiNode* pNode) const
    {
        XMMATRIX parentMatrix = XMMatrixIdentity();
        if (pNode->mParent != nullptr) {
            parentMatrix = GetMeshDefaultMatrix(pNode->mParent);
        }

        XMMATRIX currentMatrix = XMMatrixTranspose(XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&pNode->mTransformation)));
        return currentMatrix * parentMatrix;
    }

    void ModelBase::CheckLoaded()
    {
        if (m_nowCPULoadedMeshCount.load() >= m_needMeshCount && m_nowGPULoadedMeshCount.load() >= m_needMeshCount && !m_bLoaded.load())
        {
            m_bLoaded.store(true);

            if (m_onLoaded)
            {
                m_onLoaded();
                m_onLoaded = nullptr;
            }
            printf("End - %d\n", timeGetTime());
        }
    }

    void ModelBase::ReleaseModel()
    {
        LockMutex lock(g_sharedMeshesMutex);
        if (g_sharedMeshes.find(m_modelPath) != g_sharedMeshes.end())
        {
            g_sharedMeshes[m_modelPath].Release();
            if (g_sharedMeshes[m_modelPath].refCount <= 0) {
                g_sharedMeshes.erase(m_modelPath);
            }
        }

        m_modelPath = "";
        m_modelName = "";
        m_bLoaded = false;

        m_pMeshes.clear();

        m_needMeshCount = false;
        m_nowCPULoadedMeshCount = 0;
		m_nowGPULoadedMeshCount = 0;
    }

    void ModelBase::CreateBuffer(Mesh* pMesh, uint32 vertexBufferSize, uint32 indexBufferSize, uint32 vertexCount, ID3D12Resource* pTempVertexBuffer, ID3D12Resource* pTempIndexBuffer)
    {
		// 頂点バッファビューとインデックスバッファビューの設定
        const uint32 bufferSize = static_cast<uint32>(vertexBufferSize * vertexCount);
        pMesh->m_pMeshData->vertexBufferView.BufferLocation = pMesh->m_pMeshData->vertexBuffer->GetGPUVirtualAddress();
        pMesh->m_pMeshData->vertexBufferView.StrideInBytes = vertexBufferSize;
        pMesh->m_pMeshData->vertexBufferView.SizeInBytes = bufferSize;

        pMesh->m_pMeshData->indexBufferView.BufferLocation = pMesh->m_pMeshData->indexBuffer->GetGPUVirtualAddress();
        pMesh->m_pMeshData->indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        pMesh->m_pMeshData->indexBufferView.SizeInBytes = indexBufferSize;

		ObjectPtr<ModelBase> pThis = shared_from_this();
		// コピースレッドに頂点バッファとインデックスバッファをGPUにコピーする処理を登録
        CopyJob copyJob{};
        copyJob.func = [pMesh, bufferSize, indexBufferSize, pTempVertexBuffer, pTempIndexBuffer](ID3D12GraphicsCommandList* pCommandList)
            {
                std::vector<D3D12_RESOURCE_BARRIER> barriers;
                barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(pMesh->m_pMeshData->vertexBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
                barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(pMesh->m_pMeshData->indexBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
                pCommandList->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());

                pCommandList->CopyBufferRegion(pMesh->m_pMeshData->vertexBuffer.Get(), 0, pTempVertexBuffer, 0, bufferSize);
                pCommandList->CopyBufferRegion(pMesh->m_pMeshData->indexBuffer.Get(), 0, pTempIndexBuffer, 0, indexBufferSize);
            };
		copyJob.OnComplete = [pThis, pMesh, pTempVertexBuffer, pTempIndexBuffer]()
			{
                std::vector<D3D12_RESOURCE_BARRIER> barriers;
                barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(pMesh->m_pMeshData->vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
                barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(pMesh->m_pMeshData->indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
                pThis->GetCommandList()->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());

                pTempVertexBuffer->Release();
                pTempIndexBuffer->Release();

                pThis->m_nowGPULoadedMeshCount.store(pThis->m_nowGPULoadedMeshCount.load() + 1);

                pThis->CheckLoaded();
            };

        // GPU処理をキューに入れる
        LockMutex lock(g_copyJobManager->m_mutex);
        g_copyJobManager->m_jobs.push_back(copyJob);
		g_copyJobManager->m_cv.notify_one();
    }
}
