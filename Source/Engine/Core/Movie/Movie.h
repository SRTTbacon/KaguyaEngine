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
#include "VideoAcceration.h"

constexpr const DXGI_FORMAT DECODE_FORMAT = DXGI_FORMAT_NV12;

class Movie : public FrameResource
{
public:
	Movie();
	~Movie();

	void Initialize(VideoAcceration* pVideoAcceration);
	void LoadMovie(std::string movieFilePath);

	void UpdateVideo();

protected:
	void LoadedModel();
	void BeforeDrawUpdate();
	void AfterDrawUpdate();

private:
	struct DecodeResultReordered
	{
		ComPtr<ID3D12Resource> texture;
		int display_order = -1;
		int frame_index = 0;
		void Create(ID3D12Device* device, uint32_t padded_width, uint32_t padded_height)
		{
			D3D12_RESOURCE_DESC desc = {};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			desc.Format = DECODE_FORMAT;
			desc.Width = padded_width;
			desc.Height = padded_height;
			desc.DepthOrArraySize = 1;
			desc.SampleDesc.Count = 1;
			desc.MipLevels = 1;
			D3D12_HEAP_PROPERTIES heap_properties = {};
			heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
			HRESULT hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(texture.ReleaseAndGetAddressOf()));
			assert(SUCCEEDED(hr));
			hr = texture->SetName(L"DecodeResultReordered::Texture");
			assert(SUCCEEDED(hr));
		}
	};

private:
	static UINT MovieMeshCount;

	ComPtr<ID3D12Resource> m_bitStreamBuffer;
	ComPtr<ID3D12Resource> m_dpbTexture;
	ComPtr<ID3D12Resource> m_dpbOutputTexture;

	VideoAcceration* m_pVideoAcceration;
	Video m_video;

	ComPtr<ID3D12VideoDecoder> m_pDecoder;
	ComPtr<ID3D12VideoDecoderHeap> m_pDecoderHeap;

	D3D12_RESOURCE_STATES dpb_layouts[17] = {};

	std::vector<DecodeResultReordered> reordered_results_free; // free image slots that can be reused as reordering image displaying
	std::vector<DecodeResultReordered> reordered_results_working; // image slots that are currently in use for reordering
	DecodeResultReordered m_decodeResults;

	UINT m_width;
	UINT m_height;
	UINT m_movieIndex;
	UINT m_beforeFrameIndex;

	bool m_bReferenceOnlyAllocation;

	void DecodeFrame();

	void Release();

	void CreateDecoder();
	void CreateBitStream();
	void CreateDPBTexture();
};
