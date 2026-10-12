//*****************************************************************************
//
// ゲームシーンを管理するクラス
//
// GameManager.h
//
// T_Takeuchi
//
// 2026/6/18
//
//*****************************************************************************

#pragma once

#include <GameLoop.h>

class GameManager : public KaguyaEngine::Game
{
public:
	GameManager();

	virtual bool Run() override;

private:
	bool ChangeScene();
};
