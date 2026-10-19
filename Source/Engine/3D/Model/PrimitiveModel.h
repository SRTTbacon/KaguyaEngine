//*****************************************************************************
//
// FBXモデルを読み込むクラス
// 基本的には非同期で読み込まれます。IsLoaded()関数、コールバック等で読み込みが完了しているか確認してください。
// 
// FBXModel.h
//
// T_Takeuchi
//
// 2026/6/7
//
//*****************************************************************************

#pragma once

#include "Core\\ModelBase.h"
#include "Core\\Mesh.h"
#include "..\\..\\Core\\CoreMinimal.h"

#include <vector>
#include <string>
#include <assimp\\scene.h>
#include <d3d12.h>
#include <DirectXMath.h>

namespace KaguyaEngine::Model
{
	// -------------------------------FBXModel------------------------------

	// スマートポインタで管理されるFBXモデルクラス
	// ※enable_shared_from_thisを継承しているため、ポインタ経由でのみアクセス可能
	class PrimitiveModel : public ModelBase
	{
	public:
		// ファイルからFBXモデルを読み込む
		void LoadModel(const std::string& modelPath);
		// modelData : FBXモデルのファイルデータ (バイナリ形式)
		// format : FBXモデルのファイル形式 (例: ".fbx", ".obj"など)
		void LoadModel(const std::vector<char>& modelData, const std::string& format);

		// ModelManagerから呼び出される
		virtual void LateUpdate() override;

	private:
		void ProcessNode(const aiScene* pScene, aiNode* pNode);
	};
}