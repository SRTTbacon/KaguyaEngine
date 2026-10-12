//*****************************************************************************
//
// エンジンの機能をすべて包括したヘッダ
// ゲーム側は基本的にこのヘッダをincludeする
//
// EngineCore.h
//
// T_Takeuchi
//
// 2026/5/25
//
//*****************************************************************************

#pragma once

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

#ifdef _DEBUG
#pragma comment(lib, "assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "assimp-vc143-mt.lib")
#endif

#include "Engine\\Engine.h"
#include "Engine\\Core\\Main\\Main.h"
#include "Engine\\Core\\CoreMinimal.h"
#include "Engine\\Core\\EngineDefine.h"
#include "Engine\\Core\\FrameBuffer.h"
#include "Engine\\Core\\Input\\Input.h"
#include "Engine\\Core\\Texture2D\\Texture2D.h"
#include "Engine\\UI\\UIManager.h"
#include "Engine\\3D\\PostProcess\\Core\\PostProcessManager.h"
#include "Engine\\3D\\Material\\PrimitiveMaterial.h"
#include "Engine\\3D\\Material\\BoneMaterial.h"
#include "Engine\\3D\\Material\\Core\\MaterialManager.h"
#include "Engine\\3D\\Model\\Core\\ModelManager.h"
#include "Engine\\3D\\Model\\PrimitiveModel.h"
#include "Engine\\3D\\Model\\CharacterModel.h"
#include "Engine\\3D\\Lights\\DirectionalLight.h"
#include "Engine\\3D\\Skybox\\SkyBox.h"

#include "GameLoop.h"

#include "Engine\\Core\\Exception\\DxSystemException.h"

using namespace KaguyaEngine;
using namespace KaguyaEngine::UI;
using namespace KaguyaEngine::Model;
using namespace KaguyaEngine::Material;
using namespace KaguyaEngine::PostProcess;

using namespace DirectX;
