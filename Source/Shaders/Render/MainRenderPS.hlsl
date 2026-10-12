Texture2D<float4> uiTex : register(t0);         //UIのレンダリング結果
Texture2D<float4> cameraTex1 : register(t1);    //メイン3Dレンダリング結果
Texture2D<float4> cameraTex2 : register(t2);    //メイン3Dレンダリング結果
Texture2D<float4> cameraTex3 : register(t3);    //メイン3Dレンダリング結果
Texture2D<float4> cameraTex4 : register(t4);    //メイン3Dレンダリング結果
Texture2D<float4> cameraTex5 : register(t5);    //メイン3Dレンダリング結果
SamplerState samLinear : register(s0);

struct PSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

struct CameraInfo
{
    float2 position;    // 左上（0～1）
    float2 size;        // 幅・高さ（0～1）
};
cbuffer CameraInfoList : register(b0)
{
    CameraInfo cameras[5]; // 将来の拡張用に少し余裕を持たせる
    uint cameraCount;
};

float3 LinearTosRGB(in float3 color)
{
    float3 x = color * 12.92f;
    float3 y = 1.055f * pow(saturate(color), 1.0f / 2.4f) - 0.055f;

    float3 clr = color;
    clr.r = color.r < 0.0031308f ? x.r : y.r;
    clr.g = color.g < 0.0031308f ? x.g : y.g;
    clr.b = color.b < 0.0031308f ? x.b : y.b;

    return clr;
}

float3 SRGBToLinear(in float3 color)
{
    float3 x = color / 12.92f;
    float3 y = pow(max((color + 0.055f) / 1.055f, 0.0f), 2.4f);

    float3 clr = color;
    clr.r = color.r <= 0.04045f ? x.r : y.r;
    clr.g = color.g <= 0.04045f ? x.g : y.g;
    clr.b = color.b <= 0.04045f ? x.b : y.b;

    return clr;
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

float4 main(PSInput input) : SV_Target
{
    float4 color = 0;
    for (uint i = 0; i < cameraCount; i++)
    {
        float2 localUV = (input.uv - cameras[i].position) / cameras[i].size;
        if (localUV.x >= 0.0f && localUV.x <= 1.0f && localUV.y >= 0.0f && localUV.y <= 1.0f)
        {
            float4 src = 0;
            switch (i)
            {
                case 0:
                    src = cameraTex1.Sample(samLinear, localUV);
                    break;
                case 1:
                    src = cameraTex2.Sample(samLinear, localUV);
                    break;
                case 2:
                    src = cameraTex3.Sample(samLinear, localUV);
                    break;
                case 3:
                    src = cameraTex4.Sample(samLinear, localUV);
                    break;
                case 4:
                    src = cameraTex5.Sample(samLinear, localUV);
                    break;
            }
            color.rgb = lerp(color.rgb, src.rgb, src.a);
            color.a = color.a + src.a * (1 - color.a);
        }
    }
    float4 uiColor = uiTex.Sample(samLinear, input.uv);
    
    float3 hdrLinear = ACESFilm(color.rgb);

    // ガンマ補正を適用（任意）
    //const float gamma = 2.2f;
    //float3 hdrLinear = pow(abs(sceneColor), 1.0f / gamma);

    //sceneColor += effectColor;

    //float3 hdrLinear = sceneColor.rgb + effectColor.rgb;

    float3 finalRGB = lerp(hdrLinear, uiColor.rgb, uiColor.a);
    return float4(finalRGB.rgb, 1.0f);
}
