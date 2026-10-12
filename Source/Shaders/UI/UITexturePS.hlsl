//*****************************************************************************
//
// UIテクスチャ、UIテキスト用のピクセルシェーダー
//
// UITexturePS.hlsl
//
// T_Takeuchi
//
// 2026/6/2
//
//*****************************************************************************

//2D画像用
Texture2D<float4> textureMap : register(t0);
//テキスト画像用
Texture2D<float> textMap : register(t1);
//共通サンプラー
SamplerState samplerState : register(s0);

cbuffer UIBuffer : register(b0)
{
    float4 color;
    uint mode; //2D画像=0、テキスト=1
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET
{
    if (mode == 0)
    {
        float4 tex = textureMap.Sample(samplerState, input.uv);
        tex *= color;
        return tex;
    }
    else
    {
        float alpha = textMap.Sample(samplerState, input.uv);
        float4 tex = color;
        tex.a *= alpha;
        return tex;
    }
}

