//*****************************************************************************
//
// ディスクリプタヒープの作成および管理
// 登録されたヒープ番号は各自のクラスで管理する必要があります
//
// DescriptorHeap.h
//
// T_Takeuchi
//
// 2026/5/26
//
//*****************************************************************************

#pragma once

#include "..\\CoreMinimal.h"

#include <d3d12.h>
#include <dxgiformat.h>
#include <vector>
#include <cstdint>

constexpr uint32 DEFAULT_HEAP_SIZE = 10000;

namespace KaguyaEngine::Core
{
	class DescriptorHeap;

	// シェーダーに送る際に連番で送信するための一時的なヒープを保持する構造体
	// 基本的にはエンジンが管理するための構造体で、ユーザーが直接触ることは想定していません。
	struct CopyDescriptor
	{
		UniquePtr<DescriptorHeap> pMainHeap;		// ずっと保持し続けるヒープへのポインタ
		UniquePtr<DescriptorHeap> pCopyHeap;		// 毎フレームリセットされるヒープへのポインタ
	};

	class DescriptorHeap
	{
	public:
		DescriptorHeap();
		~DescriptorHeap();

		// ディスクリプタヒープの初期化
		// pDevice: デバイス, heapSize: ヒープのサイズ（デフォルトは10000）
		void Initialize(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_FLAGS flag, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32 heapSize = DEFAULT_HEAP_SIZE);

		uint32 SetResource(ID3D12Resource* pResource, uint32 offset = UINT32_MAX);
		uint32 SetUAVResource(ID3D12Resource* pResource, uint32 offset = UINT32_MAX);
		uint32 SetResource(ID3D12Resource* pResource, DXGI_FORMAT format, uint32 offset = UINT32_MAX);
		uint32 SetUAVResource(ID3D12Resource* pResource, DXGI_FORMAT format, uint32 offset = UINT32_MAX);
		uint32 SetResource(ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC& shaderResource, uint32 offset = UINT32_MAX);
		uint32 SetUAVResource(ID3D12Resource* pResource, D3D12_UNORDERED_ACCESS_VIEW_DESC& shaderResource, uint32 offset = UINT32_MAX);
		uint32 SetMainTexture(ID3D12Resource* mainTex, ID3D12Resource* normalMap);
		uint32 SetUserTexture(ID3D12Resource* tex, int position, D3D12_SRV_DIMENSION dimension);

		void ClearDescriptor(uint32 offset);
		void ClearAllDescriptors();

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuDescriptorHandle(uint32 offset);
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptorHandle(uint32 offset);
		ID3D12DescriptorHeap* GetHeap() const;

		// 使用可能なディスクリプタのインデックスを取得
		uint32 GetNextAvailableDescriptor(bool bLoop = true);
		// 連続して使用可能なディスクリプタのインデックスを取得
		uint32 GetNextAvailableDescriptor(uint32 continueCount, uint32 startIndex = UINT32_MAX, bool bLoop = true);

		inline uint32 GetHeapSize() const { return m_descriptorSize; }
		inline D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() const { return m_heapType; }

	private:
		ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
		ID3D12Device* m_pDevice;
		uint32 m_descriptorSize;

		uint32 m_nowDescriptorCount;            // 1度でも初期化されたディスクリプタの数 (GetNextAvailableDescriptor()の検索用)

		D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;

		std::vector<bool> m_descriptorUsage;    // ディスクリプタの使用状況を管理するベクター

		// シェーダーリソースビューを作成するヘルパー関数
		void CreateShaderResourceView(ID3D12Resource* pResource, D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle);
	};
}