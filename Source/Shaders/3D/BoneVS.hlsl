#define COMMON_BUFFER_DEFINED
#include "..\\CommonVS.hlsl"

cbuffer BoneMatrices : register(b0)
{
    matrix boneMatrices[1024]; // ボーンマトリックス（現在のフレーム512個 + 前のフレーム512個)
}

cbuffer UniqueVSBuffer : register(b1, space1)
{
    UniqueVSStruct commonBuffer;
    uint shapeCount;
};

Texture2D<float4> ShapeDeltasTexture : register(t0); // シェイプキーごとの位置変位データ
StructuredBuffer<float> ShapeWeights : register(t1); // 各シェイプキーのウェイト

float4x4 InvTangentMatrix(float3 tangent, float3 binormal, float3 normal)
{
    float4x4 mat =
    {
        float4(tangent, 0.0f),
        float4(binormal, 0.0f),
        float4(normal, 0.0f),
        { 0.0f, 0.0f, 0.0f, 1.0f }
    };
    return transpose(mat); // 転置
}

struct VSInput2
{
    VSInput common;
    float4 boneWeights : BONEWEIGHTS;   // 各頂点のボーンの影響度
    uint4 boneIDs : BONEIDS;            // 各頂点に影響を与えるボーンのインデックス
};

// 頂点IDとシェイプキーのIDから相対位置を取得
float3 GetShapeDelta(uint vertexID, uint shapeID)
{
    // ウェイトが非常に小さい場合は無視
    float weight = ShapeWeights[shapeID];
    if (weight < 0.001f)
        return float3(0.0f, 0.0f, 0.0f);
    
    // シェイプキーのテクスチャサイズを取得
    uint width = 0, height = 0;
    ShapeDeltasTexture.GetDimensions(width, height);

    // 座標
    uint x = vertexID % width;
    uint y = (vertexID / width) * shapeCount + shapeID;

    return ShapeDeltasTexture.Load(int3(x, y, 0)).xyz * weight;
}

float4 GetSkinnedPosition(float4 boneWeight, uint4 boneID, float3 localPos)
{
    // ボーンを反映
    float4 skinnedPos =
    boneWeight.x * mul(float4(localPos, 1.0f), boneMatrices[boneID.x]) +
    boneWeight.y * mul(float4(localPos, 1.0f), boneMatrices[boneID.y]) +
    boneWeight.z * mul(float4(localPos, 1.0f), boneMatrices[boneID.z]) +
    boneWeight.w * mul(float4(localPos, 1.0f), boneMatrices[boneID.w]);

    return skinnedPos;
}

float3 GetSkinnedNormal(float4 boneWeight, uint4 boneID, float3 normal)
{
    // ボーンアニメーションによる法線変換
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
    skinnedNormal += boneWeight.x * mul(normal, (float3x3) boneMatrices[boneID.x]);
    skinnedNormal += boneWeight.y * mul(normal, (float3x3) boneMatrices[boneID.y]);
    skinnedNormal += boneWeight.z * mul(normal, (float3x3) boneMatrices[boneID.z]);
    skinnedNormal += boneWeight.w * mul(normal, (float3x3) boneMatrices[boneID.w]);

    return skinnedNormal;
}

VSOutput main(VSInput2 input)
{
    VSOutput output;
	
    // シェイプキーの影響を加算
    float3 shapePos = input.common.pos;
    for (uint i = 0; i < shapeCount.x; i++)
    {
        shapePos += GetShapeDelta(input.common.vertexID, i);
    }

	// ボーンを反映
    float4 skinnedPos = GetSkinnedPosition(input.boneWeights, input.boneIDs, shapePos);
	
    float4 meshPos = mul(commonBuffer.meshMatrix, skinnedPos);              // ワールド座標に変換
    float4 meshPosPrev = mul(commonBuffer.meshMatrixPrev, skinnedPos);      // 前のフレームのワールド座標に変換
    float4 worldPos = mul(commonBuffer.modelMatrix, meshPos);               // ワールド座標に変換
    float4 worldPosPrev = mul(commonBuffer.modelMatrixPrev, meshPosPrev);   // 前のフレームのワールド座標に変換
    float4 viewPos = mul(viewMatrix, worldPos);                             // ビュー座標に変換
    float4 viewPosPrev = mul(viewMatrixPrev, worldPosPrev);                 // 前のフレームのビュー座標に変換
    float4 projPos = mul(projectionMatrix, viewPos);                        // 投影変換
    float4 projPosPrev = mul(projectionMatrixPrev, viewPosPrev);            // 前のフレームの投影変換

	// ボーンアニメーションによる法線変換
    float3 skinnedNormal = GetSkinnedNormal(input.boneWeights, input.boneIDs, input.common.normal);
    float3 nor = normalize(mul(input.common.normal, (float3x3) commonBuffer.modelMatrix));
    float3 bi = normalize(mul(input.common.binormal, (float3x3) commonBuffer.modelMatrix));
    float3 tan = normalize(mul(input.common.tangent, (float3x3) commonBuffer.modelMatrix));
	
    output.svpos = projPos; // 投影変換された座標
    output.normal = normalize(mul(commonBuffer.normalMatrix, float4(skinnedNormal, 0.0f)).xyz);
    output.uv = input.common.uv; // UV
    output.worldPos = worldPos.xyz / worldPos.w;

    projPos.xy += cameraJitters.xy * 2.0f * projPos.w;
    projPosPrev.xy += cameraJitters.zw * 2.0f * projPosPrev.w;

    output.projPos = projPos;
    output.projPosPrev = projPosPrev;
    output.viewPosZ = viewPos.z;

    return output; // ピクセルシェーダーに渡す
}