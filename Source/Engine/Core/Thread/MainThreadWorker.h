//*****************************************************************************
//
// 非同期処理で行った処理の結果をメインスレッドで受け取るためのクラス
// このクラスはエンジンのコア部分で使用されるため、ユーザーから直接アクセスされないことを想定しています。
//
// MainThreadWorker.h
//
// T_Takeuchi
//
// 2026/5/25
//
//*****************************************************************************

#pragma once

#include <functional>
#include <queue>
#include <mutex>
#include "..\CoreMinimal.h"

namespace KaguyaEngine::AsyncTask
{
	struct MainThreadTask
	{
		std::function<void()> func;
	};

	class MainThreadWorker
	{
	public:
		static void CreateInstance();

		// メインスレッドで呼び出される関数。非同期処理の結果を受け取るために使用される。
		void ExecuteMainThreadTasks();

	public:
		std::queue<MainThreadTask> g_mainThreadTasks;
		std::mutex g_mainThreadTaskMutex;
	};

	extern UniquePtr<MainThreadWorker> g_mainThreadWorker;
}