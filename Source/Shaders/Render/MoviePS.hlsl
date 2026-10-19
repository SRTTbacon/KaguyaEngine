#include "..\\CommonPS.hlsl"

Texture2D<float> inputTextureY : register(t0);
Texture2D<float2> inputTextureUV : register(t1);

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

PSOutput main(VSOutput input)
{
    float y = inputTextureY.Sample(texSampler, input.uv);
    float2 uvSample = inputTextureUV.Sample(texSampler, input.uv);

    // ITU-R BT.601 YUV -> RGB 変換式
    float C = y - 16.0 / 255.0;
    float D = uvSample.x - 0.5;
    float E = uvSample.y - 0.5;

    float r = saturate(1.164383 * C + 1.596027 * E);
    float g = saturate(1.164383 * C - (0.391762 * D) - (0.812968 * E));
    float b = saturate(1.164383 * C + 2.017232 * D);
    
    //テクスチャ
    float4 tex = float4(r, g, b, 1.0f);
    
    PSOutput output;
    output.albedo = tex;

    float3 n = normalize(input.normal);
    float3 enc = n * 0.5f + 0.5f;
    output.normal = float4(enc, 1.0f);

    // 速度計算
    float2 currNDC = input.projPos.xy / input.projPos.w;
    float2 prevNDC = input.projPosPrev.xy / input.projPosPrev.w;
    output.velocity = currNDC - prevNDC;

    return output;
}

