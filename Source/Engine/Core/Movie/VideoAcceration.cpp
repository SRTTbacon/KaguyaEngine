#include "VideoAcceration.h"

#include <assert.h>
#include <vector>

namespace KaguyaEngine::Core
{
	VideoAcceration::VideoAcceration()
		: m_resetToken(0)
		, m_pD3D12Device(nullptr)
	{}

	VideoAcceration::~VideoAcceration()
	{
		if (m_pVideoQueue.Get()) {
			CloseHandle(m_fenceEvent);
		}
	}

	void VideoAcceration::Initialize(ComPtr<ID3D12Device5> pD3d12Device, ComPtr<ID3D12CommandQueue> pD3d12Queue)
	{
		HRESULT hr;

		m_pD3D12Device = pD3d12Device.Get();

		if (SUCCEEDED(pD3d12Device.As(&m_pVideoDevice)))
		{
			D3D12_FEATURE_DATA_VIDEO_DECODE_PROFILE_COUNT video_decode_profile_count = {};
			hr = m_pVideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_PROFILE_COUNT, &video_decode_profile_count, sizeof(video_decode_profile_count));
			assert(SUCCEEDED(hr));
			std::vector<GUID> video_decode_profile_list;
			video_decode_profile_list.resize(video_decode_profile_count.ProfileCount);
			D3D12_FEATURE_DATA_VIDEO_DECODE_PROFILES video_decode_profiles = {};
			video_decode_profiles.ProfileCount = video_decode_profile_count.ProfileCount;
			video_decode_profiles.pProfiles = video_decode_profile_list.data();
			hr = m_pVideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_PROFILES, &video_decode_profiles, sizeof(video_decode_profiles));
			assert(SUCCEEDED(hr));
			bool h264_supported = false;
			for (GUID& profile : video_decode_profile_list)
			{
				if (profile == D3D12_VIDEO_DECODE_PROFILE_H264)
				{
					h264_supported = true;
					break;
				}
			}
			if (!h264_supported)
			{
				printf("GPUがH.264のデコードに対応していません。\n");
				return;
			}

			CreateCommandObjects();
		}
	}

	void VideoAcceration::BeginVideoCommand()
	{
		m_pVideoCommandAllocator->Reset();
		m_pVideoCommand->Reset(m_pVideoCommandAllocator.Get());
	}

	void VideoAcceration::EndVideoCommand()
	{
		HRESULT hr = m_pVideoCommand->Close();
		assert(SUCCEEDED(hr));
		ID3D12CommandList* ppCommandLists[] = { m_pVideoCommand.Get() };
		m_pVideoQueue->ExecuteCommandLists(1, ppCommandLists);
		const UINT64 fence = ++m_resetToken;
		m_pVideoQueue->Signal(m_pVideoFence.Get(), fence);

		//GPU完了を待ってからReset
		if (m_pVideoFence->GetCompletedValue() < fence) {
			m_pVideoFence->SetEventOnCompletion(fence, m_fenceEvent);
			WaitForSingleObject(m_fenceEvent, INFINITE);
		}
	}

	void VideoAcceration::CreateCommandObjects()
	{
		HRESULT hr;

		D3D12_COMMAND_QUEUE_DESC queue_desc = {};
		queue_desc.Type = D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE;
		hr = m_pD3D12Device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_pVideoQueue));
		m_pVideoQueue->SetName(L"TestQueue");
		assert(SUCCEEDED(hr));
		hr = m_pD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE, IID_PPV_ARGS(&m_pVideoCommandAllocator));
		m_pVideoCommandAllocator->SetName(L"TestAllocator");
		assert(SUCCEEDED(hr));
		hr = m_pD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE, m_pVideoCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_pVideoCommand));
		m_pVideoCommand->SetName(L"TestVideoCommand");
		assert(SUCCEEDED(hr));
		m_pD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pVideoFence));
		assert(SUCCEEDED(hr));

		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		m_pVideoCommand->Close();
	}
}
