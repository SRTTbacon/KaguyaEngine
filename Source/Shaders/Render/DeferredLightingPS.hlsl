Texture2D<float4> albedoTex : register(t0);
Texture2D<float3> normalTex : register(t1);
Texture2D<float2> velocityTex : register(t2);
Texture2D<float> depthTex : register(t3);

SamplerState texSampler : register(s0);

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

cbuffer LightingBuffer : register(b0)
{
    matrix invView;         // 逆ビュー行列
    matrix invProjection;   // 逆プロジェクション行列
    float4 lightDir;        // ライトの方向
    float4 diffuseColor;    // 標準の色
    float4 ambientColor;    // 影色
    float4 fogColor;        // フォグの色
    float2 fogStartEnd;     // フォグの開始距離、終了距離
    float3 cameraPosition;  // カメラの位置
    float sceneTime;        // シーンの時間
};

//法線と光の方向に応じたバイアス
float ComputeShadowBias(float3 normal, float3 lightDir)
{
    float cosTheta = saturate(dot(normal, lightDir));
    return max(0.0015 * (1.0 - cosTheta), 0.0005f); //法線が光と平行なほどバイアスを減らす
}

float rand(float2 co)
{
    return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}


// ライティング計算
float4 main(VSOutput input) : SV_TARGET
{
    float4 albedo = albedoTex.Sample(texSampler, input.uv);
    float3 normal = normalTex.Sample(texSampler, input.uv);
    float depth = depthTex.Sample(texSampler, input.uv);

    // ディレクショナルライトが当たるかどうかの判定
    float3 L = normalize(-lightDir);
    float NdotL = saturate(dot(normal, L));

    float4 directLight = diffuseColor * NdotL;
    directLight.a = 1.0f;

    float4 lighting = ambientColor + directLight;
    lighting *= albedo;
    lighting.a = 1.0f;
    
    float3 ndc = float3(input.uv.x * 2.0f - 1.0f, 1.0f - input.uv.y * 2.0f, depth);
    float4 viewPos = mul(float4(ndc, 1.0f), invProjection);
    viewPos /= viewPos.w;
    float4 worldPos = mul(viewPos, invView);

    //カメラからの距離計算
    float distance = abs(length(worldPos.xyz - cameraPosition));
    //0 ～ 1にマッピング
    float fogFactor = saturate((distance - fogStartEnd.x) / (fogStartEnd.y - fogStartEnd.x));
    fogFactor = min(0.9f, fogFactor);
    
    float4 lightingColor = lerp(lighting, fogColor, fogFactor);
    return lightingColor;

}