#pragma once

#include <d3d12video.h>
#include <dxva.h>

#include "..\\CoreMinimal.h"

namespace KaguyaEngine::Core
{
	class VideoAcceration
	{
	public:
		VideoAcceration();
		~VideoAcceration();

		void Initialize(ComPtr<ID3D12Device5> pD3d12Device, ComPtr<ID3D12CommandQueue> pD3d12Queue);

		void BeginVideoCommand();
		void EndVideoCommand();

	public:
		inline ID3D12Device* GetDeviceD3D12() const { return m_pD3D12Device; }

		inline ID3D12VideoDevice* GetVideoDevice() const { return m_pVideoDevice.Get(); }

		inline ID3D12VideoDecodeCommandList* GetVideoCommand() const { return m_pVideoCommand.Get(); }

		inline ID3D12Fence* GetVideoFence() const { return m_pVideoFence.Get(); }

		inline UINT GetFenceValue() const { return m_resetToken; }

	private:
		ComPtr<ID3D12CommandQueue> m_pVideoQueue;
		ComPtr<ID3D12CommandAllocator> m_pVideoCommandAllocator;
		ComPtr<ID3D12VideoDecodeCommandList> m_pVideoCommand;
		ComPtr<ID3D12Fence> m_pVideoFence;

		ID3D12Device5* m_pD3D12Device;

		ComPtr<ID3D12VideoDevice> m_pVideoDevice;

		UINT m_resetToken;

		HANDLE m_fenceEvent = nullptr;				//フェンスで使うイベント

		void CreateCommandObjects();
	};
}
