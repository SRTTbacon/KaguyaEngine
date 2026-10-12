Texture2D inputTex : register(t0);
SamplerState samp : register(s0);

cbuffer BloomParams : register(b0)
{
    float2 texelSize;
}

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    float3 color = 0.0f;

    color += inputTex.Sample(samp, input.uv + float2(-texelSize.x, -texelSize.y)).rgb;
    color += inputTex.Sample(samp, input.uv + float2(texelSize.x, -texelSize.y)).rgb;
    color += inputTex.Sample(samp, input.uv + float2(-texelSize.x, texelSize.y)).rgb;
    color += inputTex.Sample(samp, input.uv + float2(texelSize.x, texelSize.y)).rgb;

    color *= 0.25f;

    return float4(color, 1.0f);
}


