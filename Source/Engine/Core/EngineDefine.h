//*****************************************************************************
//
// エンジン内部でも使用する定数をまとめたヘッダーファイル
//
// EngineDefine.h
//
// T_Takeuchi
//
// 2026/5/24
//
//*****************************************************************************

#pragma once

// -----エンジン関連の定数定義-----

constexpr int FRAME_BUFFER_COUNT = 3;	// フレームバッファの数 (デフォルトはトリプルバッファリング)
constexpr int MAX_CAMERA_COUNT = 5;		// 最大3Dカメラ数

#include <d3d12.h>
#include <dxgitype.h>
#include <DirectXMath.h>
#include <string>
#include <cstdio>

// -----よく使用する色の定数定義-----

#ifdef TRANSPARENT
#undef TRANSPARENT
#endif

namespace Color
{
	inline const DXGI_RGBA WHITE = DXGI_RGBA(1.0f, 1.0f, 1.0f, 1.0f);

	inline const DXGI_RGBA BLACK = DXGI_RGBA(0.0f, 0.0f, 0.0f, 1.0f);

	inline const DXGI_RGBA GRAY = DXGI_RGBA(0.5f, 0.5f, 0.5f, 1.0f);

	inline const DXGI_RGBA RED = DXGI_RGBA(1.0f, 0.0f, 0.0f, 1.0f);

	inline const DXGI_RGBA GREEN = DXGI_RGBA(0.0f, 1.0f, 0.0f, 1.0f);

	inline const DXGI_RGBA BLUE = DXGI_RGBA(0.0f, 0.0f, 1.0f, 1.0f);

	inline const DXGI_RGBA CYAN = DXGI_RGBA(0.0f, 1.0f, 1.0f, 1.0f);

	inline const DXGI_RGBA MAGENTA = DXGI_RGBA(1.0f, 0.0f, 1.0f, 1.0f);

	inline const DXGI_RGBA YELLOW = DXGI_RGBA(1.0f, 0.92f, 0.016f, 1.0f);

	inline const DXGI_RGBA TRANSPARENT = DXGI_RGBA(0.0f, 0.0f, 0.0f, 0.0f);
}

namespace KaguyaEngine::Texture
{
	class Texture2D;
}

struct MaterialTexture
{
	ObjectPtr<KaguyaEngine::Texture::Texture2D> pTexture;	// テクスチャ本体
	uint32 mainDescriptorIndex;	// m_pDescriptor->pMainHeapに登録されたインデックス
	uint32 copyDescriptorIndex;	// m_pDescriptor->pCopyHeapに登録されたインデックス
	MaterialTexture()
		: pTexture(nullptr)
		, mainDescriptorIndex(UINT32_MAX)
		, copyDescriptorIndex(UINT32_MAX)
	{}
};

struct RenderResource
{
	ID3D12Resource* pResource;	// リソース
	D3D12_CPU_DESCRIPTOR_HANDLE mainSrvHandleCPU;	// メインレンダラの結果を格納するリソースのpMainHeapのSRVハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE mainSrvHandleGPU;	// メインレンダラの結果を格納するリソースのpMainHeapのSRVハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE copySrvHandleCPU;	// メインレンダラの結果を格納するリソースのpCopyHeapのSRVハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE copySrvHandleGPU;	// メインレンダラの結果を格納するリソースのpCopyHeapのSRVハンドル
	uint32 mainSrvIndex;	// メインレンダラの結果を格納するリソースのpMainHeapのSRVインデックス
	uint32 mainRtvIndex;	// メインレンダラの結果を格納するリソースのm_pRtvHeapのRTVインデックス

	RenderResource()
		: pResource(nullptr)
		, mainSrvHandleCPU(D3D12_CPU_DESCRIPTOR_HANDLE())
		, mainSrvHandleGPU(D3D12_GPU_DESCRIPTOR_HANDLE())
		, copySrvHandleCPU(D3D12_CPU_DESCRIPTOR_HANDLE())
		, copySrvHandleGPU(D3D12_GPU_DESCRIPTOR_HANDLE())
		, mainSrvIndex(UINT32_MAX)
		, mainRtvIndex(UINT32_MAX)
	{}
};

// シェーダーパス取得
#ifdef _DEBUG
constexpr auto BUILD_MODE = "Resource\\DebugShaders\\";
#else
constexpr auto BUILD_MODE = "Resource\\ReleaseShaders\\";
#endif

static const std::string GetShaderPath(const std::string& shaderName)
{
	if (shaderName.ends_with(".cso")) {
		return BUILD_MODE + shaderName;
	}
	return BUILD_MODE + shaderName + ".cso";
}

inline std::vector<D3D12_INPUT_ELEMENT_DESC> GetBasicVertexLayout()
{
    return
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        },
        {
            "TEXCOORD",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        }
    };
}

#ifdef _DEBUG
static void PrintfMatrix(const DirectX::XMMATRIX& matrix)
{
	printf("%f, %f, %f, %f\n%f, %f, %f, %f\n%f, %f, %f, %f\n%f, %f, %f, %f\n", matrix.r[0].m128_f32[0], matrix.r[0].m128_f32[1], matrix.r[0].m128_f32[2], matrix.r[0].m128_f32[3],
		matrix.r[1].m128_f32[0], matrix.r[1].m128_f32[1], matrix.r[1].m128_f32[2], matrix.r[1].m128_f32[3],
		matrix.r[2].m128_f32[0], matrix.r[2].m128_f32[1], matrix.r[2].m128_f32[2], matrix.r[2].m128_f32[3],
		matrix.r[3].m128_f32[0], matrix.r[3].m128_f32[1], matrix.r[3].m128_f32[2], matrix.r[3].m128_f32[3]);
}

static void PrintfFloat3(const DirectX::XMFLOAT3& float3)
{
	printf("%f, %f, %f\n", float3.x, float3.y, float3.z);
}
#endif
