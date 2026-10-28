#include "Engine.h"
#include "Core\\CoreMinimal.h"
#include "Core\\Main\\WindowSetting.h"
#include "Core\\Thread\\WorkerThread.h"
#include "Core\\Thread\\MainThreadWorker.h"
#include "Core\\DescriptorHeap\\DescriptorHeap.h"
#include "Core\\Movie\\VideoAcceration.h"
#include "UI\\UIManager.h"
#include "3D\\Camera\\Camera.h"
#include "3D\\Material\\Core\\MaterialManager.h"
#include "3D\\Model\\Core\\ModelManager.h"
#include "3D\\Lights\\DirectionalLight.h"
#include "Core\\Input\\Input.h"
#include "3D\\Skybox\\SkyBox.h"

#include <thread>
#include <Windows.h>
#include <cstdio>
#include <memory>
#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <chrono>
#include <algorithm>
#include <DirectXMathVector.inl>
#include <DirectXMathConvert.inl>
#include <string.h>
#include <vector>

#if _DEBUG
#include <pix3.h>
#include <pix3_win.h>
#endif

Engine* g_Engine = nullptr;

using namespace KaguyaEngine::AsyncTask;
using namespace KaguyaEngine::Material;
using namespace KaguyaEngine::Texture;
using namespace KaguyaEngine::Light;
using namespace KaguyaEngine::Model;
using namespace KaguyaEngine::Core;
using namespace KaguyaEngine::UI;
using namespace KaguyaEngine;

using namespace DirectX;

Engine::Engine(HWND hwnd)
	: m_hWnd(hwnd)
	, m_directXDevice(hwnd)
	, m_pCurrentBackBufferIndex(nullptr)
	, m_bClearRenderTexture(true)
	, m_frameRate(120)
	, m_frameTime(0.0f)
	, m_cpuTimeMS(0.0f)
	, m_gpuTimeMS(0.0f)
	, m_queryData(D3D12_QUERY_DATA_PIPELINE_STATISTICS())
{
	m_nowWindowSize = DEFAULT_WINDOW_SIZE;

	g_Engine = this;

	// ハードウェアのスレッド数を取得し、メインスレッドとコピースレッドを除いた非同期処理用のスレッドを作成
	int threadCount = static_cast<int>(std::thread::hardware_concurrency()) - 2;
	if (threadCount <= 0) {
		threadCount = 1;
	}
	for (int i = 0; i < threadCount; i++)
	{
		m_workerThreads.emplace_back(CreateUniquePtr<KaguyaEngine::AsyncTask::WorkerThread>());
		m_workerThreads[m_workerThreads.size() - 1]->StartThread();
	}

	// FrameTime計測用
	m_prevTime = std::chrono::high_resolution_clock::now();
	m_frameTime = 0.0f;
}

Engine::~Engine()
{
	// 起動中のスレッドを停止
	for (const UniquePtr<KaguyaEngine::AsyncTask::WorkerThread>& thread : m_workerThreads) {
		thread->StopThread();
	}
}

bool Engine::Initialize()
{
	// 別スレッドの処理結果をメインスレッドに伝えるクラスを初期化
	MainThreadWorker::CreateInstance();

	bool bSuccess = m_directXDevice.CreateDevice(m_nowWindowSize);
	if (!bSuccess)
	{
		printf("DirectXDeviceの初期化に失敗\n");
		return false;
	}

	// メインディスクリプタヒープの初期化
	m_pCopyDescriptor = CreateObjectPtr<KaguyaEngine::Core::CopyDescriptor>();
	m_pCopyDescriptor->pMainHeap = CreateUniquePtr<DescriptorHeap>();
	m_pCopyDescriptor->pMainHeap->Initialize(m_directXDevice.GetDevice(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_pCopyDescriptor->pCopyHeap = CreateUniquePtr<DescriptorHeap>();
	m_pCopyDescriptor->pCopyHeap->Initialize(m_directXDevice.GetDevice(), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_pCurrentBackBufferIndex = m_directXDevice.GetCurrentBackBufferIndex();

	m_frameResource.SetFrameResource(m_pCopyDescriptor.get(), GetDevice(), m_directXDevice.GetFrameResources(), m_pCurrentBackBufferIndex, &m_frameTime);

	// メインカメラを初期化
	m_pMainCamera = CreateObjectPtr<Camera>(m_directXDevice.GetRtvHeap(), m_directXDevice.GetDsvHeap(), m_nowWindowSize, &m_nowWindowSize);
	m_pMainCamera->SetFrameResource(&m_frameResource);
	m_pMainCamera->Initialize();
	m_pSortedCameras.push_back(m_pMainCamera);

	/*
	XMUINT2 subCameraSize(500, 1010);
	ObjectPtr<Camera> pSubCamera = CreateObjectPtr<Camera>(m_directXDevice.GetRtvHeap(), m_directXDevice.GetDsvHeap(), subCameraSize, &m_nowWindowSize);
	pSubCamera->SetFrameResource(&m_frameResource);
	pSubCamera->Initialize();
	pSubCamera->m_priority = 1;
	m_pSortedCameras.push_back(pSubCamera);
	*/
	// マテリアルマネージャーを初期化
	m_pMaterialManager = CreateObjectPtr<MaterialManager>(&m_frameResource);
	// モデルマネージャーを初期化
	m_pModelManager = CreateObjectPtr<ModelManager>(&m_frameResource);
	// ディレクショナルライトを初期化
	m_pDirectionalLight = CreateObjectPtr<DirectionalLight>();
	// ビデオアクセラレーションを初期化
	m_pVideoAcceration = CreateObjectPtr<VideoAcceration>();
	m_pVideoAcceration->Initialize(m_directXDevice.GetDevice(), m_directXDevice.GetQueue());

	// キーボード、Padの初期化
	m_pInput = CreateObjectPtr<Input>(m_hWnd);
	// スカイボックスを初期化
	m_pSkyBox = CreateObjectPtr<SkyBox>();
	m_pSkyBox->SetFrameResource(&m_frameResource);
	m_pSkyBox->Initialize();

	m_pUIManager = CreateObjectPtr<UIManager>(m_directXDevice.GetRtvHeap(), m_pCopyDescriptor.get(), &m_nowWindowSize);
	m_pUIManager->SetFrameResource(&m_frameResource);
	m_pUIManager->Initialize();

	std::vector<Camera*> cameraList;
	cameraList.push_back(m_pMainCamera.get());
	//cameraList.push_back(pSubCamera.get());
	m_directXDevice.UpdateRenderTargets(m_pCopyDescriptor.get(), cameraList, m_pUIManager->GetUIRenderTarget());
	m_pMaterialManager->UpdateCameraCount(static_cast<uint32>(cameraList.size()));
	m_pSkyBox->UpdateCameraCount(static_cast<uint32>(cameraList.size()));

	if (!CreateQuery()) {
		printf("CommandQueueの初期化に失敗\n");
		return false;
	}

	return true;
}

void Engine::Update()
{
	auto nowTime = std::chrono::high_resolution_clock::now();
	// 前回の計測時間と比較しフレーム時間を計算
	m_frameTime = std::chrono::duration<float>(nowTime - m_prevTime).count();
	m_prevTime = nowTime;
	// フリーズ対策
	if (m_frameTime > 0.5f) {
		m_frameTime = 0.5f;
	}

	if (*m_pCurrentBackBufferIndex == 0) {
		// クエリ結果を取得
		void* mappedData;
		D3D12_RANGE readRange = { 0, sizeof(m_queryData) };
		m_pQueryResult->Map(0, &readRange, &mappedData);
		if (mappedData) {
			memcpy(&m_queryData, mappedData, sizeof(m_queryData));
			m_pQueryResult->Unmap(0, nullptr);
		}

		uint64* timestamps = nullptr;
		m_pTimeStampQuaryResult->Map(0, nullptr, reinterpret_cast<void**>(&timestamps));
		if (timestamps)
		{
			uint64 start = timestamps[0];
			uint64 end = timestamps[1];

			m_pTimeStampQuaryResult->Unmap(0, nullptr);

			// GPUタイマー周波数を取得
			uint64 freq = 0;
			m_directXDevice.GetQueue()->GetTimestampFrequency(&freq);

			//GPU時間（ms）
			m_gpuTimeMS = (end - start) / static_cast<float>(freq) * 1000.0f;
		}
	}

	// コマンドリストをリセット
	m_directXDevice.BeginFrame();

	m_pVideoAcceration->BeginVideoCommand();

	// フレームの時間計測
	m_cpuFrameStart = std::chrono::high_resolution_clock::now();

	// 実際にGPUに送信するヒープは毎フレーム空にする
	m_pCopyDescriptor->pCopyHeap->ClearAllDescriptors();

	m_pModelManager->Update();

	// メインスレッドで非同期処理の結果を受け取る
	g_mainThreadWorker->ExecuteMainThreadTasks();

	m_pInput->Update();

	UpdateCamera(m_pSortedCameras[0].get());

#if _DEBUG
	if (m_pInput->GetKeyStateSync(DIK_F11))
	{
		PIXGpuCaptureNextFrames(L"CaptureResult.wpix", 1);
	}
#endif
}

void Engine::LateUpdate()
{
	for (const ObjectPtr<Camera>& camera : m_pSortedCameras)
	{
		camera->LateUpdate();
	}

	m_pDirectionalLight->LateUpdate();
	m_pModelManager->LateUpdate();
	m_pUIManager->LateUpdate();

	sort(m_pSortedCameras.begin(), m_pSortedCameras.end(), [](const ObjectPtr<Camera>& a, const ObjectPtr<Camera>& b)
		{
			return *a < *b;
		});

	// 動画のデコード処理を実行
	m_pVideoAcceration->EndVideoCommand();
}

void Engine::UpdateCamera(Camera* pCamera)
{
	// カメラの前方ベクトルを計算
	XMVECTOR forward = XMVector3Normalize(pCamera->GetTargetPos() - pCamera->GetEyePos());
	// カメラの右方向ベクトルを計算
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(pCamera->GetUpFoward(), forward));
	XMFLOAT3 fowardDir, rightDir;
	XMStoreFloat3(&fowardDir, forward);
	XMStoreFloat3(&rightDir, right);

	float cameraSpeed = 1.2f * m_frameTime;
	float rotationSpeed = 0.25f * m_frameTime;

	if (m_pInput->GetKeyState(DIK_LCONTROL)) {
		cameraSpeed *= 10.0f;
	}

	// 上昇
	if (m_pInput->GetKeyState(DIK_SPACE)) {
		pCamera->m_position.y += cameraSpeed;
	}
	// 下降
	else if (m_pInput->GetKeyState(DIK_LSHIFT)) {
		pCamera->m_position.y -= cameraSpeed;
	}

	// 右移動、左移動
	if (m_pInput->GetKeyState(DIK_A)) {
		pCamera->m_position += rightDir * cameraSpeed;
	}
	else if (m_pInput->GetKeyState(DIK_D)) {
		pCamera->m_position -= rightDir * cameraSpeed;
	}

	// 前進、後進
	if (m_pInput->GetKeyState(DIK_S)) {
		pCamera->m_position -= fowardDir * cameraSpeed;
	}
	else if (m_pInput->GetKeyState(DIK_W)) {
		pCamera->m_position += fowardDir * cameraSpeed;
	}

	// マウス移動量による視点変更
	POINT mouseDelta = m_pInput->GetMouseMove();

	// yawとpitchを更新
	pCamera->m_rotation.y -= mouseDelta.x * rotationSpeed;
	pCamera->m_rotation.x += mouseDelta.y * rotationSpeed;

	// 垂直回転角度の制限
	const float pitchLimit = XM_PIDIV2 - 0.01f;
	pCamera->m_rotation.x = std::clamp(pCamera->m_rotation.x, -pitchLimit, pitchLimit);
}

bool Engine::CreateQuery()
{
	D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
	queryHeapDesc.Count = 1; // 計測する回数
	queryHeapDesc.NodeMask = 0;
	HRESULT hr = m_directXDevice.GetDevice()->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_pQueryHeap));

	if (FAILED(hr)) {
		return false;
	}

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS));
	CD3DX12_HEAP_PROPERTIES prop(D3D12_HEAP_TYPE_READBACK);
	hr = m_directXDevice.GetDevice()->CreateCommittedResource(
		&prop,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_pQueryResult)
	);

	if (FAILED(hr)) {
		return false;
	}

	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	queryHeapDesc.Count = 2; // Start/End用
	hr = m_directXDevice.GetDevice()->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_pTimeStampQueryHeap));

	if (FAILED(hr)) {
		return false;
	}

	// 結果格納用バッファ
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint64) * 2);
	hr = m_directXDevice.GetDevice()->CreateCommittedResource(
		&prop,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_pTimeStampQuaryResult)
	);

	return !FAILED(hr);
}

void Engine::Begin3DRender()
{
	if (*m_pCurrentBackBufferIndex == 0) {
		GetCommandList()->BeginQuery(m_pQueryHeap.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
		GetCommandList()->EndQuery(m_pTimeStampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
	}

	ID3D12DescriptorHeap* ppHeaps[] = { m_pCopyDescriptor->pCopyHeap->GetHeap() };
	GetCommandList()->SetDescriptorHeaps(1, ppHeaps);

	// 3Dモデルの描画前に行う処理
	m_pModelManager->BeforeDrawUpdate();

	for (uint32 cameraIndex = 0; cameraIndex < static_cast<uint32>(m_pSortedCameras.size()); cameraIndex++)
	{
		m_pMaterialManager->LateUpdate(m_pSortedCameras[cameraIndex].get(), m_pDirectionalLight.get(), cameraIndex);
		m_pSkyBox->LateUpdate(m_pSortedCameras[cameraIndex].get(), cameraIndex);

		// 深度情報を書き込むモデルの描画を開始
		m_pSortedCameras[cameraIndex]->BeginDraw(true);

		// 3Dモデルの描画
		m_pMaterialManager->Draw(cameraIndex);

		// 深度情報を書き込まないモデルの描画を開始
		m_pSortedCameras[cameraIndex]->EndDraw3D();

		// スカイボックスの描画
		m_pSkyBox->Draw(cameraIndex);
	}

	// 3Dモデルの描画後に行う処理
	m_pModelManager->AfterDrawUpdate();
}

void Engine::Begin2DRender()
{
	m_pUIManager->BeginRender();
}

void Engine::EndRender()
{
	for (size_t cameraIndex = 0; cameraIndex < m_pSortedCameras.size(); cameraIndex++)
	{
		// 描画を終了し、ポストプロセスをかける
		m_pSortedCameras[cameraIndex]->EndDraw();
	}

	m_pUIManager->EndRender();

	m_directXDevice.FinalRender();

	// 3フレームごとにGPU時間を計測
	if (*m_pCurrentBackBufferIndex == 0) {
		GetCommandList()->EndQuery(m_pQueryHeap.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);

		GetCommandList()->ResolveQueryData(
			m_pQueryHeap.Get(),
			D3D12_QUERY_TYPE_PIPELINE_STATISTICS,
			0, 1,
			m_pQueryResult.Get(),
			0
		);

		//GPUの時間計測終了
		GetCommandList()->EndQuery(m_pTimeStampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
		GetCommandList()->ResolveQueryData(m_pTimeStampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, m_pTimeStampQuaryResult.Get(), 0);
	}

	// CPU時間計測終了
	m_cpuFrameEnd = std::chrono::high_resolution_clock::now();
	m_cpuTimeMS = std::chrono::duration<float, std::milli>(m_cpuFrameEnd - m_cpuFrameStart).count();

	// 描画
	m_directXDevice.ExecuteCommandList();
}
