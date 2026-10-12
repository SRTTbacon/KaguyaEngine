//*****************************************************************************
//
// マテリアル関係の基底クラス (CommonVS、CommonPSを継承しているシェーダーのみ)
// このクラス単体では動作を想定していません。必ず派生クラスを利用してください。
//
// MaterialBase.h
//
// T_Takeuchi
//
// 2026/6/9
//
//*****************************************************************************

#pragma once

#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\EngineDefine.h"
#include "..\\..\\..\\Core\\FrameBuffer.h"

#include <d3d12.h>
#include <string>
#include <DirectXMath.h>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>

namespace KaguyaEngine::Core
{
	struct PsoDescKey;
}
namespace KaguyaEngine::Model
{
	class Mesh;
}
namespace KaguyaEngine::Texture
{
	class Texture2D;
}

namespace KaguyaEngine::Material
{
	struct UniquePSBuffer
	{
		DirectX::XMMATRIX modelMatrix;         //モデル全体のマトリックス
		DirectX::XMMATRIX modelMatrixPrev;     //前のフレームのモデル全体のマトリックス
		DirectX::XMMATRIX meshMatrix;          //メッシュ単位のマトリックス
		DirectX::XMMATRIX meshMatrixPrev;      //前のフレームのメッシュ単位のマトリックス
		DirectX::XMMATRIX normalMatrix;        //モデルのスケール、回転などをinput.normalにも適応する用
	};

	class MaterialBase : public FrameResource, public std::enable_shared_from_this<MaterialBase>
	{
	public:
		MaterialBase(FrameResource* pFrameResource, uint8 textureCount);
		virtual ~MaterialBase();

		// MaterialManager用コールバック関数 (ユーザーは指定しない)
		void SetOnPipelineChanged(std::function<void(MaterialBase* pThis, uint64 beforeHashID, uint64 afterHashID)> callback);

		// マテリアルの初期化。 ユーザーが必ず実行させる
		// 引数 : cullMode 両面描画または片面描画, bTransparentMode 半透明をサポートするかどうか
		virtual void Initialize(D3D12_CULL_MODE cullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_FRONT, bool bTransparentMode = false);

		// マテリアルの更新 (エンジンで実行される)
		virtual void LateUpdate();

		// マテリアル設定をGPUに送信
		virtual void Draw();
		// このマテリアルと紐づけられているメッシュをすべて描画
		virtual void DrawMesh(class KaguyaEngine::Model::Mesh* pMesh);

		// 描画対象のメッシュを追加
		virtual void AddMesh(KaguyaEngine::Model::Mesh* pMesh);
		// メッシュをマテリアルから削除
		virtual void RemoveMesh(KaguyaEngine::Model::Mesh* pMesh);
		// 指定したインデックスのテクスチャを書き換える (非同期読み込み)
		virtual void SetTexture(uint32 index, const std::string& textureFilePath);
		// 指定したインデックスのテクスチャを書き換える (非同期読み込み)
		virtual void SetTexture(uint32 index, const std::string& textureFilePath, const D3D12_SHADER_RESOURCE_VIEW_DESC& shaderResource);
		// 指定したインデックスのテクスチャを書き換える (非同期読み込み)
		virtual void SetTexture(uint32 index, const char* data, size_t size, bool bDDSFile);
		// 指定したインデックスのテクスチャを書き換える (非同期読み込み)
		virtual void SetTexture(uint32 index, const char* data, size_t size, bool bDDSFile, const D3D12_SHADER_RESOURCE_VIEW_DESC& shaderResource);
		// 指定したインデックスのテクスチャを書き換える
		virtual void SetTexture(uint32 index, ObjectPtr<KaguyaEngine::Texture::Texture2D> pTexture);
		// 指定したインデックスのテクスチャを書き換える
		virtual void SetTexture(uint32 index, ObjectPtr<KaguyaEngine::Texture::Texture2D> pTexture, const D3D12_SHADER_RESOURCE_VIEW_DESC& shaderResource);
		// 指定したインデックスのテクスチャを削除 (白色にする)
		virtual void RemoveTexture(uint32 index);

	public:
		// 半透明を有効/無効化
		void SetEnableTransparent(bool bEnable);
		void SetMaterialName(std::string materialName);

		// 半透明マテリアルかどうか取得
		inline bool GetEnableTransparent() const{ return m_bTransparent; }
		// マテリアル名を取得
		inline std::string GetMaterialName() const { return m_materialName; }
		// このマテリアルのパスを取得
		inline std::string GetMaterialPath() const { return m_materialPath; }
		// 紐づけられている頂点シェーダーのパスを取得
		inline std::string GetVertexShaderPath() const { return m_vertexShaderPath; }
		// 紐づけられているピクセルシェーダーのパスを取得
		inline std::string GetPixelShaderPath() const { return m_pixelShaderPath; }
		// 紐づけられているパイプラインのハッシュIDを取得
		inline uint64 GetPipelineID() const { return m_pipelineHashID; }
		// 使用するテクスチャ数 (ピクセルシェーダーt0～の連番数)
		inline uint8 GetTextureCount() const { return m_textureCount; }

		virtual bool IsCanDraw() const { return m_dummyTexture.mainDescriptorIndex != UINT32_MAX; }


	public:
		// マテリアル固有データ
		// ※本体は子クラスで持つ
		UniquePSBuffer* m_pUniquePSData;

	protected:
		// Initialize時またはSetEnableTransparentを呼び出された際に実行
		// マテリアルごとに異なるため純粋仮想関数
		virtual void CreatePipeline() = 0;
		// Initialize時に実行
		// ピクセルシェーダーに送信するバッファを作成。マテリアルごとに内容が異なるため純粋仮想関数
		virtual void CreateUniquePSBuffer() = 0;

		virtual void CreatePipeline(std::vector<D3D12_ROOT_PARAMETER>& rootParameters, std::vector<D3D12_INPUT_ELEMENT_DESC>& inputElements, std::vector<D3D12_STATIC_SAMPLER_DESC>& samplers, Core::PsoDescKey& desc);

		// Primitiveの頂点シェーダーに送信する頂点情報を設定
		// 独自のシェーダーで別の情報が必要な場合はこの関数は使用できません。
		virtual void GetDefaultInputElements(std::vector<D3D12_INPUT_ELEMENT_DESC>& inputElements);

		// 基本的にすべての3Dオブジェクトのピクセルシェーダーで使用するルートパラメーターを設定
		// 独自のシェーダーで(b0, space1)を使用しないものであれば必要ありません。
		// ルートパラメーターの0～3まで使用します。
		virtual void GetDefaultRootParameters(std::vector<D3D12_ROOT_PARAMETER>& rootParameters);

		// だいたい1つはサンプラーを持たせる
		// 独自のシェーダーでサンプラーを変える場合は必要ありません。
		virtual void GetDefaultSamplers(std::vector<D3D12_STATIC_SAMPLER_DESC>& samplers);

		// 3DモデルのデフォルトのPsoDescを設定
		virtual void GetDefailtPsoDesc(Core::PsoDescKey& psoDesc);

		// UniquePSBufferまたはUniquePSBufferを継承した固有バッファを作成
		// pBuffer : UniquePSBufferを継承したデータ
		// bufferSize : sizeof(UniquePSBuffer)
		virtual void CreateUniquePSBuffer(UniquePSBuffer* pBuffer, uint32 bufferSize);

	protected:
		std::vector<ComPtr<ID3D12Resource>> m_textureBuffers;	// テクスチャバッファのポインタの配列
		ComPtr<ID3D12Resource> m_pUniquePSBuffer[FRAME_BUFFER_COUNT];   // ピクセルシェーダー用の固有定数バッファ
		void* m_pUniquePSBufferMapped[FRAME_BUFFER_COUNT];

		std::vector<KaguyaEngine::Model::Mesh*> m_pMeshes;	// このマテリアルに割り当てられているメッシュの配列
		std::vector<MaterialTexture> m_textures;	// シェーダーに割り当てられている数分テクスチャを格納
		MaterialTexture m_dummyTexture;				// テクスチャが未設定またはロード中の場合に使用する真っ白のテクスチャ

		std::string m_materialName;
		std::string m_materialPath;

		std::string m_vertexShaderPath;
		std::string m_pixelShaderPath;

		uint64 m_pipelineHashID;	// パイプライン
		uint32 m_uniqueBufferSize;
		int m_textureRootIndex;		// RootSignatureのテクスチャスロット (-1は何もしない)

		D3D12_CULL_MODE m_cullMode;
		const uint8 m_textureCount;
		bool m_bTransparent;

	private:
		std::function<void(MaterialBase* pThis, uint64 beforeHashID, uint64 afterHashID)> m_onPipelineChanged;

		void ResetTexture(uint32 index);
	};
}
