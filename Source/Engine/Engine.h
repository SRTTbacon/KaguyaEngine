//*****************************************************************************
//
// エンジンのメインクラス。各コンポーネントはこのクラスを経由して取得します。
// このクラスはGame.hで作成されるため、インスタンスを作成する必要はありません。
//
// Engine.h
//
// T_Takeuchi
//
// 2026/5/24
//
//*****************************************************************************

#pragma once

#include "Core\\CoreMinimal.h"
#include "Core\\FrameBuffer.h"
#include "Core\\DirectXCore\\DirectXDevice.h"
#include "Core\\Thread\\WorkerThread.h"

#include <DirectXMath.h>
#include <d3d12.h>
#include <Windows.h>
#include <vector>
#include <chrono>

namespace KaguyaEngine
{
	namespace UI
	{
		class UIManager;
		class UITexture;
	}
	namespace Texture
	{
		class Texture2D;
	}
	namespace Core
	{
		struct CopyDescriptor;
	}
	namespace Material
	{
		class MaterialManager;
	}
	namespace Light
	{
		class DirectionalLight;
	}
	namespace Model
	{
		class FBXModel;
		class ModelManager;
	}

	class Camera;
	class Input;
	class SkyBox;
}

class Engine
{
public:
	Engine(HWND hwnd);
	~Engine();

	bool Initialize();

	void Update();
	void LateUpdate();

	void UpdateCamera(KaguyaEngine::Camera* pCamera);

	void Begin3DRender();
	void Begin2DRender();
	void EndRender();

public:
	inline ID3D12Device6* GetDevice() const { return m_directXDevice.GetDevice(); }
	inline ID3D12GraphicsCommandList4* GetCommandList() const { return m_directXDevice.GetCommandList(); }

	// カメラ関係
	inline KaguyaEngine::Camera* GetMainCamera() const { return m_pMainCamera.get(); }
	inline KaguyaEngine::Camera* GetCameraIndex(uint32 index) const { return m_pSortedCameras[index].get(); }
	inline uint32 GetCameraCount() const { return static_cast<uint32>(m_pSortedCameras.size()); }

	inline KaguyaEngine::Input* GetInput() const { return m_pInput.get(); }
	inline KaguyaEngine::UI::UIManager* GetUIManager() const { return m_pUIManager.get(); }
	inline KaguyaEngine::Model::ModelManager* GetModelManager() const { return m_pModelManager.get(); }
	inline KaguyaEngine::Material::MaterialManager* GetMaterialManager() const { return m_pMaterialManager.get(); }
	inline KaguyaEngine::Light::DirectionalLight* GetDirectionalLight() const { return m_pDirectionalLight.get(); }
	inline KaguyaEngine::SkyBox* GetSkyBox() const { return m_pSkyBox.get(); }

	DirectXDevice* GetDirectXDevice() { return &m_directXDevice; }

	inline FrameResource* GetFrameResource() { return &m_frameResource; }
	inline const int* GetBackBufferIndex() const { return m_pCurrentBackBufferIndex; }

	inline float GetCPUTime() const { return m_cpuTimeMS; }
	inline float GetGPUTime() const { return m_gpuTimeMS; }

public:
	// 3D描画用のレンダーテクスチャを毎フレーム真っ黒にするかどうか
	// 毎フレーム、スカイボックスなどで全ピクセル確実に再描画されることが保障される場合はfalse推奨 (デフォルト : true)
	bool m_bClearRenderTexture;

private:	// -----エンジン関連の初期化-----
	HWND m_hWnd;	// ウィンドウハンドル

	DirectXDevice m_directXDevice;	//DirectX12の初期化と管理を行うクラス
	std::vector<UniquePtr<KaguyaEngine::AsyncTask::WorkerThread>> m_workerThreads;	// 非同期スレッド

private:	// -----ウィンドウに関連する変数-----
	DirectX::XMUINT2 m_nowWindowSize;

	FrameResource m_frameResource;

	std::vector<ObjectPtr<KaguyaEngine::Camera>> m_pSortedCameras;

	ObjectPtr<KaguyaEngine::Core::CopyDescriptor> m_pCopyDescriptor;
	ObjectPtr<KaguyaEngine::Camera> m_pMainCamera;
	ObjectPtr<KaguyaEngine::Material::MaterialManager> m_pMaterialManager;
	ObjectPtr<KaguyaEngine::Model::ModelManager> m_pModelManager;
	ObjectPtr<KaguyaEngine::UI::UIManager> m_pUIManager;
	ObjectPtr<KaguyaEngine::Light::DirectionalLight> m_pDirectionalLight;
	ObjectPtr<KaguyaEngine::Input> m_pInput;
	ObjectPtr<KaguyaEngine::SkyBox> m_pSkyBox;

	ComPtr<ID3D12QueryHeap> m_pQueryHeap;	//クエリヒープ
	ComPtr<ID3D12Resource> m_pQueryResult;	//クエリ結果格納用バッファ
	ComPtr<ID3D12QueryHeap> m_pTimeStampQueryHeap;	//タイムスタンプクエリヒープ
	ComPtr<ID3D12Resource> m_pTimeStampQuaryResult;	//タイムスタンプクエリ結果格納用バッファ
	D3D12_QUERY_DATA_PIPELINE_STATISTICS m_queryData;

	const int* m_pCurrentBackBufferIndex;

	//フレームレート
	uint32 m_frameRate;	//30～

	// 経過時間計測用
	std::chrono::high_resolution_clock::time_point m_cpuFrameStart, m_cpuFrameEnd, m_prevTime;
	float m_frameTime;	// 1フレームにかかった時間
	float m_cpuTimeMS;
	float m_gpuTimeMS;	// GPUの処理時間

private:
	bool CreateQuery();
};

//どこからでも参照するためグローバル変数
extern Engine* g_Engine;
