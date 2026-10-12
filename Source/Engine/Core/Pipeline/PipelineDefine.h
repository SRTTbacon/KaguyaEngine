//*****************************************************************************
//
// シェーダーの構築に必要な情報を定義した構造体および関数
//
// PipelineDefine.h
//
// T_Takeuchi
//
// 2026/5/25
//
//*****************************************************************************

#pragma once

#include "..\\..\\Core\\CoreMinimal.h"

#include <d3d12.h>
#include <dxgiformat.h>
#include <string>

namespace KaguyaEngine::Core
{
	enum DepthStencilPreset
	{
		DepthStencilPreset_None,    // プリセットなし。デフォルトの深度ステンシルステートが使用される
		DepthStencilPreset_Skybox,  // スカイボックス用の深度ステンシルステート。深度テストは有効だが、深度書き込みは無効
		DepthStencilPreset_UI,      // UIやPostProcessなどのの深度ステンシルステート。深度テストも深度書き込みも無効
		DepthStencilPreset_Custom,  // カスタムの深度ステンシルステート。PsoDescKey::DepthStencilDescで指定した内容が使用される
	};

	struct PsoDescKey {
		std::string vertexShaderPath;   // 頂点シェーダーパス
		std::string pixelShaderPath;    // ピクセルシェーダーパス
		std::string domainShaderPath;   // ドメインシェーダーパス
		std::string hullShaderPath;     // ハルシェーダーパス
		std::string csShaderPath;       // コンピュートシェーダーパス
		D3D12_CULL_MODE cullMode;		// カリングモード
		D3D12_RENDER_TARGET_BLEND_DESC blendDesc;				// ブレンドステートの設定
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc;				// 深度ステンシルステートの設定
		D3D12_TEXTURE_ADDRESS_MODE textureAddressMode;			// テクスチャのアドレッシングモード (デフォルトはD3D12_TEXTURE_ADDRESS_MODE_WRAP)
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType;	// デフォルトはD3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE (テッセレーション時はD3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH)
		DepthStencilPreset depthStencilPreset;					// 深度ステンシルのプリセット
		DXGI_FORMAT rtvFormats[8];		// 各レンダーターゲットのフォーマット (最大8個)
		uint8 numRenderTargets;			// レンダーターゲット数 (1～8個)
		bool bAlphaBlend;				// 半透明 (デフォルトはfalse)
		bool bDisableDepth;				// 常に手前 (デフォルトはfalse)

		PsoDescKey();

		// このパイプライン構造のハッシュ値を求める
		uint64 GetHash() const;
	};


	struct PsoInfo
	{
		ComPtr<ID3D12PipelineState> pPipelineState;
		ID3D12RootSignature* pRootSignature;
		uint64 rootSignatureHash;
		PsoDescKey desc;

		PsoInfo() : pRootSignature(nullptr), rootSignatureHash(0), desc(PsoDescKey()) {}
	};

	// 頂点シェーダー、ピクセルシェーダーのパスからハッシュ値を取得
	extern uint64 GetShaderHash(std::string vertexShader, std::string pixelShader);
	// g_rootSignatureCacheからルートシグネチャの解放
	extern void DeleteRootSignature(ID3D12RootSignature* pRootSignature);
	// g_pipelineStateCacheからパイプラインステートの解放
	extern void DeletePipelineState(ID3D12PipelineState* pPipelineState);

	// ルートシグネチャのキャッシュ。頂点シェーダー、ピクセルシェーダーともに同じであれば再利用可能
	// MaterialManagerのデストラクタで解放
	// key : GetShaderHash()の値
	extern Map<uint64, ComPtr<ID3D12RootSignature>> g_rootSignatureCache;
	// パイプラインステートのキャッシュ
	// MaterialManagerのデストラクタで解放
	// key : PsoDescKey::GetHash()の値
	extern Map<uint64, PsoInfo> g_pipelineStateCache;
}
