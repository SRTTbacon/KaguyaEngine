//*****************************************************************************
//
// 3Dモデルを作成、管理を行うクラス
//
// ModelManager.h
//
// T_Takeuchi
//
// 2026/6/20
//
//*****************************************************************************

#pragma once

#include "ModelBase.h"
#include "..\\..\\..\\Core\\CoreMinimal.h"
#include "..\\\..\\..\\Core\\FrameBuffer.h"

#include <vector>
#include <utility>
#include <concepts>

namespace KaguyaEngine::Model
{
	class ModelManager : public FrameResource
	{
	public:
		ModelManager(FrameResource* pFrameResource);

		template<typename T, typename... Args>
		requires std::derived_from<T, ModelBase>
		ObjectPtr<T> CreateModel(Args&&... args);

		void RemoveModel(ModelBase* pModel);
		void RemoveModel(ObjectPtr<ModelBase> pModel);

		void Update();
		void LateUpdate();

	private:
		std::vector<ObjectPtr<ModelBase>> m_pModels;
	};


	template<typename T, typename ...Args>
	requires std::derived_from<T, ModelBase>
	inline ObjectPtr<T> ModelManager::CreateModel(Args&& ...args)
	{
		ObjectPtr<T> pModel = CreateObjectPtr<T>(std::forward<Args>(args)...);
		pModel->SetFrameResource(this);

		m_pModels.push_back(pModel);

		return pModel;
	}
}
