#include "..\\CommonVS.hlsl"

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;
    float4 localPos = float4(input.pos, 1.0f); //頂点座標
    float4 meshPos = mul(commonBuffer.meshMatrix, localPos); //ローカル座標に変換
    float4 meshPosPrev = mul(commonBuffer.meshMatrixPrev, localPos); //前のフレームのローカル座標に変換
    float4 worldPos = mul(commonBuffer.modelMatrix, meshPos); //ワールド座標に変換
    float4 worldPosPrev = mul(commonBuffer.modelMatrixPrev, meshPosPrev); //前のフレームのワールド座標に変換
    float4 viewPos = mul(viewMatrix, worldPos); //ビュー座標に変換
    float4 viewPosPrev = mul(viewMatrixPrev, worldPosPrev); //前のフレームのビュー座標に変換
    float4 projPos = mul(projectionMatrix, viewPos); //投影変換
    float4 projPosPrev = mul(projectionMatrixPrev, viewPosPrev); //前のフレームの投影変換
    
    output.projPos = projPos;
    output.projPosPrev = projPosPrev;
    output.viewPosZ = viewPos.z;

    output.svpos = projPos; //投影変換された座標
    output.worldPos = worldPos.xyz; //ワールド座標
    output.normal = normalize(mul(commonBuffer.normalMatrix, float4(input.normal, 0.0f)).xyz);
    output.uv = input.uv; //UV
    return output; //ピクセルシェーダーに渡す
}