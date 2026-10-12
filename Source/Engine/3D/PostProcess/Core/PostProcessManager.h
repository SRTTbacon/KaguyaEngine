#pragma once

#include "PostProcessBase.h"
#include "..\\..\\..\\Core\\EngineDefine.h"

#include <DirectXMath.h>
#include <algorithm>

namespace KaguyaEngine
{
	class Camera;
	namespace Core
	{
		class DescriptorHeap;
	}
}

namespace KaguyaEngine::PostProcess
{
	class PostProcessManager : public FrameResource
	{
	public:
		PostProcessManager(const KaguyaEngine::Camera* pCamera, KaguyaEngine::Core::DescriptorHeap* pRtvHeap);
		~PostProcessManager();

		void Initialize();

		// 指定したポストプロセスを削除
		void RemovePostProcess(void* p);
		// 全てのポストプロセスを削除
		void RemoveAllPostProcess();

		// カメラのレンダーターゲットのサイズが変更された場合に呼び出す
		void ChangeRenderSize();

		const RenderResource& Render(const RenderResource& renderResource);

	public:
		inline RenderResource* GetLastRender() { return &m_lastRenderResource; }

		inline uint32 GetPostProcessCount() const { return static_cast<uint32>(m_processes.size()); }
		uint32 GetEnableCount();

	public:
		template<typename T, typename... Args>
		requires std::derived_from<T, PostProcessBase>
		std::shared_ptr<T> AddPostProcess(BYTE depth)
		{
			std::shared_ptr<T> process = CreateObjectPtr<T>(GetNextPostProcessID(), m_pCamera, m_pRtvHeap, m_pBaseRootSignature);
			process->SetFrameResource(this);
			process->Initialize(true);
			process->m_depth = depth;

			//重複しないようにm_depthを調整
			bool unique;
			do {
				unique = true;
				for (const auto& p : m_processes) {
					if (p->m_depth == process->m_depth) {
						process->m_depth++;
						unique = false;
						break;
					}
				}
			} while (!unique);

			m_processes.push_back(process);

			std::sort(m_processes.begin(), m_processes.end(),
				[](const std::shared_ptr<PostProcessBase>& a, const std::shared_ptr<PostProcessBase>& b) {
					return a->m_depth < b->m_depth;
				});

			return process;
		}

	private:
		static const uint32 MAX_POSTPROCESS_COUNT;

		const KaguyaEngine::Camera* m_pCamera;

		KaguyaEngine::Core::DescriptorHeap* m_pRtvHeap;
		ID3D12RootSignature* m_pBaseRootSignature;

		RenderResource m_lastRenderResource;

		std::vector<std::shared_ptr<PostProcessBase>> m_processes;

		std::vector<bool> m_bFreePostProcessIDs;	//ポストプロセスIDが使用可能かどうか (trueの場合は使用していない)
		uint32 m_nextPostProcessID;					//次に格納する際に使用するID

		uint32 GetNextPostProcessID();
	};
}
