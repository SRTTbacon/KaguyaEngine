//*****************************************************************************
//
// デバイスを用いた静的リソースを作成するクラス
//
// ResourceStatics.h
//
// T_Takeuchi
//
// 2026/5/24
//
//*****************************************************************************

#pragma once

#include "..\\CoreMinimal.h"

#include <d3d12.h>
#include <cstdint>
#include <dxgiformat.h>

class ResourceStatics
{
public:
	static void Initialize(ID3D12Device* pDevice);

	// アップロードバッファを作成する関数
	// 生ポインタで生成するため、呼び出し側でReleaseする必要がある (生ポインタで作成するため、ComPtrに入れても自動解放されません。必ずRelease()すること)
	static ID3D12Resource* CreateUploadResource(uint64 bufferSize);
	// アップロードバッファを作成する関数
	static bool CreateUploadResource(uint64 bufferSize, ComPtr<ID3D12Resource>& outResource);

	static ID3D12Resource* CreateResource(DXGI_FORMAT format, D3D12_HEAP_TYPE heapType, size_t width, size_t height);
	static bool CreateResource(DXGI_FORMAT format, D3D12_HEAP_TYPE heapType, size_t width, size_t height, ComPtr<ID3D12Resource>& outResource);
	static bool CreateResource(D3D12_HEAP_TYPE heapType, uint64 bufferSize, ComPtr<ID3D12Resource>& outResource, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);

private:
	static ID3D12Device* m_pDevice;
};
