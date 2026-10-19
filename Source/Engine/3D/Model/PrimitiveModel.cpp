#include "PrimitiveModel.h"
#include "Core\\ModelBase.h"
#include "Core\Mesh.h"
#include "..\..\Core\CoreMinimal.h"
#include "..\\..\\Core\\Thread\\WorkerThread.h"
#include "..\\..\\Core\\Main\\Utility.h"

#include <d3d12.h>
#include <assimp\\scene.h>
#include <assimp\\Importer.hpp>
#include <assimp\\postprocess.h>
#include <vector>
#include <DirectXMath.h>
#include <cstdio>
#include <string>
#include <assimp\mesh.h>
#include <DirectXMathVector.inl>
#include <utility>
#include <memory>

using namespace KaguyaEngine::AsyncTask;
using namespace DirectX;

namespace KaguyaEngine::Model
{    
    // ------------------------------FBXModel------------------------------

	void PrimitiveModel::LoadModel(const std::string& modelPath)
	{
		Job asyncJob{};

        m_modelPath = modelPath;

        // ロード中に解放されないように
        ObjectPtr<PrimitiveModel> pThis = std::dynamic_pointer_cast<PrimitiveModel>(shared_from_this());

		asyncJob.func = [pThis, modelPath]()
			{
                printf("Start - %d\n", timeGetTime());
                Assimp::Importer importer;

                //ファイル拡張子を取得
                std::string extension = GetFileExtension(modelPath);

                //モデル読み込み時のフラグ。メッシュのポリゴンはすべて三角形に
                unsigned int flag = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
                    aiProcess_CalcTangentSpace | aiProcess_RemoveRedundantMaterials | aiProcess_GenUVCoords | aiProcess_OptimizeMeshes;
                //const aiScene* scene = importer.ReadFileFromMemory(buffer.data(), buffer.size(), flag, extension.c_str());
                const aiScene* scene = importer.ReadFile(modelPath.c_str(), flag);
                if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
                {
                    printf("FBXファイルをロードできませんでした。%s\n", importer.GetErrorString());
                    return;
                }

                pThis->m_needMeshCount = pThis->GetNeedMeshCount(scene->mRootNode);

                pThis->ProcessNode(scene, scene->mRootNode);

                pThis->CheckLoaded();
            };

        LockMutex lock(g_jobManager->m_mutex);
		g_jobManager->m_queue.push(std::move(asyncJob));
        g_jobManager->m_cv.notify_one();
	}

    void PrimitiveModel::LoadModel(const std::vector<char>& modelData, const std::string& format)
    {

    }

    void PrimitiveModel::LateUpdate()
    {
        if (!m_bLoaded.load()) {
            return;
        }

        ModelBase::LateUpdate();
    }

    void PrimitiveModel::ProcessNode(const aiScene* pScene, aiNode* pNode)
    {
        // ノードにメッシュが含まれていたら読み込む
        for (uint32 j = 0; j < pNode->mNumMeshes; j++)
        {
            // メッシュをロード
            aiMesh* pAIMesh = pScene->mMeshes[pNode->mMeshes[j]];
            uint32 meshIndex = static_cast<uint32>(m_pMeshes.size());

            std::vector<VertexPrimitive> vertices(pAIMesh->mNumVertices);
            std::vector<XMFLOAT3> vertexFloat3(pAIMesh->mNumVertices);
            std::vector<uint32> indices;

            // 頂点の処理
            for (uint32 i = 0; i < pAIMesh->mNumVertices; i++)
            {
                VertexPrimitive& vertex = vertices[i];
                vertex.position = { pAIMesh->mVertices[i].x, pAIMesh->mVertices[i].y, pAIMesh->mVertices[i].z };
                vertex.normal = { pAIMesh->mNormals[i].x, pAIMesh->mNormals[i].y, pAIMesh->mNormals[i].z };
                if (pAIMesh->mTextureCoords[0]) {
                    vertex.texCoords = { pAIMesh->mTextureCoords[0][i].x, pAIMesh->mTextureCoords[0][i].y };
                }
                else {
                    vertex.texCoords = { 0.0f, 0.0f };
                }
                vertex.tangent = { pAIMesh->mTangents[i].x, pAIMesh->mTangents[i].y, pAIMesh->mTangents[i].z };
                vertex.bitangent = { pAIMesh->mBitangents[i].x, pAIMesh->mBitangents[i].y, pAIMesh->mBitangents[i].z };
                vertex.vertexID = i;

                vertexFloat3[i] = vertex.position;
            }

            // インデックスの処理
            for (uint32 i = 0; i < pAIMesh->mNumFaces; i++)
            {
                aiFace& face = pAIMesh->mFaces[i];
                for (uint32 j = 0; j < face.mNumIndices; j++) {
                    indices.push_back(face.mIndices[j]);
                }
            }

            ObjectPtr<PrimitiveMesh> pMesh = CreateObjectPtr<PrimitiveMesh>(m_pDevice, meshIndex);
            pMesh->SetFrameResource(this);
            pMesh->CreateConstantBuffer();
            pMesh->m_meshName = pNode->mName.C_Str();

            // 初期位置、角度を取得
            XMMATRIX matrix = GetMeshDefaultMatrix(pNode);
            pMesh->m_position = XMFLOAT3(matrix.r[3].m128_f32[0], matrix.r[3].m128_f32[1], matrix.r[3].m128_f32[2]) / 100.0f;
            XMVECTOR rotation = ExtractEulerAngles(matrix);
            pMesh->m_rotation = XMFLOAT4(XMVectorGetX(rotation), XMVectorGetY(rotation), XMVectorGetZ(rotation), 0.0f);

			bool bExistMesh = CreateBuffer<MeshData>(pMesh.get(), vertices, indices, sizeof(VertexPrimitive), meshIndex);

			m_pMeshes.push_back(pMesh);

            if (!bExistMesh)
            {
                pMesh->m_pMeshData->vertices = vertexFloat3;
                pMesh->m_pMeshData->indices = indices;
            }

            m_nowCPULoadedMeshCount.store(m_nowCPULoadedMeshCount.load() + 1);
        }

        for (uint32 i = 0; i < pNode->mNumChildren; i++)
        {
            aiNode* childNode = pNode->mChildren[i];
            ProcessNode(pScene, childNode);
        }
    }
}
