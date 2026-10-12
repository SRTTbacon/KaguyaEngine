#include "WorkerThread.h"
#include "..\CoreMinimal.h"
#include "MainThreadWorker.h"

#include <thread>
#include <mutex>
#include <utility>
#include <memory>
#include <Windows.h>

namespace KaguyaEngine::AsyncTask
{
	UniquePtr<JobManager> g_jobManager = nullptr;

	WorkerThread::WorkerThread()
		: m_bIsRunning(false)
	{
		// JobManagerのインスタンスを作成
		if (!g_jobManager) {
			g_jobManager = CreateUniquePtr<JobManager>();
		}
	}

	void WorkerThread::StartThread()
	{
		m_bIsRunning.store(true);

		m_thread = std::thread(&WorkerThread::MainLoop, this);
	}

	void WorkerThread::StopThread()
	{
		m_bIsRunning.store(false);
		g_jobManager->m_cv.notify_all(); // スレッドが待機している場合に備えて通知
		if (m_thread.joinable()) {
			m_thread.join();
		}
	}

	void WorkerThread::MainLoop()
	{
		// スレッドが停止されるまでループ
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		while (m_bIsRunning.load())
		{
			LockMutex lock(g_jobManager->m_mutex);

			// ジョブがキューに追加されるか、スレッドが停止されるのを待機
			g_jobManager->m_cv.wait(lock, [&]()
				{
					return !g_jobManager->m_queue.empty() || !m_bIsRunning.load();
				});

			if (!m_bIsRunning.load()) break;

			// ジョブをキューから取得して実行
			Job job = std::move(g_jobManager->m_queue.front());
			g_jobManager->m_queue.pop();

			lock.unlock();

			// ジョブを実行
			if (job.func) {
				job.func();
			}
			
			if (job.OnComplete)
			{
				LockMutex lock(g_mainThreadWorker->g_mainThreadTaskMutex);

				MainThreadTask task{};
				task.func = job.OnComplete;

				g_mainThreadWorker->g_mainThreadTasks.push(task);
			}
		}
	}
}
