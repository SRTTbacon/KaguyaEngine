//*****************************************************************************
//
// エンジンのmain()を呼び出す
//
// Main.cpp
//
// T_Takeuchi
//
// 2026/6/17
//
//*****************************************************************************

#include <EngineCore.h>
#include <GameLoop.h>

#include "GameManager.h"

int main(int argc, wchar_t** argv, wchar_t** envp)
{
	KaguyaEngine::Game::CreateInstance<GameManager>();

	return StartApp();
}
