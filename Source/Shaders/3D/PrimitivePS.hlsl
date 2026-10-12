#include "..\\CommonPS.hlsl"

struct PSInput
{
    float4 svpos : SV_POSITION; //スクリーン座標
    float3 normal : NORMAL; //法線
    float2 uv : TEXCOORD; //UV
    float3 worldPos : TEXCOORD1; //ワールド座標
    float4 projPos : TEXCOORD2;
    float4 projPosPrev : TEXCOORD3;
    float viewPosZ : TEXCOORD4;
};

PSOutput main(PSInput input) : SV_TARGET
{
    PSOutput output;

    output.albedo = float4(1.0f, 1.0f, 1.0f, 1.0f);
    
    float3 n = normalize(input.normal);
    float3 enc = n * 0.5f + 0.5f;
    output.normal = float4(enc, 1.0f);

    // 速度計算
    float2 currNDC = input.projPos.xy / input.projPos.w;
    float2 prevNDC = input.projPosPrev.xy / input.projPosPrev.w;
    output.velocity = currNDC - prevNDC;
    
    return output;
}