#include "..\CoreMinimal.h"

#include "PipelineDefine.h"
#include "..\Hash\xxhash.h"

#include <string>
#include <d3d12.h>
#include <dxgiformat.h>

namespace KaguyaEngine::Core
{
	Map<uint64, ComPtr<ID3D12RootSignature>> g_rootSignatureCache;
	std::mutex g_rootSignatureCacheMutex;
	Map<uint64, PsoInfo> g_pipelineStateCache;

	PsoDescKey::PsoDescKey()
		: cullMode(D3D12_CULL_MODE_NONE)
		, vertexShaderPath("")
		, pixelShaderPath("")
		, domainShaderPath("")
		, hullShaderPath("")
		, csShaderPath("")
		, blendDesc(D3D12_RENDER_TARGET_BLEND_DESC())
		, depthStencilDesc(D3D12_DEPTH_STENCIL_DESC())
		, textureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_WRAP)
		, depthStencilPreset(DepthStencilPreset_None)
		, primitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
		, numRenderTargets(3)
		, bAlphaBlend(false)
		, bDisableDepth(false)
	{
		// 通常描画 (Camera.cppで定義されているレンダラを参照)
		rtvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtvFormats[1] = DXGI_FORMAT_R10G10B10A2_UNORM;
		rtvFormats[2] = DXGI_FORMAT_R16G16_FLOAT;

		blendDesc =
		{
			false, false,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};
	}

	uint64 PsoDescKey::GetHash() const
	{
		XXH3_state_t* state = XXH3_createState();
		XXH3_64bits_reset(state);

		// シェーダーパス
		XXH3_64bits_update(state, vertexShaderPath.data(), vertexShaderPath.size());
		XXH3_64bits_update(state, pixelShaderPath.data(), pixelShaderPath.size());
		XXH3_64bits_update(state, domainShaderPath.data(), domainShaderPath.size());
		XXH3_64bits_update(state, hullShaderPath.data(), hullShaderPath.size());
		XXH3_64bits_update(state, csShaderPath.data(), csShaderPath.size());

		// POD部分
		XXH3_64bits_update(state, &cullMode, sizeof(cullMode));
		XXH3_64bits_update(state, &blendDesc, sizeof(blendDesc));
		XXH3_64bits_update(state, &depthStencilDesc, sizeof(depthStencilDesc));
		XXH3_64bits_update(state, &textureAddressMode, sizeof(textureAddressMode));
		XXH3_64bits_update(state, &primitiveTopologyType, sizeof(primitiveTopologyType));
		XXH3_64bits_update(state, &depthStencilPreset, sizeof(depthStencilPreset));
		XXH3_64bits_update(state, rtvFormats, sizeof(rtvFormats));
		XXH3_64bits_update(state, &numRenderTargets, sizeof(numRenderTargets));
		XXH3_64bits_update(state, &bAlphaBlend, sizeof(bAlphaBlend));
		XXH3_64bits_update(state, &bDisableDepth, sizeof(bDisableDepth));

		uint64 hash = XXH3_64bits_digest(state);
		XXH3_freeState(state);

		return hash;
	}

	uint64 GetShaderHash(std::string vertexShaderPath, std::string pixelShaderPath)
	{
		XXH3_state_t* state = XXH3_createState();
		XXH3_64bits_reset(state);

		// シェーダーパス
		XXH3_64bits_update(state, vertexShaderPath.data(), vertexShaderPath.size());
		XXH3_64bits_update(state, pixelShaderPath.data(), pixelShaderPath.size());

		uint64 hash = XXH3_64bits_digest(state);
		XXH3_freeState(state);

		return hash;
	}

	void DeleteRootSignature(ID3D12RootSignature* pRootSignature)
	{
		for (const auto& pair : g_rootSignatureCache)
		{
			if (pair.second.Get() == pRootSignature)
			{
				g_rootSignatureCache.erase(pair.first);
				break;
			}
		}
	}

	void DeletePipelineState(ID3D12PipelineState* pPipelineState)
	{
		for (const auto& pair : g_pipelineStateCache)
		{
			if (pair.second.pPipelineState.Get() == pPipelineState)
			{
				g_pipelineStateCache.erase(pair.first);
				break;
			}
		}
	}
}