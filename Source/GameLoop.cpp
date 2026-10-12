#include "GameLoop.h"
#include "Engine\Core\CoreMinimal.h"

#include <utility>

namespace KaguyaEngine
{
	//シーン構造体
	int GameLoop::m_nowSceneID;
	int GameLoop::m_nextSceneID;

	UniquePtr<Game> Game::g_pGameInstance;

	void GameLoop::ChangeScene(int sceneID)
	{
		m_nextSceneID = sceneID;
	}

	void Game::UpdateScene()
	{
		if (m_pNowScene) {
			m_pNowScene->Update();
		}
	}

	void Game::Draw2D()
	{
		if (m_pNowScene) {
			m_pNowScene->Draw2D();
		}
	}

	bool Game::Run()
	{
		if (m_pNextScene)
		{
			m_pNowScene = std::move(m_pNextScene);
			m_pNextScene = nullptr;
			m_pNowScene->Start();
		}
		return true;
	}
}