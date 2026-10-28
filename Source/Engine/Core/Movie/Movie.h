//*****************************************************************************
//
// 動画(mp4想定)ファイルをデコードし、指定したリソースに書き込むクラス
// https://github.com/turanszkij/mini_video
//
// Movie.h
//
// T_Takeuchi
//
// 2026/10/12
//
//*****************************************************************************

#pragma once

#include "..\\CoreMinimal.h"
#include "..\\FrameBuffer.h"
#include "..\\..\\..\\Library\\mp4\\common.h"

#include <d3d12video.h>
#include <dxva.h>

namespace KaguyaEngine::Core
{
	constexpr const DXGI_FORMAT DECODE_FORMAT = DXGI_FORMAT_NV12;

	class VideoAcceration;

	class Movie : public FrameResource
	{
	public:
		Movie();
		~Movie();

		void Initialize(VideoAcceration* pVideoAcceration);
		void LoadMovie(const std::string& movieFilePath);

		void UpdateVideo();

		void BeforeDrawUpdate(uint32 srvIndex);
		void AfterDrawUpdate();

	public:
		inline ID3D12Resource* GetOutputTexture() const { return m_pOutputTexture.Get(); };

	public:
		float m_time;
		float m_speed;
		bool m_bPaused;
		bool m_bLoop;

	private:
		struct DecodeResultReordered
		{
			ComPtr<ID3D12Resource> texture;
			int display_order = -1;
			int frame_index = 0;
			void create(ID3D12Device* device, uint32_t padded_width, uint32_t padded_height)
			{
				D3D12_RESOURCE_DESC desc = {};
				desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				desc.Format = DXGI_FORMAT_NV12;
				desc.Width = padded_width;
				desc.Height = padded_height;
				desc.DepthOrArraySize = 1;
				desc.SampleDesc.Count = 1;
				desc.MipLevels = 1;
				D3D12_HEAP_PROPERTIES heap_properties = {};
				heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
				HRESULT hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&texture));
				assert(SUCCEEDED(hr));
				hr = texture->SetName(L"DecodeResultReordered::texture");
				assert(SUCCEEDED(hr));
			}
		};

	private:
		static uint32 MovieMeshCount;

		ComPtr<ID3D12Resource> m_bitStreamBuffer;
		ComPtr<ID3D12Resource> m_dpbTexture;
		ComPtr<ID3D12Resource> m_dpbOutputTexture;

		VideoAcceration* m_pVideoAcceration;
		Video m_video;

		ComPtr<ID3D12VideoDecoder> m_pDecoder;
		ComPtr<ID3D12VideoDecoderHeap> m_pDecoderHeap;

		ComPtr<ID3D12Resource> m_pOutputTexture;

		std::vector<DecodeResultReordered> m_reorderedResultsFree; // free image slots that can be reused as reordering image displaying
		std::vector<DecodeResultReordered> m_reorderedResultsWorking; // image slots that are currently in use for reordering
		DecodeResultReordered m_displayedImage; // the latest reordered picture

		D3D12_RESOURCE_STATES m_dpbLayouts[17];

		uint32 m_width;
		uint32 m_height;
		uint32 m_movieIndex;
		uint32 m_beforeFrameIndex;

		bool m_bLoaded;
		bool m_bReferenceOnlyAllocation;
		bool m_bMustDecode;

		void DecodeFrame();

		void Release();

		void CreateDecoder();
		void CreateBitStream();
		void CreateDPBTexture();
		void CreateOutputTexture();
	};
}
