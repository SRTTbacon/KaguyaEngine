//*****************************************************************************
//
// メッシュ情報を保持する構造体を定義
//
// Mesh.h
//
// T_Takeuchi
//
// 2026/6/2
//
//*****************************************************************************

#pragma once

#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\EngineDefine.h"
#include "..\\..\\Transform\\Transform.h"
#include "..\..\..\Core\DirectXCore\ResourceStatics.h"

#include <d3d12.h>
#include <d3dcommon.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <concepts>
#include <mutex>

// MaterialBaseの前方宣言
namespace KaguyaEngine::Material
{
    class MaterialBase;
}

namespace KaguyaEngine::Model
{
    // フラスタムカリング用
    struct InstanceData
    {
        DirectX::XMFLOAT4 minBounds;
        DirectX::XMFLOAT4 maxBounds;
        DirectX::XMMATRIX worldMatrix;
        uint32 instanceID;
    };

    // 描画タイプ
    enum DrawType
    {
        DrawType_Main,              // 通常描画
        DrawType_DirectionalShadow, // ディレクショナルライト用の影の描画
        DrawType_PointShadow,       // ポイントライト用の影の描画
    };

	// 全メッシュ共通情報 (CommonVS.hlsl)
    // ModelManagerクラスで制御
    struct CommonConstantBuffer
	{
        DirectX::XMMATRIX viewMatrix;       // カメラのビューマトリックス
        DirectX::XMMATRIX viewMatrixPrev;   // 前フレームのカメラのビューマトリックス
        DirectX::XMMATRIX projectionMatrix; // カメラのプロジェクションマトリックス
        DirectX::XMMATRIX lightViewProj;    // ライトのビュープロジェクション
        DirectX::XMFLOAT4 lightPos;         // ライトの位置
        DirectX::XMFLOAT4 cameraPos;        // カメラの位置
        float time;                         // 時間情報
		DirectX::XMFLOAT3 padding;          // 16バイト境界に合わせるためのパディング
	};

	// メッシュ固有の不変情報 (CommonVS.hlsl)
    struct MeshInfoBuffer
    {
		uint32 vertexCount;   // 頂点数
		uint32 indexCount;    // インデックス数
    };

    struct MeshData
    {
        ComPtr<ID3D12Resource> vertexBuffer;            //頂点バッファ(GPUメモリに頂点情報を保存)
        ComPtr<ID3D12Resource> indexBuffer;             //インデックスバッファ(GPUメモリに入っている頂点バッファの各頂点にインデックスを付けて保存)
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView;      //頂点バッファのデータ内容とサイズを保持
        D3D12_INDEX_BUFFER_VIEW indexBufferView;        //インデックスバッファのデータ内容とサイズを保持

        std::vector<DirectX::XMFLOAT3> vertices;
        std::vector<uint32> indices;

        MeshData()
            : indexBufferView(D3D12_INDEX_BUFFER_VIEW())
            , vertexBufferView(D3D12_VERTEX_BUFFER_VIEW())
        {}
    };

    // メッシュを共有するための構造体
    struct MeshDataList
    {
        struct MeshDataMain
        {
            std::string meshName;
            std::string materialName;
            DirectX::XMFLOAT3 position;
            DirectX::XMFLOAT4 rotation;
        };

        int refCount;
        std::vector<ObjectPtr<MeshData>> meshDataList;
        std::vector<MeshDataMain> meshMain;
        //std::vector<Animation> animations;

        MeshDataList()
            : refCount(0)
        {}

        void Release()
        {
            refCount--;
            if (refCount <= 0) {
                meshDataList.clear();
            }
        }
    };

    extern Map<std::string, MeshDataList> g_sharedMeshes;   // 各モデルのメッシュを保持 (key : ファイルパス, value : メッシュリスト)
    extern std::mutex g_sharedMeshesMutex;                  // g_sharedMeshesは非同期で書き換えられる

    // メッシュのマトリックス情報 (CommonVS.hlsl)
    struct MeshConstantBuffer
    {
        DirectX::XMMATRIX modelMatrix;      // モデル全体の位置、角度、スケール
        DirectX::XMMATRIX modelMatrixPrev;  // 前フレームのモデル全体の位置、角度、スケール
        DirectX::XMMATRIX meshMatrix;       // メッシュの位置、角度、スケール
        DirectX::XMMATRIX meshMatrixPrev;   // 前フレームのメッシュの位置、角度、スケール
        DirectX::XMMATRIX normalMatrix;     // メッシュとモデル全体のマトリックスを掛けたもの
    };

    // メッシュ情報の基底クラス
    class Mesh : public Transform
    {
    public:
        Mesh(ID3D12Device* pDevice, uint32 meshIndex);
        ~Mesh();

        // メッシュバッファがMeshConstantBufferではない場合は手動でCreateConstantBuffer<T>()を呼び出してください
        virtual void CreateConstantBuffer() {};

        // 各メッシュの定数バッファを作成
		// クラス作成後に呼び出す必要がある
        // 引数 : MeshConstantBufferのサイズ (継承している場合は全体のサイズ)
        template<typename T>
        requires std::derived_from<T, MeshConstantBuffer>
		void CreateConstantBuffer(uint32 bufferSize);

        // マテリアルを設定
        void SetMaterial(Material::MaterialBase* pMaterial);

		// シェーダーに渡す定数バッファの内容を更新
        virtual void LateUpdate();

        // 通常描画。単純に頂点、インデックス情報のみを送信
        // MaterialBaseクラスのDraw()から呼び出される
        virtual void Draw(uint32 instanceCount = 1, D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 影の描画
        // MaterialBaseクラスのDrawShadowMesh()から呼び出される
        virtual void DrawShadow(const DirectX::XMMATRIX& lightViewProj, DrawType drawType) {}

    public:
        inline ID3D12Resource* GetMeshConstantBuffer() const { return m_pMeshConstantBufferResource[*m_pCurrentBackBufferIndex].Get(); }
        inline ID3D12Resource* GetMeshConstantBuffer(uint32 backBufferIndex) const { return m_pMeshConstantBufferResource[backBufferIndex].Get(); }

        inline uint32 GetMeshIndex() const { return m_meshIndex; }

        inline bool CanDraw() const;
        bool CanDrawShadow() const;     // なぜかinlineを付けるとコンパイルエラー

    protected:
        // m_pMeshBufferDataをID3D12Resourceへコピー
        virtual void CopyMeshBuffer();

    protected:
        Material::MaterialBase* m_pMaterial;    // 割り当てられているマテリアル (nullptrの場合描画されません)

        ComPtr<ID3D12Resource> m_pMeshConstantBufferResource[FRAME_BUFFER_COUNT];   // メッシュ用定数バッファ (CommonVS.hlsl)
        void* m_pMeshConstantBufferMapped[FRAME_BUFFER_COUNT];                      // 定数バッファをマップしたときのCPU側のアドレス

        DirectX::XMMATRIX m_meshMatrix;             // ModelBaseクラスのLateUpdate()で更新される

        const uint32 m_meshIndex;   // メッシュインデックス

        uint32 m_bufferSize;

    public:
        ObjectPtr<MeshData> m_pMeshData;    // 共有メッシュデータ
        ObjectPtr<MeshConstantBuffer> m_pMeshBufferData;    // メッシュ単位の頂点シェーダーに送信するデータ
        class ModelBase* m_pParentModel;    // このメッシュを構成するモデルへのポインタ
        
        std::string m_meshName;     // メッシュ名

        bool m_bDraw;               // 通常描画するか
        bool m_bDrawShadow;         // 影を描画するか
        bool m_bStaticMesh;         // 静的メッシュか
    };

    __declspec(selectany) uint32 g_instanceCounter = 0;     // 各メッシュのインスタンスID取得用。メッシュを作るたびにプラス1。カウンターであり、総インスタンス数とは異なる。
    __declspec(selectany) std::vector<InstanceData*> g_instanceDataList;
    __declspec(selectany) std::vector<class Transform*> g_meshVector;


    template<typename T>
    requires std::derived_from<T, MeshConstantBuffer>
    inline void Mesh::CreateConstantBuffer(uint32 bufferSize)
    {
        //定数バッファをリソースとして作成
        for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
        {
            bool bSuccess = ResourceStatics::CreateResource(D3D12_HEAP_TYPE_UPLOAD, bufferSize, m_pMeshConstantBufferResource[i]);
            if (bSuccess)
            {
                m_pMeshConstantBufferResource[i]->Map(0, nullptr, &m_pMeshConstantBufferMapped[i]);
                m_pMeshConstantBufferResource[i]->SetName(L"meshContantBuffer");
            }
        }

        m_pMeshBufferData = CreateObjectPtr<T>();
        m_bufferSize = bufferSize;
    }

    // -------------------------------PrimitiveMesh------------------------------

    // デフォルトモデル用のメッシュクラス
    class PrimitiveMesh : public Mesh
    {
    public:
        PrimitiveMesh(ID3D12Device* pDevice, uint32 meshIndex);
        virtual ~PrimitiveMesh() override;
        virtual void CreateConstantBuffer() override;
        virtual void LateUpdate() override;
    };
}
