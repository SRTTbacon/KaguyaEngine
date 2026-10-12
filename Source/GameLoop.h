//*****************************************************************************
//
// シーンの基底クラス
// ゲームプロジェクトのSceneBaseと紐づける
//
// GameLoop.h
//
// T_Takeuchi
//
// 2026/6/17
//
//*****************************************************************************

#pragma once

#include "Engine\\Core\\CoreMinimal.h"

#include <concepts>

namespace KaguyaEngine
{
	// -----------------------------GameLoop-----------------------------

	class GameLoop
	{
	public:
		virtual ~GameLoop() = default;

		virtual void Start() = 0;
		virtual void Update() = 0;
		virtual void Draw2D() = 0;

		void ChangeScene(int sceneID);

	public:
		static int m_nowSceneID;
		static int m_nextSceneID;
	};

	// -----------------------------Game-----------------------------

	class Game
	{
	public:
		template<typename T>
		requires std::derived_from<T, Game>
		static void CreateInstance();
		static Game* GetInstance() { return g_pGameInstance.get(); }
		static void ClearInstance() { g_pGameInstance = nullptr; }

	public:
		Game() {}

		virtual void UpdateScene();
		virtual void Draw2D();

		//=============================================================================
		// フレーム開始
		//=============================================================================
		virtual bool Run();

	protected:
		template<typename T>
		requires std::derived_from<T, GameLoop>
		void SetNextScene()
		{
			m_pNextScene = CreateUniquePtr<T>();
		}

	private:
		static UniquePtr<Game> g_pGameInstance;

		UniquePtr<GameLoop> m_pNowScene;
		UniquePtr<GameLoop> m_pNextScene;

		//コピー防止
		Game(const Game&) = delete;
		Game& operator=(const Game&) = delete;
	};

	template<typename T>
	requires std::derived_from<T, Game>
	inline void Game::CreateInstance()
	{
		g_pGameInstance = CreateUniquePtr<T>();
	}
}
