#define SHADOWSIZE 8192.0f

// 必須固有バッファ
struct UniquePSStruct
{
    float2 RenderSize;      // カメラ解像度
};

// 3Dレンダリング用の出力構造体
struct PSOutput
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    float2 velocity : SV_Target2;
};

// 独自にUniquePSBufferを定義する場合はinclude前に
// #define COMMON_BUFFER_DEFINED を宣言してください。
#ifndef COMMON_BUFFER_DEFINED
#define COMMON_BUFFER_DEFINED
cbuffer UniquePSBuffer : register(b1, space1)
{
    UniquePSStruct commonBuffer;
};
#endif

SamplerState texSampler : register(s0); //テクスチャ用サンプラー
