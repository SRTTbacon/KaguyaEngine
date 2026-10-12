cbuffer ViewProjection : register(b0)
{
    matrix viewProj;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

struct VSInput
{
    float3 position : POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    //頂点位置をそのままスカイボックスのテクスチャ座標に使用
    output.texCoord = input.position;

    //カメラの位置を考慮し、スカイボックスの遠方に配置
    float4 pos = float4(input.position, 0.0f);
    pos = mul(viewProj, pos);
    //遠方に配置するため、W成分をそのままX,Y,Zに適用
    output.position = float4(pos.x, pos.y, pos.w, pos.w);
    
    return output;
}

