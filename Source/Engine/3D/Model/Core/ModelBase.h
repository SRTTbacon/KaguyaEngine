//*****************************************************************************
//
// モデル関係の基底クラス
// このクラス単体では動作を想定していません。必ず派生クラスを利用してください。
//
// Model.h
//
// T_Takeuchi
//
// 2026/6/2
//
//*****************************************************************************

#pragma once

#include "Mesh.h"
#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\DirectXCore\\ResourceStatics.h"
#include "..\\..\\Transform\\Transform.h"

#include <DirectXMath.h>
#include <vector>
#include <d3d12.h>
#include <d3dx12.h>
#include <atomic>
#include <string.h>
#include <string>
#include <memory>
#include <functional>
#include <concepts>
#include <assimp\scene.h>

namespace KaguyaEngine::Model
{
	class ModelBase : public Transform, public std::enable_shared_from_this<ModelBase>
	{
	public:
        ModelBase();
		virtual ~ModelBase();

        // シーンのUpdateより前に実行される
        virtual void Update() {};
        // シーンのUpdateより後に実行される
		virtual void LateUpdate();
		// シーンのDraw()より前に実行される
		virtual void BeforeDrawUpdate() {};
		// シーンのDraw()より後に実行される
		virtual void AfterDrawUpdate() {};

        void SetOnLoaded(std::function<void()> callback);

	public:
		inline std::string GetModelPath() const { return m_modelPath; }
		inline std::string GetModelName() const { return m_modelName; }
		inline DirectX::XMMATRIX GetModelMatrix() const { return m_modelMatrix; }
        inline Mesh* GetMesh(uint32 index) const { return m_pMeshes[index].get(); }
        inline uint32 GetMeshCount() const { return static_cast<uint32>(m_pMeshes.size()); }
		inline bool IsLoaded() const { return m_bLoaded.load(); }	// モデルの読み込みが完了しているかどうか
        inline bool CanDraw() const { return m_bVisible && m_bDraw && m_bLoaded.load(); }
        inline bool CanDrawShadow() const { return m_bVisible && m_bDrawShadow && m_bLoaded.load(); }

	public:
		bool m_bDraw;			// 描画するかどうか
		bool m_bDrawShadow;		// 影を描画するかどうか (すべてのライトを対象)
		bool m_bStaticModel;	// 静的モデルかどうか
		bool m_bEnableDirectionalShadow;	// 太陽の影を描画するかどうか (他のライトは無視)

	protected:
		// シェーダーに渡す頂点情報 (プリミティブ用。拡張する場合は別構造体として定義)
        // 基本は継承する形が望ましい
		struct VertexPrimitive {
			DirectX::XMFLOAT3 position;     // 頂点座標
			DirectX::XMFLOAT3 normal;       // 法線
			DirectX::XMFLOAT2 texCoords;    // UV
			DirectX::XMFLOAT3 tangent;      // 正接
			DirectX::XMFLOAT3 bitangent;    // 従法線
            uint32 vertexID;                //頂点ID (Index)
        };

		DirectX::XMMATRIX m_modelMatrix;		// LateUpdate()で更新されるモデル全体のマトリックス
		DirectX::XMMATRIX m_modelMatrixPrev;	// 1フレーム前のモデル全体のマトリックス

        DirectX::XMFLOAT3 m_rootPosition;	    // アーマチュアの位置なども考慮したモデルの最終位置 (自動でm_positionが入る)

        std::string m_modelPath;
		std::string m_modelName;

		std::vector<ObjectPtr<Mesh>> m_pMeshes;

		std::atomic<bool> m_bLoaded;	// モデルの読み込みが完了しているかどうか
        uint8 m_needMeshCount;
        std::atomic<uint8> m_nowCPULoadedMeshCount;
        std::atomic<uint8> m_nowGPULoadedMeshCount;

        // m_pMeshDataを作成
        // すでに作成済みで再利用する場合にtrueを返す
        template <typename T, typename VertexType>
        requires std::derived_from<T, MeshData>
		bool CreateBuffer(Mesh* pMesh, std::vector<VertexType>& vertices, std::vector<uint32>& indices, uint32 vertexStructSize, uint32 meshIndex);

        // クアッドモデルを作成
        void CreatePrimitiveQuad();

        // ロード予定のメッシュ数を取得
        uint8 GetNeedMeshCount(const aiNode* pNode);

        DirectX::XMMATRIX GetMeshDefaultMatrix(const aiNode* pNode) const;

        void CheckLoaded();

        virtual void ReleaseModel();

    private:
        std::function<void()> m_onLoaded;

        void CreateBuffer(Mesh* pMesh, uint32 vertexBufferSize, uint32 indexBufferSize, uint32 vertexCount, ID3D12Resource* pTempVertexBuffer, ID3D12Resource* pTempIndexBuffer);
	};


    // 必要なバッファを作成 (キャラクターと普通のモデル両方をに対応)
    template<typename T, typename VertexType>
    requires std::derived_from<T, MeshData>
    inline bool ModelBase::CreateBuffer(Mesh* pMesh, std::vector<VertexType>& vertices, std::vector<uint32>& indices, uint32 vertexStructSize, uint32 meshIndex)
    {
        pMesh->m_pParentModel = this;

        LockMutex lock(g_sharedMeshesMutex);
        if (g_sharedMeshes.find(m_modelPath) != g_sharedMeshes.end())
        {
            if (meshIndex == 0) {
                g_sharedMeshes[m_modelPath].refCount++;
            }
            if (g_sharedMeshes[m_modelPath].meshDataList.size() > meshIndex)
            {
                size_t size = g_sharedMeshes[m_modelPath].meshDataList.size();
                pMesh->m_pMeshData = g_sharedMeshes[m_modelPath].meshDataList[meshIndex];
                pMesh->m_meshName = g_sharedMeshes[m_modelPath].meshMain[meshIndex].meshName;
                pMesh->m_position = g_sharedMeshes[m_modelPath].meshMain[meshIndex].position;
                pMesh->m_rotation = g_sharedMeshes[m_modelPath].meshMain[meshIndex].rotation;
                return true;
            }
        }
        else
        {
            g_sharedMeshes[m_modelPath] = MeshDataList();
            g_sharedMeshes[m_modelPath].meshDataList = std::vector<ObjectPtr<MeshData>>();
            g_sharedMeshes[m_modelPath].meshMain = std::vector<MeshDataList::MeshDataMain>();
            if (meshIndex == 0) {
                g_sharedMeshes[m_modelPath].refCount = 1;
            }
        }

        // MeshDataを作成
        ObjectPtr<MeshData> p1 = CreateObjectPtr<T>();
        g_sharedMeshes[m_modelPath].meshDataList.push_back(p1);
        pMesh->m_pMeshData = p1;
        MeshDataList::MeshDataMain dataMain{};
        dataMain.meshName = pMesh->m_meshName;
        dataMain.position = pMesh->m_position;
        dataMain.rotation = pMesh->m_rotation;
        g_sharedMeshes[m_modelPath].meshMain.push_back(dataMain);
        lock.unlock();

        // 頂点バッファのリソース
        const uint32 bufferSize = static_cast<uint32>(vertexStructSize * vertices.size());
        CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        ResourceStatics::CreateResource(D3D12_HEAP_TYPE_DEFAULT, bufferSize, pMesh->m_pMeshData->vertexBuffer);
        pMesh->m_pMeshData->vertexBuffer->SetName(L"meshData->vertexBuffer");

        ID3D12Resource* pUploadBuffer = ResourceStatics::CreateUploadResource(bufferSize);
        pUploadBuffer->SetName(L"TempMeshDataVertexBuffer");

        // インデックスバッファの作成
        const uint32 indexBufferSize = static_cast<uint32>(sizeof(uint32) * indices.size());
        ResourceStatics::CreateResource(D3D12_HEAP_TYPE_DEFAULT, indexBufferSize, pMesh->m_pMeshData->indexBuffer);
        pMesh->m_pMeshData->indexBuffer->SetName(L"meshData->indexBuffer");

        ID3D12Resource* pUploadBuffer2 = ResourceStatics::CreateUploadResource(indexBufferSize);
        pUploadBuffer2->SetName(L"TempMeshDataIndexBuffer");

        // 頂点データをGPUに送信
        void* vertexDataBegin;
        pUploadBuffer->Map(0, nullptr, &vertexDataBegin);
        memcpy(vertexDataBegin, vertices.data(), bufferSize);
        pUploadBuffer->Unmap(0, nullptr);

        // インデックスデータをGPUに送信
        void* indexDataBegin;
        pUploadBuffer2->Map(0, nullptr, &indexDataBegin);
        memcpy(indexDataBegin, indices.data(), indexBufferSize);
        pUploadBuffer2->Unmap(0, nullptr);

        CreateBuffer(pMesh, vertexStructSize, indexBufferSize, static_cast<uint32>(vertices.size()), pUploadBuffer, pUploadBuffer2);

        return false;
    }
}
