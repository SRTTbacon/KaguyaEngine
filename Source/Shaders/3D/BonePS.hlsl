#include "..\\CommonPS.hlsl"

Texture2D mainTex : register(t0);

struct VSOutput
{
    float4 svpos : SV_POSITION; //スクリーン座標
    float3 normal : NORMAL; //法線
    float2 uv : TEXCOORD; //UV
    float3 worldPos : TEXCOORD1; //ワールド座標
    float4 projPos : TEXCOORD2;
    float4 projPosPrev : TEXCOORD3;
    float viewPosZ : TEXCOORD4;
};

PSOutput main(VSOutput input) : SV_TARGET
{
    float4 tex = mainTex.Sample(texSampler, input.uv);

    PSOutput output;

    // ライティング結果とテクスチャカラーを掛け合わせる
    output.albedo = tex;

    float3 n = normalize(input.normal);
    float3 enc = n * 0.5f + 0.5f;
    output.normal = float4(enc, 1.0f);

    // 速度計算
    float2 currNDC = input.projPos.xy / input.projPos.w;
    float2 prevNDC = input.projPosPrev.xy / input.projPosPrev.w;
    float2 velocityNDC = currNDC - prevNDC;
    
    float2 velocityUV = velocityNDC * float2(0.5f, -0.5f);

    // NDC → ピクセル単位
    float2 velocityPixels = velocityNDC * commonBuffer.RenderSize;

    output.velocity = velocityPixels;
    
    return output;
}