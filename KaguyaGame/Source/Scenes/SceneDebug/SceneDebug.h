#pragma once

#include "..\\SceneBase\\SceneBase.h"

class SceneDebug : public SceneBase
{
public:
	//=============================================================================
	// コンストラクタ
	//=============================================================================
	SceneDebug();

	//=============================================================================
	// デストラクタ
	//=============================================================================
	~SceneDebug();

	//=============================================================================
	// シーンの実行時に１度だけ呼び出される開始処理関数
	//=============================================================================
	virtual void Start() override;

	//=============================================================================
	// シーンの実行時に繰り返し呼び出される更新処理関数
	//=============================================================================
	virtual void Update() override;

	//=============================================================================
	// シーンの実行時に繰り返し呼び出される描画処理関数 (2Dのみ)
	//=============================================================================
	virtual void Draw2D() override;

private:
	ObjectPtr<KaguyaEngine::UI::UITexture> m_pTexture1;
	ObjectPtr<KaguyaEngine::UI::UITexture> m_pTexture2;
	ObjectPtr<KaguyaEngine::UI::UITexture> m_pTexture3;
	ObjectPtr<KaguyaEngine::UI::UITexture> m_pTexture4;

	ObjectPtr<PrimitiveModel> m_pModel1;
	ObjectPtr<PrimitiveModel> m_pModel2;
	ObjectPtr<MovieModel> m_pMovieModel1;

	ObjectPtr<CharacterModel> m_pCharacter1;
};
