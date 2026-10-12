Texture2D originalColor : register(t0); // 元の高解像度カラー
Texture2D cocMap : register(t1); // CoCマップ
Texture2D blurredColor : register(t2); // ブラー済みカラー
SamplerState samLinear : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    float coc = cocMap.Sample(samLinear, input.uv).r;
    coc = saturate(coc / 1.0); // CoC を[0,1]に正規化（必要なら）

    float4 sharp = originalColor.Sample(samLinear, input.uv);
    float4 blur = blurredColor.Sample(samLinear, input.uv);
    
    return lerp(sharp, blur, coc); // CoC に応じて合成
}


