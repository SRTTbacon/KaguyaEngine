#include "ResourceCopyThread.h"
#include "..\CoreMinimal.h"
#include "..\\Thread\\MainThreadWorker.h"

#include <d3d12.h>
#include <Windows.h>
#include <thread>
#include <mutex>
#include <cstdio>
#include <functional>
#include <vector>

namespace KaguyaEngine::AsyncTask
{
	UniquePtr<CopyJobManager> g_copyJobManager;
	UniquePtr<ResourceCopyThread> g_resourceCopyThread;

	void ResourceCopyThread::CreateInstance(ID3D12Device* pDevice)
	{
		if (!g_resourceCopyThread)
			g_resourceCopyThread = CreateUniquePtr<ResourceCopyThread>(pDevice);

		if (!g_copyJobManager)
			g_copyJobManager = CreateUniquePtr<CopyJobManager>();
	}

	ResourceCopyThread::ResourceCopyThread(ID3D12Device* pDevice)
		: m_copyFenceValue(0)
		, m_pDevice(pDevice)
		, m_bIsRunning(true)
	{
		// Copy用アロケータ
		HRESULT hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_pCopyAllocator));
		if (FAILED(hr)) {
			return;
		}
		m_pCopyAllocator->SetName(L"m_pCopyAllocator");

		// コピー用コマンドリスト
		hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_pCopyAllocator.Get(), nullptr, IID_PPV_ARGS(&m_pCopyCommandList));
		if (FAILED(hr)) {
			return;
		}
		m_pCopyCommandList->SetName(L"m_pCopyCommandList");
		m_pCopyCommandList->Close();

		m_pCopyAllocator->Reset();
		m_pCopyCommandList->Reset(m_pCopyAllocator.Get(), nullptr);

		// Copy用コマンドキュー
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		hr = m_pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(m_pCopyQueue.ReleaseAndGetAddressOf()));
		if (FAILED(hr)) {
			return;
		}

		// Copy用フェンス
		hr = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_pCopyFence.ReleaseAndGetAddressOf()));
		if (FAILED(hr)) {
			return;
		}
		m_copyFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		// Copyスレッドを作成
		m_thread = std::thread(&ResourceCopyThread::CopyCommandThread, this);
	}

	ResourceCopyThread::~ResourceCopyThread()
	{
		CloseHandle(m_copyFenceEvent);
	}

	void ResourceCopyThread::StopThread()
	{
		m_bIsRunning.store(false);

		g_copyJobManager->m_cv.notify_all(); // スレッドが待機している場合に備えて通知
		if (m_thread.joinable()) {
			m_thread.join();
		}
	}

	void ResourceCopyThread::CopyCommandThread()
	{
		while (m_bIsRunning.load())
		{
			std::unique_lock lock(g_copyJobManager->m_mutex);

			// ジョブがキューに追加されるか、スレッドが停止されるのを待機
			g_copyJobManager->m_cv.wait(lock, [&]()
				{
					return !g_copyJobManager->m_jobs.empty() || !m_bIsRunning.load();
				});

			if (!m_bIsRunning.load()) break;

			// メインスレッドをブロックさせないため、コピージョブはすべてコピーしておく
			std::vector<CopyJob> copyJobs;
			copyJobs.swap(g_copyJobManager->m_jobs);

			// ジョブをクリア
			g_copyJobManager->m_jobs.clear();

			lock.unlock();

			// コピージョブが1つでもあればすべて実行する
			for (size_t i = 0; i < copyJobs.size(); i++)
			{
				// ジョブをキューから取得して実行
				CopyJob& job = copyJobs[i];
				if (job.func) {
					job.func(m_pCopyCommandList.Get());
				}
			}

			// コピーコマンドを実行
			ExecuteCopyCommands();

			// コピー完了後のコールバックを呼び出す
			for (size_t i = 0; i < copyJobs.size(); i++)
			{
				if (copyJobs[i].OnComplete)
				{
					std::lock_guard lock(g_mainThreadWorker->g_mainThreadTaskMutex);

					MainThreadTask task{};
					task.func = copyJobs[i].OnComplete;

					g_mainThreadWorker->g_mainThreadTasks.push(task);
				}
			}
		}
	}
	void ResourceCopyThread::ExecuteCopyCommands()
	{
		// コマンドリストに追加されたコマンドを実行
		m_pCopyCommandList->Close();
		ID3D12CommandList* commandLists[] = { m_pCopyCommandList.Get() };
		m_pCopyQueue->ExecuteCommandLists(1, commandLists);

		// フェンスをシグナル
		const uint64 currentFenceValue = ++m_copyFenceValue;
		HRESULT hr = m_pCopyQueue->Signal(m_pCopyFence.Get(), currentFenceValue);
		if (FAILED(hr)) {
			printf("フェンスシグナルに失敗しました。");
		}

		// 完了待機
		hr = m_pCopyFence->SetEventOnCompletion(currentFenceValue, m_copyFenceEvent);
		if (FAILED(hr)) {
			printf("フェンスイベント設定に失敗しました。");
		}

		// フェンスイベントがシグナルされるまで待機
		WaitForSingleObject(m_copyFenceEvent, INFINITE);

		// コマンドリストとアロケーターをリセットして、次のコピーコマンドの準備をする
		hr = m_pCopyAllocator->Reset();
		if (FAILED(hr)) {
			printf("CopyAllocatorのリセットに失敗しました。");
		}
		hr = m_pCopyCommandList->Reset(m_pCopyAllocator.Get(), nullptr);
		if (FAILED(hr)) {
			printf("CopyCommandListのリセットに失敗しました。");
		}
	}
}
