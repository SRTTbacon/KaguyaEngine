//*****************************************************************************
//
// mp4ファイルをデコードし、板ポリゴンに描画するモデルクラス
//
// MovieModel.h
//
// T_Takeuchi
//
// 2026/10/14
//
//*****************************************************************************

#pragma once

#include "..\\Core\\ModelBase.h"
#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\..\\Core\\Movie\\Movie.h"

namespace KaguyaEngine::Core
{
	class VideoAcceration;
}

namespace KaguyaEngine::Model
{
	// スマートポインタで管理されるFBXモデルクラス
	// ※enable_shared_from_thisを継承しているため、ポインタ経由でのみアクセス可能
	class MovieModel : public ModelBase
	{
	public:
		void LoadMovie(const std::string& moviePath);

		virtual void Update() override;

		virtual void BeforeDrawUpdate() override;
		virtual void AfterDrawUpdate() override;

	protected:
		ComPtr<ID3D12Resource> m_pNV12Texture;

		KaguyaEngine::Core::Movie m_movie;
	};
}
