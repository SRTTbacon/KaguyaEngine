//*****************************************************************************
//
// 動画(mp4想定)ファイルをデコードし、指定したリソースに書き込むクラス
//
// Movie.h
//
// T_Takeuchi
//
// 2026/10/12
//
//*****************************************************************************

#pragma once

#include "..\\..\\..\\Library\\mp4\\common.h"
#include "..\\CoreMinimal.h"
#include "..\\FrameBuffer.h"

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

		void BeforeDrawUpdate();
		void AfterDrawUpdate();

	public:
		inline ID3D12Resource* GetOutputTexture() const { return m_pOutputTexture.Get(); };

	public:
		float m_time;
		float m_speed;
		bool m_bPaused;
		bool m_bLoop;

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

		D3D12_RESOURCE_STATES dpb_layouts[17] = {};

		uint32 m_width;
		uint32 m_height;
		uint32 m_movieIndex;
		uint32 m_beforeFrameIndex;

		bool m_bLoaded;
		bool m_bReferenceOnlyAllocation;

		void DecodeFrame();

		void Release();

		void CreateDecoder();
		void CreateBitStream();
		void CreateDPBTexture();
		void CreateOutputTexture();
	};
}
