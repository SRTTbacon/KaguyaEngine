//*****************************************************************************
//
// マテリアル管理クラス
// すべての3D描画はこのクラスを通して行う
//
// MaterialManager.h
//
// T_Takeuchi
//
// 2026/6/10
//
//*****************************************************************************

#pragma once

#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\FrameBuffer.h"
#include "..\\..\\..\\Core\\EngineDefine.h"
#include "MaterialBase.h"

#include <d3d12.h>
#include <DirectXMath.h>
#include <concepts>
#include <utility>
#include <vector>
#include <array>

namespace KaguyaEngine
{
	namespace Light
	{
		class DirectionalLight;
	}

	class Camera;
}

namespace KaguyaEngine::Material
{
	class MaterialManager : public FrameResource
	{
	public:
		MaterialManager(FrameResource* pFrameResource);
		~MaterialManager();

		// カメラの数が変更された際にバッファ数を更新
		// エンジンから呼び出される
		void UpdateCameraCount(uint32 cameraCount);

		// 全マテリアル共通のバッファを更新
		// エンジンから呼び出される
		void LateUpdate(KaguyaEngine::Camera* pCamera, KaguyaEngine::Light::DirectionalLight* pLight, uint32 cameraIndex = 0);

		// 全メッシュ描画
		// エンジンから呼び出される
		void Draw(uint32 cameraIndex = 0);

		// MaterialBaseを継承したマテリアルを作成
		// @param 可変引数で、pFrameResource*を除いたもの
		// pFrameResourceは自動で挿入されるため、それ以降の引数を入力してください
		template<typename T, typename... Args>
		requires std::derived_from<T, MaterialBase>
		ObjectPtr<T> CreateMaterial(Args&&... args);

	public:
		struct CommonVSBuffer
		{
			DirectX::XMMATRIX viewMatrix;			// カメラのビューマトリックス
			DirectX::XMMATRIX viewMatrixPrev;		// 前フレームのカメラのビューマトリックス
			DirectX::XMMATRIX projectionMatrix;		// カメラのプロジェクションマトリックス
			DirectX::XMMATRIX projectionMatrixPrev;	// 前フレームのカメラのプロジェクションマトリックス
			DirectX::XMMATRIX lightViewProj;		// ライトのビュープロジェクション
			DirectX::XMFLOAT4 lightPos;				// ライトの位置
			DirectX::XMFLOAT4 cameraPos;			// カメラの位置
			DirectX::XMFLOAT4 cameraJitters;		// カメラのジャイター情報
			float time;								// 時間情報
		};

		struct CommonPSBuffer
		{
			DirectX::XMUINT2 renderSize;		// レンダーターゲットのサイズ
		};

		inline CommonVSBuffer* GetCommonVSBuffer() { return &m_commonVSBuffer; }
		inline CommonPSBuffer* GetCommonPSBuffer() { return &m_commonPSBuffer; }

	private:
		std::vector<ObjectPtr<MaterialBase>> m_pAllMaterials;

		Map<uint64, std::vector<MaterialBase*>> m_sharedOpaquePipelines;		// 不透明パイプライン
		Map<uint64, std::vector<MaterialBase*>> m_sharedTransparentPipelines;	// 半透明パイプライン

		CommonVSBuffer m_commonVSBuffer;	// 頂点シェーダー用の共通定数バッファ
		CommonPSBuffer m_commonPSBuffer;    // ピクセルシェーダー用の共通定数バッファ

		//カメラの数だけ保持する必要がある
		std::vector<std::array<ComPtr<ID3D12Resource>, MAX_CAMERA_COUNT>> m_pCommonVSBuffers;	// 頂点シェーダー用の共通定数バッファ
		std::vector<std::array<ComPtr<ID3D12Resource>, MAX_CAMERA_COUNT>> m_pCommonPSBuffers;	// ピクセルシェーダー用の共通定数バッファ
		std::vector<std::array<void*, MAX_CAMERA_COUNT>> m_pCommonVSBufferMapped;				// m_pCommonVSBufferのアクセス先
		std::vector<std::array<void*, MAX_CAMERA_COUNT>> m_pCommonPSBufferMapped;				// m_pCommonPSBufferのアクセス先

		void OnPipelineChanged(MaterialBase* pMaterial, uint64 beforeHashID, uint64 afterHashID);

		void CreateConstantBuffer(D3D12_RESOURCE_DESC& desc, ComPtr<ID3D12Resource>& pOutResource, void** pOutMapped);
	};

	template<typename T, typename ...Args>
	requires std::derived_from<T, MaterialBase>
	inline ObjectPtr<T> MaterialManager::CreateMaterial(Args && ...args)
	{
		// マテリアル作成
		ObjectPtr<T> pMaterial = CreateObjectPtr<T>(this, std::forward<Args>(args)...);
		MaterialBase* pMaterialBase = static_cast<MaterialBase*>(pMaterial.get());
		// パイプラインを作成または変更した場合呼び出される
		pMaterialBase->SetOnPipelineChanged(
			[this](MaterialBase* pThis, uint64 beforeHashID, uint64 afterHashID)
			{
				OnPipelineChanged(pThis, beforeHashID, afterHashID);
			});

		m_pAllMaterials.push_back(pMaterial);
		return pMaterial;
	}
}
