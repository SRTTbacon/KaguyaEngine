#include "ResourceStatics.h"
#include "..\\CoreMinimal.h"

#include <d3dx12.h>
#include <d3d12.h>
#include <cstdint>
#include <Windows.h>
#include <cstdio>
#include <dxgiformat.h>

ID3D12Device* ResourceStatics::m_pDevice = nullptr;

void ResourceStatics::Initialize(ID3D12Device* pDevice)
{
	m_pDevice = pDevice;
}

ID3D12Resource* ResourceStatics::CreateUploadResource(uint64 bufferSize)
{
    // アップロード用の一時バッファを作成
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(GetAlignedSize(bufferSize));

    ID3D12Resource* pBuffer;

    HRESULT hr = m_pDevice->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pBuffer)
    );
    if (FAILED(hr)) {
        printf("UploadBufferの作成に失敗しました。\n");
        return nullptr;
    }
	pBuffer->SetName(L"ResourceStatics::CreateUploadResource");

    return pBuffer;
}

bool ResourceStatics::CreateUploadResource(uint64 bufferSize, ComPtr<ID3D12Resource>& outResource)
{
	// アップロード用の一時バッファを作成
	CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(GetAlignedSize(bufferSize));

	HRESULT hr = m_pDevice->CreateCommittedResource(
		&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(outResource.ReleaseAndGetAddressOf())
	);
	if (FAILED(hr)) {
		printf("UploadBufferの作成に失敗しました。\n");
		return false;
	}
	outResource->SetName(L"ResourceStatics::CreateUploadResource");

	return true;
}

ID3D12Resource* ResourceStatics::CreateResource(DXGI_FORMAT format, D3D12_HEAP_TYPE heapType, size_t width, size_t height)
{
	CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, static_cast<uint32>(width), static_cast<uint32>(height), 1, 1);
	CD3DX12_HEAP_PROPERTIES texHeapProp = CD3DX12_HEAP_PROPERTIES(heapType);
	ID3D12Resource* buff = nullptr;
	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
	HRESULT result = m_pDevice->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		state,
		nullptr,
		IID_PPV_ARGS(&buff)
	);
	if (FAILED(result))
	{
		printf("ResourceStatics::CreateResource : エラーコード = %1x\n", result);
		return nullptr;
	}
	buff->SetName(L"ResourceStatics::CreateResource");

	return buff;
}

bool ResourceStatics::CreateResource(DXGI_FORMAT format, D3D12_HEAP_TYPE heapType, size_t width, size_t height, ComPtr<ID3D12Resource>& outResource)
{
	CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, static_cast<uint32>(width), static_cast<uint32>(height), 1, 1);
	CD3DX12_HEAP_PROPERTIES texHeapProp = CD3DX12_HEAP_PROPERTIES(heapType);
	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
	HRESULT result = m_pDevice->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		state,
		nullptr,
		IID_PPV_ARGS(&outResource)
	);
	if (FAILED(result))
	{
		printf("ResourceStatics::CreateResource : エラーコード = %1x\n", result);
		return false;
	}
	outResource->SetName(L"ResourceStatics::CreateResource");

	return true;
}

bool ResourceStatics::CreateResource(D3D12_HEAP_TYPE heapType, uint64 bufferSize, ComPtr<ID3D12Resource>& outResource, D3D12_RESOURCE_STATES state)
{
	CD3DX12_HEAP_PROPERTIES heapProps(heapType);
	CD3DX12_RESOURCE_DESC contentsBuffer = CD3DX12_RESOURCE_DESC::Buffer(GetAlignedSize(bufferSize));
	if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
		state = D3D12_RESOURCE_STATE_GENERIC_READ;
	}
	HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &contentsBuffer, state, nullptr, IID_PPV_ARGS(outResource.ReleaseAndGetAddressOf()));
	if (FAILED(hr)) {
		printf("ResourceStatics::CreateResource : エラーコード = %1x\n", hr);
		return false;
	}
	outResource->SetName(L"ResourceStatics::CreateResource");
	return true;
}
