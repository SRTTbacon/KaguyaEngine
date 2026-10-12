TextureCube skyboxTexture : register(t0);
SamplerState samplerState : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

float4 main(VSOutput input) : SV_TARGET
{
    return skyboxTexture.Sample(samplerState, input.texCoord);
}


