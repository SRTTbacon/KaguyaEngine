#include "MainThreadWorker.h"
#include "..\CoreMinimal.h"

#include <mutex>
#include <utility>

namespace KaguyaEngine::AsyncTask
{
	UniquePtr<MainThreadWorker> g_mainThreadWorker;

	void MainThreadWorker::CreateInstance()
	{
		if (!g_mainThreadWorker)
			g_mainThreadWorker = CreateUniquePtr<MainThreadWorker>();
	}

	void MainThreadWorker::ExecuteMainThreadTasks()
	{
		std::lock_guard lock(g_mainThreadTaskMutex);
		while (!g_mainThreadTasks.empty())
		{
			MainThreadTask task = std::move(g_mainThreadTasks.front());
			g_mainThreadTasks.pop();
			task.func();
		}
	}
}
