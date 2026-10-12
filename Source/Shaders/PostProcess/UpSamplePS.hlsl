Texture2D lowTex : register(t0);
Texture2D highTex : register(t1);

SamplerState samp : register(s0);

cbuffer BloomParams : register(b0)
{
    float intensity;
}

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    float3 highResColor = highTex.Sample(samp, input.uv).rgb;

    float3 lowResColor = lowTex.Sample(samp, input.uv).rgb;

    float3 result = highResColor + lowResColor * intensity;
    
    return float4(result, 1.0f);
}


