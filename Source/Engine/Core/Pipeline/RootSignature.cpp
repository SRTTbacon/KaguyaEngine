#include "RootSignature.h"
#include "..\\CoreMinimal.h"
#include "PipelineDefine.h"

#include <d3d12.h>
#include <Windows.h>
#include <cstdio>
#include <d3dcommon.h>

namespace KaguyaEngine::Core
{
	ID3D12RootSignature* RootSignature::CreateRootSignature(ID3D12Device* pDevice, uint64 shaderHash, D3D12_ROOT_PARAMETER* pRootParameters, uint32 numRootParameters, D3D12_STATIC_SAMPLER_DESC* pSamplers, uint32 numSamplers)
	{
		RootSignature rootSignature{};
		rootSignature.Initialize(pDevice, shaderHash, pRootParameters, numRootParameters, pSamplers, numSamplers);
		return rootSignature.GetRootSignature();
	}
	void RootSignature::Initialize(ID3D12Device* pDevice, uint64 shaderHash, D3D12_ROOT_PARAMETER* pRootParameters, uint32 numRootParameters, D3D12_STATIC_SAMPLER_DESC* pSamplers, uint32 numSamplers)
	{
		// g_rootSignatureCacheMutex は全アクセス箇所で共有する
		std::lock_guard<std::mutex> lock(g_rootSignatureCacheMutex);

		if (g_rootSignatureCache.find(shaderHash) != g_rootSignatureCache.end())
		{
			m_pRootSignature = g_rootSignatureCache[shaderHash].Get();
			m_shaderHash = shaderHash;
			return;
		}

		D3D12_ROOT_SIGNATURE_FLAGS flag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;	// アプリケーションの入力アセンブラを使用する
		flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;								// ジオメトリシェーダーのルートシグネチャへのアクセスを拒否する

		// ルートシグネチャの設定（ルートパラメーターとスタティックサンプラーを入れる）
		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = numRootParameters;		// ルートパラメーターの個数
		desc.NumStaticSamplers = numSamplers;		// サンプラーの個数
		desc.pParameters = pRootParameters;			// ルートパラメーターのポインタ
		desc.pStaticSamplers = pSamplers;			// サンプラーのポインタ
		desc.Flags = flag;							// フラグを設定

		ComPtr<ID3DBlob> pBlob;
		ComPtr<ID3DBlob> pErrorBlob;

		// シリアライズ
		HRESULT hr = D3D12SerializeRootSignature(
			&desc,
			D3D_ROOT_SIGNATURE_VERSION_1_0,
			pBlob.GetAddressOf(),
			pErrorBlob.GetAddressOf());
		if (FAILED(hr))
		{
			if (pErrorBlob) {
				printf("ルートシグネチャシリアライズに失敗 - %s\n", (char*)pErrorBlob->GetBufferPointer());
			}
			else {
				printf("ルートシグネチャシリアライズに失敗\n");
			}
			return;
		}

		g_rootSignatureCache.try_emplace(shaderHash);

		ComPtr<ID3D12RootSignature> rootSignature;

		// ルートシグネチャ生成
		hr = pDevice->CreateRootSignature(
			0,										// GPUが複数ある場合のノードマスク（基本1つのため0を指定）
			pBlob->GetBufferPointer(),				// シリアライズしたデータのポインタ
			pBlob->GetBufferSize(),					// シリアライズしたデータのサイズ
			IID_PPV_ARGS(&rootSignature));		// 格納先のポインタ
		if (FAILED(hr))
		{
			HRESULT hr2 = pDevice->GetDeviceRemovedReason();
			printf("ルートシグネチャの生成に失敗。 %u, %u, シェーダー : %s\n", (uint32)hr, (uint32)hr2, "");
		}

		g_rootSignatureCache.try_emplace(shaderHash);
		g_rootSignatureCache[shaderHash] = rootSignature;

		m_pRootSignature = g_rootSignatureCache[shaderHash].Get();
		m_shaderHash = shaderHash;
	}
}
