#include "..\..\..\Core\CoreMinimal.h"
#include "..\..\..\Core\FrameBuffer.h"
#include "ModelManager.h"
#include "ModelBase.h"

#include <vector>

namespace KaguyaEngine::Model
{
	ModelManager::ModelManager(FrameResource* pFrameResource)
		: FrameResource(pFrameResource)
	{}

	void ModelManager::RemoveModel(ModelBase* pModel)
	{
		std::erase_if(m_pModels, [pModel](auto& sp) { return sp.get() == pModel; });
	}

	void ModelManager::RemoveModel(ObjectPtr<ModelBase> pModel)
	{
		std::erase(m_pModels, pModel);
	}

	void ModelManager::Update()
	{
		for (ObjectPtr<ModelBase>& pModel : m_pModels) {
			pModel->Update();
		}
	}

	void ModelManager::LateUpdate()
	{
		for (ObjectPtr<ModelBase>& pModel : m_pModels) {
			pModel->LateUpdate();
		}
	}
}