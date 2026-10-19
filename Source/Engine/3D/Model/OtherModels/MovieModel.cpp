#include "MovieModel.h"
#include "..\\..\\Material\\Core\\MaterialManager.h"
#include "..\\..\\Material\\OtherMaterials\\MovieMaterial.h"
#include "..\\..\\..\\Core\\EngineDefine.h"
#include "..\\..\\..\\Engine.h"

using namespace DirectX;
using namespace KaguyaEngine::Material;

namespace KaguyaEngine::Model
{
	void MovieModel::LoadMovie(const std::string& moviePath)
	{
		m_movie.Initialize(g_Engine->GetVideoAcceration());
		m_movie.SetFrameResource(this);
		m_movie.LoadMovie(moviePath);

		if (m_pMeshes.size() <= 0)
		{
			m_modelPath = "PrimitiveQuad";
			m_modelName = "PrimitiveQuad";

			CreatePrimitiveQuad();

			ObjectPtr<MovieMaterial> pMovieMaterial = g_Engine->GetMaterialManager()->CreateMaterial<MovieMaterial>();
			pMovieMaterial->SetMaterialName("MovieMaterial");
			pMovieMaterial->Initialize();
			pMovieMaterial->SetNV12Texture(m_movie.GetOutputTexture());
			m_pMeshes[0]->SetMaterial(pMovieMaterial.get());

			m_bLoaded = true;
		}
	}
	void MovieModel::Update()
	{
		m_movie.UpdateVideo();
	}

	void MovieModel::BeforeDrawUpdate()
	{
		m_movie.BeforeDrawUpdate();
	}

	void MovieModel::AfterDrawUpdate()
	{
		m_movie.AfterDrawUpdate();
	}
}