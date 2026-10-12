Texture2D<float> depthTex : register(t0);
SamplerState samLinear : register(s0);

cbuffer DoFParams : register(b0)
{
    float focusDistance;    //ピントを合わせたい距離（リニアZ）
    float focusRange;       //許容範囲
    float maxCoC;           //最大ボケ半径（ピクセル単位）
    float nearZ;
    float farZ;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float LinearizeDepth(float z)
{
    // 非線形の深度をリニアに変換（右手系の透視射影用）
    return (nearZ * farZ) / (farZ - z * (farZ - nearZ));
    return (nearZ * farZ) / (farZ - z * (farZ - nearZ));
}

float4 main(PSInput input) : SV_Target
{
    float depth = depthTex.Sample(samLinear, input.uv);
    float linearDepth = LinearizeDepth(depth);

    float coc = abs(linearDepth - focusDistance) / focusRange;
    coc = saturate(coc);    //0～1にクランプ
    coc *= maxCoC;          //実ピクセル単位の CoC に変換

    return float4(coc, 0.0f, 0.0f, 1.0f); //Rチャネルに出力
}


