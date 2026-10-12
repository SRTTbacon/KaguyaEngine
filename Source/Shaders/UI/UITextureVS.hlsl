//*****************************************************************************
//
// UIテクスチャ、UIテキスト用の頂点シェーダー
//
// UITextureVS.hlsl
//
// T_Takeuchi
//
// 2026/6/2
//
//*****************************************************************************

cbuffer UIConstants : register(b0)
{
    float2 position;
    float2 size;
    float2 screenSize;
    float2 texCoordMin;
    float2 texCoordMax;
    float4x4 parentRotationMatrix;
    float2 parentRotationCenter;    //回転中心座標
    float4x4 childRotationMatrix;
};

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

PSInput main(VSInput input)
{
    PSInput output;

    float2 pos = input.position.xy * size;

    float4 rotatedPosition = mul(childRotationMatrix, float4(pos, 1.0f, 1.0f));

    float2 posCenter = rotatedPosition.xy - parentRotationCenter;
    rotatedPosition = mul(parentRotationMatrix, float4(posCenter, 1.0f, 1.0f));
    rotatedPosition.xy += parentRotationCenter;

    //NDC -> ピクセル座標
    float2 pixelPos = rotatedPosition.xy + position * 2.0f + float2(size.x, size.y);
    
    //ピクセル座標 -> NDC座標 (左上が原点)
    float2 ndcPos = float2((pixelPos.x / screenSize.x) - 1.0f, (1.0f - (pixelPos.y / screenSize.y)));

    output.position = float4(ndcPos, input.position.z, 1.0f);
    output.uv.x = lerp(texCoordMin.x, texCoordMax.x, input.uv.x);
    output.uv.y = lerp(texCoordMax.y, texCoordMin.y, input.uv.y);

    return output;
}


