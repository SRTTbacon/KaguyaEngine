#include "CharacterModel.h"
#include "Core\Mesh.h"
#include "Core\Bone.h"
#include "Core\ModelBase.h"
#include "..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\Core\\EngineDefine.h"
#include "..\\..\\Core\\Main\Utility.h"
#include "..\\..\\Core\\DescriptorHeap\\DescriptorHeap.h"
#include "..\\..\\Core\\Thread\\WorkerThread.h"
#include "..\\..\\Core\\Thread\\ResourceCopyThread.h"
#include "..\\..\\Core\\DirectXCore\\ResourceStatics.h"

#include <assimp\\scene.h>
#include <assimp\\Importer.hpp>
#include <assimp\\postprocess.h>
#include <assimp\vector3.h>
#include <assimp\mesh.h>
#include <DirectXMath.h>
#include <DirectXMathVector.inl>
#include <DirectXMathConvert.inl>
#include <DirectXMathMatrix.inl>
#include <dxgiformat.h>
#include <memory>
#include <string>
#include <string.h>
#include <vector>
#include <unordered_map>
#include <utility>
#include <cstdio>
#include <d3d12.h>
#include <d3dx12.h>
#include <Windows.h>
#include <cstdint>

using namespace KaguyaEngine::AsyncTask;
using namespace DirectX;

namespace KaguyaEngine::Model
{
    std::unordered_map<std::string, std::shared_ptr<HumanoidList>> CharacterModel::s_sharedHumanoidMeshes;

    // -----------------------CharacterMesh-----------------------

    CharacterMesh::CharacterMesh(ID3D12Device* pDevice, uint32 meshIndex)
        : Mesh(pDevice, meshIndex)
    {
        for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
        {
            m_pShapeWeightsBufferMapped[i] = nullptr;
        }
    }

    CharacterMesh::~CharacterMesh()
    {
        // 保持しているバッファを解放
        for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
        {
            if (m_pShapeWeightsBuffer[i])
            {
                m_pShapeWeightsBuffer[i]->Unmap(0, nullptr);
                m_pShapeWeightsBufferMapped[i] = nullptr;
            }
        }
    }

    void CharacterMesh::CreateConstantBuffer()
    {
        Mesh::CreateConstantBuffer<CharacterConstantBuffer>(sizeof(CharacterConstantBuffer));
    }

    void CharacterMesh::LateUpdate()
    {
        //描画フラグが立っていない場合は更新しない
        if (!CanDraw() && !CanDrawShadow()) {
            return;
        }

        Mesh::LateUpdate();
    }

    // -----------------------HumanoidMesh-----------------------

    HumanoidMesh::HumanoidMesh()
        : pMesh(nullptr)
    {
        for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
            bChangeShapeValue[i] = false;
        }
    }

    // -----------------------CharacterModel-----------------------

    CharacterModel::CharacterModel()
        : m_boneManager(&m_position, &m_rotation, &m_scale)
        , m_loadModelCount(0)
    {
        for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
            m_pBoneMatricesMap[i] = nullptr;
        }
    }

    CharacterModel::~CharacterModel()
    {
        ReleaseModel();
    }

    void CharacterModel::LoadModel(const std::string& modelPath)
    {
        Job asyncJob{};

        m_modelPath = modelPath;

        // ロード中に解放されないように
        ObjectPtr<CharacterModel> pThis = std::dynamic_pointer_cast<CharacterModel>(shared_from_this());

        asyncJob.func = [pThis, modelPath]()
            {
                printf("Start - %d\n", timeGetTime());

                if (s_sharedHumanoidMeshes.find(modelPath) == s_sharedHumanoidMeshes.end()) {
                    std::shared_ptr<HumanoidList> pHumanoidList = std::make_shared<HumanoidList>();
                    s_sharedHumanoidMeshes.emplace(modelPath, pHumanoidList);
                }
                s_sharedHumanoidMeshes[modelPath].get()->shapeDeltas.clear();

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

                //ボーンに親子関係を付ける
                pThis->LoadBoneFamily(scene->mRootNode);

                pThis->CreateShapeDeltasTexture();

                HumanoidList* pHumanoidList = s_sharedHumanoidMeshes[modelPath].get();
                for (uint32 i = 0; i < static_cast<uint32>(pThis->m_pMeshes.size()); i++)
                {
					pHumanoidList->meshNames.push_back(pThis->m_pMeshes[i]->m_meshName);
                }

                pThis->CheckLoaded();
            };

        LockMutex lock(g_jobManager->m_mutex);
        g_jobManager->m_queue.push(std::move(asyncJob));
        g_jobManager->m_cv.notify_one();
    }

    void CharacterModel::Update()
    {
        if (!m_bLoaded.load()) {
            return;
        }

        ModelBase::Update();

        // ボーンのマトリックスを更新
        // 後半は1フレーム前のボーン情報のため、あらかじめコピーしておく
        XMMATRIX* dst = reinterpret_cast<XMMATRIX*>(reinterpret_cast<char*>(m_pBoneMatricesMap[*m_pCurrentBackBufferIndex]));
        memcpy(dst + BoneManager::MAX_BONE_COUNT, m_boneManager.m_boneInfos.data(), sizeof(XMMATRIX) * m_boneManager.m_boneInfos.size());
        m_boneManager.UpdateBoneMatrix();
    }

    void CharacterModel::LateUpdate()
    {
        if (!m_bLoaded.load()) {
            return;
        }

        // ボーンマトリックスバッファを更新
        // 最初4バイトはシェイプキーの数を割り当てているため、それ以降にコピー

        memcpy(m_pBoneMatricesMap[*m_pCurrentBackBufferIndex], m_boneManager.m_boneInfos.data(), sizeof(XMMATRIX) * m_boneManager.m_boneInfos.size()); //ボーンマトリックスをコピー

        // シェイプキーのウェイト情報を更新
        for (HumanoidMesh& mesh : m_humanoidMeshes)
        {
            // 毎フレームやると重たいので変更があれば実行
            if (mesh.pMesh->m_pShapeWeightsBuffer && mesh.bChangeShapeValue[*m_pCurrentBackBufferIndex])
            {
                // シェイプバッファにコピー
                memcpy(mesh.pMesh->m_pShapeWeightsBufferMapped[*m_pCurrentBackBufferIndex], mesh.shapeWeights.data(), sizeof(float) * mesh.shapeWeights.size()); //各シェイプキーのウェイトをコピー
                mesh.bChangeShapeValue[*m_pCurrentBackBufferIndex] = false;
            }
        }

        //キャラクターはルートモーションを考慮
        XMFLOAT3 armaturePos = m_boneManager.m_armatureBone.m_position;
        armaturePos.x = armaturePos.x;
        armaturePos.z = -armaturePos.z;
        XMFLOAT3 hipPos = XMFLOAT3(0.0f, 0.0f, 0.0f);
        if (m_boneManager.m_bones.size() > 0) {
            //m_bonesの0番目は基本的にルートボーン
            hipPos = m_boneManager.m_bones[0].m_position;
            float temp = hipPos.x;
            hipPos.x = -hipPos.z;
            hipPos.z = -temp;
        }

        // アニメーション時にルート位置が変わるのを考慮
        m_rootPosition = armaturePos + hipPos + m_position;

        for (size_t i = 0; i < m_pMeshes.size(); i++)
        {
            CharacterMeshData* pMeshData = static_cast<CharacterMeshData*>(m_pMeshes[i]->m_pMeshData.get());
            HumanoidMesh& humanoidMesh = m_humanoidMeshes[i];

            if (!pMeshData->shapeDeltasBuffer) {
                continue;
            }

            uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor();
            m_pDevice->CopyDescriptorsSimple(1, m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex), m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(pMeshData->mainShapeDeltaHeapIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            pMeshData->copyShapeDeltaHeapIndex = copyIndex;

            CharacterConstantBuffer* pBuffer = static_cast<CharacterConstantBuffer*>(m_pMeshes[i]->m_pMeshBufferData.get());
            pBuffer->shapeCount = static_cast<uint32>(humanoidMesh.shapeWeights.size());
        }

        ModelBase::LateUpdate();
    }

    //ボーンの位置を更新
    void CharacterModel::UpdateBonePosition(std::string boneName, const XMFLOAT3& position)
    {
        if (!m_boneManager.Exist(boneName)) {
            return;
        }

        UINT boneIndex = m_boneManager.m_boneMapping[boneName];

        m_boneManager.m_bones[boneIndex].m_position = position;
    }

    //ボーンの回転を変更
    void CharacterModel::UpdateBoneRotation(std::string boneName, const XMFLOAT4& rotation)
    {
        if (!m_boneManager.Exist(boneName)) {
            return;
        }

        UINT boneIndex = m_boneManager.m_boneMapping[boneName];

        m_boneManager.m_bones[boneIndex].m_rotation = rotation;
    }

    //ボーンのスケールを変更
    void CharacterModel::UpdateBoneScale(std::string boneName, const XMFLOAT3& scale)
    {
        if (!m_boneManager.Exist(boneName)) {
            return;
        }

        UINT boneIndex = m_boneManager.m_boneMapping[boneName];

        m_boneManager.m_bones[boneIndex].m_scale = scale;
    }

    //シェイプキーのウェイトを更新
    void CharacterModel::SetShapeWeight(const std::string shapeName, float weight)
    {
        if (weight < 0.0f)
            weight = 0.0f;
        else if (weight > 1.2f)
            weight = 1.2f;

        for (HumanoidMesh& mesh : m_humanoidMeshes) {
            if (mesh.shapeMapping.find(shapeName) == mesh.shapeMapping.end()) {
                continue;
            }

            UINT shapeIndex = mesh.shapeMapping[shapeName];
            mesh.shapeWeights[shapeIndex] = weight;
            for (UINT i = 0; i < FRAME_BUFFER_COUNT; i++) {
                mesh.bChangeShapeValue[*m_pCurrentBackBufferIndex] = true;
            }
        }
    }

    void CharacterModel::ReleaseModel()
    {
        ModelBase::ReleaseModel();

        s_sharedHumanoidMeshes.clear();
        m_shapeBuffers.clear();
        m_humanoidMeshes.clear();
        m_boneManager.ResetBoneManager();

        for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
        {
            if (m_boneMatricesBuffer[i])
            {
                m_boneMatricesBuffer[i]->Unmap(0, nullptr);
                m_pBoneMatricesMap[i] = nullptr;
            }
        }

        for (ObjectPtr<Mesh> pMesh : m_pMeshes)
        {
            if (!pMesh) {
                continue;
            }

            CharacterMeshData* pMeshData = static_cast<CharacterMeshData*>(pMesh->m_pMeshData.get());
            if (pMeshData->mainShapeDeltaHeapIndex != UINT32_MAX)
            {
                m_pDescriptor->pMainHeap->ClearDescriptor(pMeshData->mainShapeDeltaHeapIndex);
            }
        }
    }

    void CharacterModel::ProcessNode(const aiScene* pScene, aiNode* pNode)
    {
        // ノードにメッシュが含まれていたら読み込む
        for (uint32 j = 0; j < pNode->mNumMeshes; j++) {
            aiMesh* pMesh = pScene->mMeshes[pNode->mMeshes[j]];
            HumanoidMesh humanoidMesh{};
            uint32 meshIndex = static_cast<uint32>(m_pMeshes.size());

            std::vector<CharacterVertex> vertices(pMesh->mNumVertices);
            std::vector<XMFLOAT3> vertexFloat3(pMesh->mNumVertices);
            std::vector<uint32> indices;

            // 頂点シェーダーに送信する頂点構造
            for (uint32 i = 0; i < pMesh->mNumVertices; i++) {
                CharacterVertex& vertex = vertices[i];

                vertex.position = { pMesh->mVertices[i].x, pMesh->mVertices[i].y, pMesh->mVertices[i].z };
                vertex.normal = { pMesh->mNormals[i].x, pMesh->mNormals[i].y, pMesh->mNormals[i].z };
                if (pMesh->mTextureCoords[0]) {
                    vertex.texCoords = { pMesh->mTextureCoords[0][i].x, pMesh->mTextureCoords[0][i].y };
                }
                else {
                    vertex.texCoords = { 0.0f, 0.0f };
                }
                vertex.tangent = { pMesh->mTangents[i].x, pMesh->mTangents[i].y, pMesh->mTangents[i].z };
                vertex.bitangent = { pMesh->mBitangents[i].x, pMesh->mBitangents[i].y, pMesh->mBitangents[i].z };

                vertex.boneWeights = { 0.0f, 0.0f, 0.0f, 0.0f };
                vertex.boneIDs[0] = vertex.boneIDs[1] = vertex.boneIDs[2] = vertex.boneIDs[3] = 0;

                vertex.vertexID = i;

                vertexFloat3[i] = vertex.position;
            }

            // インデックス
            for (uint32 i = 0; i < pMesh->mNumFaces; i++) {
                aiFace face = pMesh->mFaces[i];
                for (uint32 j = 0; j < face.mNumIndices; j++) {
                    indices.push_back(face.mIndices[j]);
                }
            }

            // メッシュを作成
            ObjectPtr<CharacterMesh> pPrimitiveMesh = CreateObjectPtr<CharacterMesh>(m_pDevice, meshIndex);
            pPrimitiveMesh->SetFrameResource(this);
            pPrimitiveMesh->CreateConstantBuffer();
            pPrimitiveMesh->m_meshName = pNode->mName.C_Str();

            // 初期位置、角度を取得
            XMMATRIX matrix = GetMeshDefaultMatrix(pNode);
            pPrimitiveMesh->m_position = XMFLOAT3(matrix.r[3].m128_f32[0], matrix.r[3].m128_f32[1], matrix.r[3].m128_f32[2]) / 100.0f;
            XMVECTOR rotation = ExtractEulerAngles(matrix);
            pPrimitiveMesh->m_rotation = XMFLOAT4(XMVectorGetX(rotation), XMVectorGetY(rotation), XMVectorGetZ(rotation), 0.0f);

            //ボーンの処理
            LoadBones(pMesh, vertices);

            //シェイプキーの処理
            LoadShapeKey(pMesh, vertices, humanoidMesh);

            bool bExistMesh = CreateBuffer<CharacterMeshData>(pPrimitiveMesh.get(), vertices, indices, sizeof(CharacterVertex), meshIndex);
            humanoidMesh.pMesh = pPrimitiveMesh.get();

            //シェーダーに必要なバッファを生成
            CreateHumanoidBuffer(humanoidMesh);

            if (!bExistMesh)
            {
                pPrimitiveMesh->m_pMeshData->vertices = vertexFloat3;
                pPrimitiveMesh->m_pMeshData->indices = indices;
            }

            m_pMeshes.push_back(pPrimitiveMesh);

            m_humanoidMeshes.push_back(humanoidMesh);

            m_nowCPULoadedMeshCount.store(m_nowCPULoadedMeshCount.load() + 1);
        }

        for (uint32 i = 0; i < pNode->mNumChildren; i++) {
            aiNode* childNode = pNode->mChildren[i];
            ProcessNode(pScene, childNode);
        }
    }

    void CharacterModel::LoadBones(aiMesh* pMesh, std::vector<CharacterVertex>& vertices)
    {
        for (uint32 i = 0; i < pMesh->mNumBones; i++) {
            aiBone* bone = pMesh->mBones[i];
            uint32 boneIndex = 0;

            std::string boneName = bone->mName.C_Str();

            //ボーンがまだ登録されていない場合、マッピングを追加
            if (!m_boneManager.Exist(boneName)) {
                //追加するボーンのインデックスを取得
                boneIndex = static_cast<uint32>(m_boneManager.m_boneInfos.size());

                //ボーンを作成
                Bone* pBoneChild = m_boneManager.AddBone(boneName, boneIndex);

                //このボーンのグローバルマトリックス
                XMMATRIX localMatrix = XMMatrixTranspose(XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&bone->mOffsetMatrix)));
                pBoneChild->m_boneGlobalOffset = localMatrix;
            }
            else {
                //同名のボーンが存在する場合それを使用 (異なるメッシュ間で同名のボーンの場合、ほとんどが共有されているため)
                boneIndex = m_boneManager.m_boneMapping[bone->mName.C_Str()];
            }

            //ボーンの影響を受ける頂点の数ぶん繰り返す
            for (uint32 j = 0; j < bone->mNumWeights; j++) {
                //頂点インデックス
                uint32 vertexID = bone->mWeights[j].mVertexId;
                //その頂点がどのくらい影響を受けるか
                float weight = bone->mWeights[j].mWeight;

                //頂点を取得
                CharacterVertex& vertex = vertices[vertexID];

                //空いているボーンウェイトスロットを探す。 (最大4つのボーンから影響を受ける)
                if (vertex.boneWeights.x == 0.0f) {
                    vertex.boneIDs[0] = boneIndex;
                    vertex.boneWeights.x = weight;
                }
                else if (vertex.boneWeights.y == 0.0f) {
                    vertex.boneIDs[1] = boneIndex;
                    vertex.boneWeights.y = weight;
                }
                else if (vertex.boneWeights.z == 0.0f) {
                    vertex.boneIDs[2] = boneIndex;
                    vertex.boneWeights.z = weight;
                }
                else if (vertex.boneWeights.w == 0.0f) {
                    vertex.boneIDs[3] = boneIndex;
                    vertex.boneWeights.w = weight;
                }
                else {
                    //4つ以上のボーンウェイトはサポート外
                    //printf("頂点 %d は 4 つ以上のボーンウェイトを持っています。\n", vertexID);
                }
            }
        }
    }

    void CharacterModel::LoadBoneFamily(aiNode* pNode)
    {
        //ノード名のボーンが存在
        if (m_boneManager.Exist(pNode->mName.C_Str())) {
            UINT boneIndex = m_boneManager.m_boneMapping[pNode->mName.C_Str()];
            //ノードの子を列挙
            for (UINT i = 0; i < pNode->mNumChildren; i++) {
                //子ノードの名前のボーンが存在
                if (m_boneManager.m_boneMapping.find(pNode->mChildren[i]->mName.C_Str()) != m_boneManager.m_boneMapping.end()) {
                    UINT childIndex = m_boneManager.m_boneMapping[pNode->mChildren[i]->mName.C_Str()];

                    //子ボーンと親ボーンを設定
                    m_boneManager.m_bones[boneIndex].AddChildBone(&m_boneManager.m_bones[childIndex]);
                }
            }
        }

        //子ボーンも同様に設定
        for (UINT i = 0; i < pNode->mNumChildren; i++) {
            LoadBoneFamily(pNode->mChildren[i]);
        }
    }

    void CharacterModel::LoadShapeKey(const aiMesh* pMesh, std::vector<CharacterVertex>& vertices, HumanoidMesh& humanoidMesh) const
    {
        HumanoidList* pHumanoidList = s_sharedHumanoidMeshes[m_modelPath].get();
        uint32 meshIndex = static_cast<uint32>(pHumanoidList->shapeDeltas.size());
        pHumanoidList->shapeDeltas.push_back(std::vector<XMFLOAT3>());

        //メッシュに含まれるシェイプキー一覧
        for (uint32 i = 0; i < pMesh->mNumAnimMeshes; i++) {
            aiAnimMesh* animMesh = pMesh->mAnimMeshes[i];

            std::string shapeName = animMesh->mName.C_Str();

            //同名のシェイプキーが存在すれば無視 (ほぼないけど)
            if (humanoidMesh.shapeMapping.find(shapeName) == humanoidMesh.shapeMapping.end()) {
                uint32 shapeIndex = static_cast<uint32>(humanoidMesh.shapeWeights.size());

                humanoidMesh.shapeWeights.push_back(0.0f);
                humanoidMesh.shapeMapping[shapeName] = shapeIndex;

                for (uint32 j = 0; j < animMesh->mNumVertices; j++) {
                    aiVector3D& vec = animMesh->mVertices[j];
                    //シェイプキーの、100%のときのその頂点の位置
                    XMFLOAT3 shapePos = XMFLOAT3(vec.x, vec.y, vec.z);
                    //元の位置を引く
                    shapePos.x -= vertices[j].position.x;
                    shapePos.y -= vertices[j].position.y;
                    shapePos.z -= vertices[j].position.z;
                    pHumanoidList->shapeDeltas[meshIndex].push_back(shapePos);
                }
            }
        }
    }

    void CharacterModel::CreateHumanoidBuffer(HumanoidMesh& humanoidMesh)
    {
        // ヒープ設定
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        // ボーン情報のリソースを作成
        constexpr uint64 boneBufferSize = GetAlignedSize(sizeof(uint32) + sizeof(XMMATRIX) * static_cast<uint64>(BoneManager::MAX_BONE_COUNT) * 2);  // 4バイト + ボーン数511個が2つ
        CD3DX12_RESOURCE_DESC boneBuffer = CD3DX12_RESOURCE_DESC::Buffer(boneBufferSize);  // 前フレームの分も考慮して多めに確保
        for (uint32 i = 0; i < FRAME_BUFFER_COUNT; i++) {
            HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &boneBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_boneMatricesBuffer[i]));
            if (FAILED(hr)) {
                printf("ボーンバッファの生成に失敗しました。\n");
            }

            m_boneMatricesBuffer[i]->Map(0, nullptr, &m_pBoneMatricesMap[i]);
            m_boneMatricesBuffer[i]->SetName(L"boneMatricesBuffer");
        }

        // シェイプキーの情報を保持する用のリソースを作成
        if (humanoidMesh.shapeWeights.size() > 0) {
            // シェイプキーのウェイト情報を保持するリソースを作成
            uint64 shapeWeightsSize = GetAlignedSize(sizeof(uint32) + sizeof(float) * humanoidMesh.shapeWeights.size());
            D3D12_RESOURCE_DESC shapeWeightsBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(shapeWeightsSize);

            for (uint32 i = 0; i < FRAME_BUFFER_COUNT; i++) {
                HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &shapeWeightsBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&humanoidMesh.pMesh->m_pShapeWeightsBuffer[i]));
                if (FAILED(hr)) {
                    printf("シェイプキーウェイトバッファの生成に失敗しました。%ld\n", hr);
                    HRESULT removedReason = m_pDevice->GetDeviceRemovedReason();
                    printf("Device removed reason:%ld\n", removedReason);
                }

                // ウェイト情報の初期値を入れる (Update関数で常に更新される)
                humanoidMesh.pMesh->m_pShapeWeightsBuffer[i]->Map(0, nullptr, &humanoidMesh.pMesh->m_pShapeWeightsBufferMapped[i]);
                memcpy(humanoidMesh.pMesh->m_pShapeWeightsBufferMapped[i], humanoidMesh.shapeWeights.data(), shapeWeightsSize);
                humanoidMesh.pMesh->m_pShapeWeightsBuffer[i]->SetName(L"shapeWeightsBuffer");
            }
        }
    }
    void CharacterModel::CreateShapeDeltasTexture()
    {
        m_shapeBuffers.resize(m_humanoidMeshes.size());

        for (uint32 i = 0; i < static_cast<uint32>(m_humanoidMeshes.size()); i++)
        {
            HumanoidMesh& humanoidMesh = m_humanoidMeshes[i];
            size_t vertexCount = humanoidMesh.pMesh->m_pMeshData->vertices.size();
            size_t shapeCount = humanoidMesh.shapeWeights.size();

            if (shapeCount <= 0) {
                m_shapeBuffers[i] = nullptr;
                continue;
            }

            uint32 maxWidth = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION; // 横幅の最大 = 16384
            uint32 width = (vertexCount > maxWidth) ? maxWidth : static_cast<uint32>(vertexCount);
            uint32 height = static_cast<uint32>(((vertexCount + width - 1) / width) * shapeCount);

            size_t rowPitch = GetAlignedSize(width * sizeof(XMFLOAT4));

            // アップロードバッファを作成
            ResourceStatics::CreateUploadResource(rowPitch * height, m_shapeBuffers[i]);

            CharacterMeshData* pMeshData = static_cast<CharacterMeshData*>(humanoidMesh.pMesh->m_pMeshData.get());

            // シェイプキー用のリソースを作成
            CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, static_cast<uint32>(width), static_cast<uint32>(height), 1, 1);
            CD3DX12_HEAP_PROPERTIES texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            HRESULT result = m_pDevice->CreateCommittedResource(&texHeapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(pMeshData->shapeDeltasBuffer.ReleaseAndGetAddressOf()));
            if (FAILED(result)) {
                printf("シェイプキー用のリソ－スの作成に失敗しました。: エラーコード = %1x\n", result);
            }
            pMeshData->shapeDeltasBuffer->SetName(L"shapeDeltasBuffer");

            pMeshData->mainShapeDeltaHeapIndex = m_pDescriptor->pMainHeap->SetResource(pMeshData->shapeDeltasBuffer.Get(), DXGI_FORMAT_R32G32B32A32_FLOAT);
        }

        for (uint32 i = 0; i < static_cast<uint32>(m_humanoidMeshes.size()); i++) {
            HumanoidMesh& humanoidMesh = m_humanoidMeshes[i];

            if (!m_shapeBuffers[i]) {
                continue;
            }

            //必要なサイズを計算
            size_t vertexCount = humanoidMesh.pMesh->m_pMeshData->vertices.size();
            size_t shapeCount = humanoidMesh.shapeWeights.size();

            if (shapeCount <= 0) {
                continue;
            }

            uint32 maxWidth = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION; //横幅の最大 = 16384
            uint32 width = (vertexCount > maxWidth) ? maxWidth : static_cast<uint32>(vertexCount);
            uint32 height = static_cast<uint32>(((vertexCount + width - 1) / width) * shapeCount);

            size_t rowPitch = GetAlignedSize(width * sizeof(XMFLOAT4)); //256の倍数になるように
            size_t tempWidth = rowPitch / sizeof(XMFLOAT4);
            std::vector<XMFLOAT4> textureData(tempWidth * height, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f)); //初期化

            //シェイプキー情報をテクスチャに変換
            for (size_t shapeID = 0; shapeID < shapeCount; shapeID++) {
                for (size_t vertexID = 0; vertexID < vertexCount; vertexID++) {
                    size_t x = vertexID % tempWidth;
                    size_t y = (vertexID / tempWidth) * shapeCount + shapeID; //縦位置

                    //範囲チェック
                    if (y >= height) {
                        continue;
                    }

                    HumanoidList* pHumanoidList = s_sharedHumanoidMeshes[m_modelPath].get();
                    XMFLOAT3 delta = pHumanoidList->shapeDeltas[i][shapeID * vertexCount + vertexID];
                    textureData[y * tempWidth + x] = XMFLOAT4(delta.x, delta.y, delta.z, 0.0f);
                }
            }

            void* mappedData = nullptr;
            HRESULT hr = m_shapeBuffers[i]->Map(0, nullptr, &mappedData);
            if (FAILED(hr)) {
                printf("UploadBufferのマップに失敗しました。\n");
                return;
            }
            memcpy(mappedData, textureData.data(), rowPitch * height);
            m_shapeBuffers[i]->Unmap(0, nullptr);
        }

        // ロード中に解放されないように
        ObjectPtr<CharacterModel> pThis = std::dynamic_pointer_cast<CharacterModel>(shared_from_this());
        CopyJob copyJob{};
        copyJob.func = [pThis](ID3D12GraphicsCommandList* pCommandList)
            {
                for (uint32 i = 0; i < static_cast<uint32>(pThis->m_humanoidMeshes.size()); i++) {
                    HumanoidMesh& humanoidMesh = pThis->m_humanoidMeshes[i];

                    //必要なサイズを計算
                    size_t vertexCount = humanoidMesh.pMesh->m_pMeshData->vertices.size();
                    size_t shapeCount = humanoidMesh.shapeWeights.size();

                    if (shapeCount <= 0) {
                        continue;
                    }

                    uint32 maxWidth = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION; //横幅の最大 = 16384
                    uint32 width = (vertexCount > maxWidth) ? maxWidth : static_cast<UINT>(vertexCount);
                    uint32 height = static_cast<uint32>(((vertexCount + width - 1) / width) * shapeCount);
                    size_t rowPitch = GetAlignedSize(width * sizeof(XMFLOAT4)); //256の倍数になるように

                    CharacterMeshData* pMeshData = static_cast<CharacterMeshData*>(humanoidMesh.pMesh->m_pMeshData.get());

                    // リソースバリアでSTATEを変更
                    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(pMeshData->shapeDeltasBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                    pCommandList->ResourceBarrier(1, &barrier);

                    //コピー先を設定
                    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
                    dstLocation.pResource = pMeshData->shapeDeltasBuffer.Get();
                    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                    dstLocation.SubresourceIndex = 0;

                    //コピー元を設定
                    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
                    srcLocation.pResource = pThis->m_shapeBuffers[i].Get();
                    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                    srcLocation.PlacedFootprint.Footprint.Width = width;
                    srcLocation.PlacedFootprint.Footprint.Height = height;
                    srcLocation.PlacedFootprint.Footprint.Depth = 1;
                    srcLocation.PlacedFootprint.Footprint.RowPitch = static_cast<uint32>(rowPitch);
                    srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                    pCommandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
                }
            };
        copyJob.OnComplete = [pThis]()
            {
                for (uint32 i = 0; i < static_cast<uint32>(pThis->m_humanoidMeshes.size()); i++) {
                    HumanoidMesh& humanoidMesh = pThis->m_humanoidMeshes[i];
                    CharacterMeshData* pMeshData = static_cast<CharacterMeshData*>(humanoidMesh.pMesh->m_pMeshData.get());

                    if (humanoidMesh.shapeWeights.size() <= 0) {
                        continue;
                    }

                    // リソースバリアでSTATEを変更
                    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(pMeshData->shapeDeltasBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    pThis->GetCommandList()->ResourceBarrier(1, &barrier);

                    pThis->m_shapeBuffers.clear();
                }
            };
        LockMutex lock(g_copyJobManager->m_mutex);
        g_copyJobManager->m_jobs.push_back(copyJob);
        g_copyJobManager->m_cv.notify_one();
    }
}
