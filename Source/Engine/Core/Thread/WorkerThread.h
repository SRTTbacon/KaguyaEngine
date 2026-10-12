//*****************************************************************************
//
// 非同期処理
// このクラスのスレッドはEngine.hで作成されるため、インスタンスを作成する必要はありません.
//
// WorkerThread.h
//
// T_Takeuchi
//
// 2026/5/24
//
//*****************************************************************************

#pragma once

#include "..\\CoreMinimal.h"

#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace KaguyaEngine::AsyncTask
{
	struct Job
	{
		std::function<void()> func;			// 別スレッドで実行する関数
		std::function<void()> OnComplete;	// 別スレッドでの処理が完了した後にメインスレッドで呼び出されるコールバック関数
	};

	struct JobManager
	{
		std::mutex m_mutex;
		std::queue<Job> m_queue;
		std::condition_variable m_cv;
	};

	// JobManagerのグローバルインスタンス
	extern UniquePtr<JobManager> g_jobManager;

	class WorkerThread
	{
	public:
		WorkerThread();

		void StartThread();
		void StopThread();

	private:
		void MainLoop();

		std::thread m_thread;

		std::atomic<bool> m_bIsRunning;
	};
}
