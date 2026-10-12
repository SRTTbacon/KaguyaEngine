//*****************************************************************************
//
// トリプルバッファリングで使用するフレームリソースの定義
// m_pCurrentBackBufferIndexを参照して、現在のフレームのGPUバッファを取得できるようにしています。
//
// FrameBuffer.h
//
// T_Takeuchi
//
// 2026/5/24
//
//*****************************************************************************

#pragma once

#include "CoreMinimal.h"

#include <d3d12.h>

struct FrameGPUBuffer
{
	ComPtr<ID3D12GraphicsCommandList4> m_pCommandList;
	ComPtr<ID3D12CommandAllocator> m_pAllocator;
	uint64 m_fenceValue;

	FrameGPUBuffer()
		: m_fenceValue(0)
	{
	}
};

namespace KaguyaEngine::Core
{
	struct CopyDescriptor;
}

class FrameResource
{
public:
	FrameResource()
		: m_pDevice(nullptr)
		, m_pFrameGPUBuffers(nullptr)
		, m_pCurrentBackBufferIndex(nullptr)
		, m_pDescriptor(nullptr)
		, m_pFrameTime(nullptr)
	{}
	FrameResource(FrameResource* pOther)
		: m_pDevice(pOther->m_pDevice)
		, m_pFrameGPUBuffers(pOther->m_pFrameGPUBuffers)
		, m_pCurrentBackBufferIndex(pOther->m_pCurrentBackBufferIndex)
		, m_pDescriptor(pOther->m_pDescriptor)
		, m_pFrameTime(pOther->m_pFrameTime)
	{}

	void SetFrameResource(const FrameResource* pFrameResource)
	{ 
		m_pDescriptor = pFrameResource->m_pDescriptor;
		m_pDevice = pFrameResource->m_pDevice; 
		m_pFrameGPUBuffers = pFrameResource->m_pFrameGPUBuffers; 
		m_pCurrentBackBufferIndex = pFrameResource->m_pCurrentBackBufferIndex; 
		m_pFrameTime = pFrameResource->m_pFrameTime;
	}

	void SetFrameResource(KaguyaEngine::Core::CopyDescriptor* pDescriptor, ID3D12Device* pDevice, FrameGPUBuffer* pFrameGPUBuffers, const int* pBackBufferIndex, const float* pFrameTime)
	{
		m_pDescriptor = pDescriptor;
		m_pDevice = pDevice;
		m_pFrameGPUBuffers = pFrameGPUBuffers;
		m_pCurrentBackBufferIndex = pBackBufferIndex;
		m_pFrameTime = pFrameTime;
	}

	inline ID3D12Device* GetDevice() const { return m_pDevice; }
	inline ID3D12GraphicsCommandList4* GetCommandList() const { return m_pFrameGPUBuffers[*m_pCurrentBackBufferIndex].m_pCommandList.Get(); }
	inline KaguyaEngine::Core::CopyDescriptor* GetDescriptor() const { return m_pDescriptor; }

public:
	const int* m_pCurrentBackBufferIndex;
	const float* m_pFrameTime;

protected:
	KaguyaEngine::Core::CopyDescriptor* m_pDescriptor;		// 全テクスチャを収納するディスクリプタヒープ
	ID3D12Device* m_pDevice;				// デバイス
	FrameGPUBuffer* m_pFrameGPUBuffers;		// 各バッファのコマンドリスト
};