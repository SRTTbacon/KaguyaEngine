#include "GameManager.h"
#include "GameBase.h"
#include "..\\Scenes\\SceneBase\\SceneBase.h"
#include "..\\Scenes\\SceneTitle\\SceneTitle.h"
#include "..\\Scenes\\SceneInit\\SceneInit.h"
#include "..\\Scenes\\SceneGameover\\SceneGameover.h"
#include "..\\Scenes\\SceneGame\\SceneGame.h"
#include "..\\Scenes\\SceneDebug\\SceneDebug.h"
#include "..\\Scenes\\SceneClear\\SceneClear.h"

#include <GameLoop.h>
#include <exception>
#include <Engine\Core\Exception\DxSystemException.h>

using namespace Common;

GameManager::GameManager()
{
	SceneBase::m_nowSceneID = -1;
	SceneBase::m_nextSceneID = SCENE_INIT;
}

bool GameManager::Run()
{
	try {

		//シーンが変わったら
		if (SceneBase::m_nextSceneID >= 0 && SceneBase::m_nextSceneID < SCENE_MAX && SceneBase::m_nowSceneID != SceneBase::m_nextSceneID) {

			SceneBase::m_nowSceneID = SceneBase::m_nextSceneID;
			SceneBase::m_nextSceneID = -1;

			//SCENE_EXITで終了
			if (!ChangeScene()) {
				return false;
			}

			Game::Run();
		}
	}
	catch (DxSystemException dxSystemExeption)
	{
		dxSystemExeption.ShowOriginalMessage();

		return false;
	}
	catch (std::bad_alloc) {

		DxSystemException(DxSystemException::OM_NEW_ERROR).ShowOriginalMessage();

		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// シーン切り替え
//-----------------------------------------------------------------------------
bool GameManager::ChangeScene()
{
	switch (SceneBase::m_nowSceneID) {

	case SCENE_INIT:
		Game::SetNextScene<SceneInit>();
		break;

	case SCENE_TITLE:
		Game::SetNextScene<SceneTitle>();
		break;

	case SCENE_GAME:
		Game::SetNextScene<SceneGame>();
		break;

	case SCENE_CLEAR:
		Game::SetNextScene<SceneClear>();
		break;

	case SCENE_GAMEOVER:
		Game::SetNextScene<SceneGameover>();
		break;

	case SCENE_DEBUG:
		Game::SetNextScene<SceneDebug>();
		break;

	case SCENE_EXIT:
		return false;
	}

	return true;
}
