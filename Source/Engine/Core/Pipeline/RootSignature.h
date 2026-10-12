//*****************************************************************************
//
// ルートシグネチャの作成/管理
//
// RootSignature.h
//
// T_Takeuchi
//
// 2026/5/28
//
//*****************************************************************************

#pragma once

#include "..\\CoreMinimal.h"

#include <d3d12.h>

namespace KaguyaEngine::Core
{
	class RootSignature
	{
	public:
		RootSignature() = default;
		~RootSignature() = default;

		static ID3D12RootSignature* CreateRootSignature(ID3D12Device* pDevice, uint64 shaderHash, D3D12_ROOT_PARAMETER* pRootParameters, uint32 numRootParameters, D3D12_STATIC_SAMPLER_DESC* pSamplers, uint32 numSamplers);

		void Initialize(ID3D12Device* pDevice, uint64 shaderHash, D3D12_ROOT_PARAMETER* pRootParameters, uint32 numRootParameters, D3D12_STATIC_SAMPLER_DESC* pSamplers, uint32 numSamplers);

		// ルートシグネチャの取得
		ID3D12RootSignature* GetRootSignature() const { return m_pRootSignature; }
		uint64 GetShaderHash() const { return m_shaderHash; }
	private:
		ID3D12RootSignature* m_pRootSignature; // ルートシグネチャのCOMポインタ
		uint64 m_shaderHash;
	};
}
