//*****************************************************************************
//
// キャラクターを読み込むクラス
// 基本的には非同期で読み込まれます。IsLoaded()関数、コールバック等で読み込みが完了しているか確認してください。
//
// CharacterModel.h
//
// T_Takeuchi
//
// 2026/6/21
//
//*****************************************************************************

#pragma once

#include "Core\\ModelBase.h"
#include "Core\\Mesh.h"
#include "Core\\Bone.h"
#include "..\..\Core\CoreMinimal.h"
#include "..\..\Core\EngineDefine.h"

#include <d3d12.h>
#include <string>
#include <memory>
#include <vector>
#include <DirectXMath.h>
#include <unordered_map>
#include <assimp\scene.h>
#include <assimp\mesh.h>
#include <cstdint>

namespace KaguyaEngine::Model
{
	struct CharacterMeshData : public MeshData
	{
		ComPtr<ID3D12Resource> shapeDeltasBuffer;       // 各頂点に対するシェイプキーの位置情報
        uint32 mainShapeDeltaHeapIndex;
        uint32 copyShapeDeltaHeapIndex;

        CharacterMeshData() : mainShapeDeltaHeapIndex(UINT32_MAX), copyShapeDeltaHeapIndex(UINT32_MAX) {}
	};

    struct CharacterConstantBuffer : public MeshConstantBuffer
    {
        uint32 shapeCount;
    };

    class CharacterMesh : public Mesh
    {
    public:
        CharacterMesh(ID3D12Device* pDevice, uint32 meshIndex);
        virtual ~CharacterMesh() override;
        virtual void CreateConstantBuffer() override;
        virtual void LateUpdate() override;

    public:
        ComPtr<ID3D12Resource> m_pShapeWeightsBuffer[FRAME_BUFFER_COUNT];      //シェイプキーのウェイト情報
        void* m_pShapeWeightsBufferMapped[FRAME_BUFFER_COUNT];
    };

    struct HumanoidList
    {
        std::vector<std::vector<DirectX::XMFLOAT3>> shapeDeltas;      // 各頂点のシェイプキー適応後の位置を格納 (頂点の数 * シェイプキーの数だけ要素数を確保)
        std::vector<std::string> meshNames;
    };

    //ヒューマノイドモデル
    struct HumanoidMesh {
        CharacterMesh* pMesh;                                   // メッシュ情報のポインタ
        std::unordered_map<std::string, uint32> shapeMapping;   // シェイプキー名からインデックスを取得
        std::vector<float> shapeWeights;                        // メッシュごとのシェイプキーのウェイト一覧
        bool bChangeShapeValue[FRAME_BUFFER_COUNT];             // ウェイトを変更したらtrue

        HumanoidMesh();
    };

    class CharacterModel : public ModelBase
    {
    public:
        CharacterModel();
        ~CharacterModel();

        // ファイルからFBXモデルを読み込む
        void LoadModel(const std::string& modelPath);

		// エンジンから呼び出されるため、ユーザーが直接呼び出す必要はありません
        virtual void Update() override;
        // エンジンから呼び出されるため、ユーザーが直接呼び出す必要はありません
        virtual void LateUpdate() override;

        void UpdateBonePosition(std::string boneName, const DirectX::XMFLOAT3& position);
        void UpdateBoneRotation(std::string boneName, const DirectX::XMFLOAT4& rotation);
        void UpdateBoneScale(std::string boneName, const DirectX::XMFLOAT3& scale);
        void SetShapeWeight(const std::string shapeName, float weight);

    public:
        ID3D12Resource* GetBoneMatricesBuffer() const { return m_boneMatricesBuffer[*m_pCurrentBackBufferIndex].Get(); }
        BoneManager& GetBoneManager() { return m_boneManager; }

        HumanoidList* GetHumanoidList() { return s_sharedHumanoidMeshes[m_modelPath].get(); }
        HumanoidMesh& GetHumanoidMesh(uint32 meshIndex) { return m_humanoidMeshes[meshIndex]; }

        void* GetBoneMatricesBufferMapped() const { return m_pBoneMatricesMap[*m_pCurrentBackBufferIndex]; }

    protected:
        virtual void ReleaseModel() override;

    private:
        //シェーダーに渡す頂点情報
        struct CharacterVertex : public VertexPrimitive
        {
            DirectX::XMFLOAT4 boneWeights;  // ボーンの影響度
            uint32 boneIDs[4];              // 4つのボーンから影響を受ける
        };

    private:
        // メッシュを取得
        void ProcessNode(const aiScene* pScene, aiNode* pNode);
        // ボーン情報を.fbxから取得
        void LoadBones(aiMesh* pMesh, std::vector<CharacterVertex>& vertices);
        // ボーンの親子関係を取得
        void LoadBoneFamily(aiNode* pNode);
        // シェイプキー情報をロード
        void LoadShapeKey(const aiMesh* pMesh, std::vector<CharacterVertex>& vertices, HumanoidMesh& humanoidMesh) const;
        // 必要なヒューマノイドバッファを作成
        void CreateHumanoidBuffer(HumanoidMesh& humanoidMesh);
        // シェイプキー用のGPUバッファを作成
        void CreateShapeDeltasTexture();

    private:
        static std::unordered_map<std::string, std::shared_ptr<HumanoidList>> s_sharedHumanoidMeshes;    // 各モデルのメッシュを保持

        ComPtr<ID3D12Resource> m_boneMatricesBuffer[FRAME_BUFFER_COUNT];    // ボーン情報をシェーダーに送信する用
        void* m_pBoneMatricesMap[FRAME_BUFFER_COUNT];                       // ボーン情報のポインタ
        std::vector<ComPtr<ID3D12Resource>> m_shapeBuffers;                 // 各メッシュのシェイプキーバッファ

        std::vector<HumanoidMesh> m_humanoidMeshes;             // ヒューマノイド用のメッシュ情報

        BoneManager m_boneManager;      // ボーンに関する情報を保持する構造体

		uint8 m_loadModelCount; 	// 非同期読み込み中にカウントが変更されれば解放させる用
    };
}
