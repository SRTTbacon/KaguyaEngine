//*****************************************************************************
//
// 非同期でのリソースコピー
// このクラスのスレッドはDirectXDevice.hで作成されるため、インスタンスを作成する必要はありません。
//
// ResourceCopyThread.h
//
// T_Takeuchi
//
// 2026/5/24
//
//*****************************************************************************

#pragma once

#include "..\\CoreMinimal.h"

#include <d3d12.h>
#include <functional>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <Windows.h>
#include <thread>
#include <atomic>

namespace KaguyaEngine::AsyncTask
{
	struct CopyJob
	{
		std::function<void(ID3D12GraphicsCommandList*)> func;	// コピースレッドで実行する関数
		std::function<void()> OnComplete;						// コピースレッドでの処理が完了した後にメインスレッドで呼び出されるコールバック関数
	};

	struct CopyJobManager
	{
		std::vector<CopyJob> m_jobs;
		std::mutex m_mutex;
		std::condition_variable m_cv;
	};

	// CopyJobManagerのグローバルインスタンス
	extern UniquePtr<CopyJobManager> g_copyJobManager;

	class ResourceCopyThread
	{
	public:
		static void CreateInstance(ID3D12Device* pDevice);

		ResourceCopyThread(ID3D12Device* pDevice);
		~ResourceCopyThread();
		void StopThread();

	public:
		ID3D12CommandList* GetCopyCommandList() const { return m_pCopyCommandList.Get(); }

	private:
		void CopyCommandThread();
		void ExecuteCopyCommands();

	private:
		ID3D12Device* m_pDevice;
		ComPtr<ID3D12CommandAllocator> m_pCopyAllocator;			//Copy用のアロケーター
		ComPtr<ID3D12GraphicsCommandList> m_pCopyCommandList;		//Copy用のコマンドリスト
		ComPtr<ID3D12CommandQueue> m_pCopyQueue;	//Copy用のコマンドキュー
		ComPtr<ID3D12Fence> m_pCopyFence;			//Copy用フェンス
		HANDLE m_copyFenceEvent = nullptr;		//Copy用のフェンスで使うイベント
		uint64 m_copyFenceValue;					//Copy用のフェンスの値

		std::thread m_thread;	//コピー用スレッド

		std::atomic<bool> m_bIsRunning;		//スレッドが動いているか
	};

	extern UniquePtr<ResourceCopyThread> g_resourceCopyThread;
}