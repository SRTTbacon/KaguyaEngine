// HLSL Compute Shader for TAA Resolve

Texture2D<float4> CurrentColorTex : register(t0);
Texture2D<float4> HistoryColorTex : register(t1);
Texture2D<float2> MotionVectorTex : register(t2);

RWTexture2D<float4> OutputTex : register(u0);

SamplerState LinearClampSampler : register(s0);
//SamplerState PointClampSampler : register(s1);

// Helper function to convert RGB to YCoCg color space
float3 RGBToYCoCg(float3 rgb)
{
    float y = rgb.r / 4.0f + rgb.g / 2.0f + rgb.b / 4.0f;
    float co = rgb.r / 2.0f - rgb.b / 2.0f;
    float cg = -rgb.r / 4.0f + rgb.g / 2.0f - rgb.b / 4.0f;
    return float3(y, co, cg);
}

// Helper function to convert YCoCg back to RGB
float3 YCoCgToRGB(float3 ycocg)
{
    float y = ycocg.x;
    float co = ycocg.y;
    float cg = ycocg.z;

    float r = y + co - cg;
    float g = y + cg;
    float b = y - co - cg;
    return float3(r, g, b);
}

// Function to clip the history color to the neighborhood bounding box
float3 ClipHistory(float3 history, float3 boxMin, float3 boxMax)
{
    // A simplified clipping mechanism. 
    // In production, ray intersection from the history color to the center of the box is preferred.
    float3 center = 0.5f * (boxMax + boxMin);
    float3 extents = 0.5f * (boxMax - boxMin);

    float3 rayDir = history - center;
    float3 rayDirAbs = abs(rayDir);

    // Prevent division by zero
    rayDirAbs = max(rayDirAbs, 0.000001f);

    float3 intersectionDistance = extents / rayDirAbs;
    float minIntersect = min(min(intersectionDistance.x, intersectionDistance.y), intersectionDistance.z);

    if (minIntersect < 1.0f)
    {
        // The history color is outside the box. Clip it.
        return center + rayDir * minIntersect;
    }

    // The history color is inside the box. Leave it alone.
    return history;
}

[numthreads(8, 8, 1)]
void main(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    int2 pixelCoord = int2(DispatchThreadID.xy);

    // Get texture dimensions
    uint width, height;
    CurrentColorTex.GetDimensions(width, height);

    // Calculate UV coordinates
    float2 uv = (float2(pixelCoord) + 0.5f) / float2(width, height);

    // 1. Fetch current color and convert to YCoCg
    float3 currentColor = CurrentColorTex.Load(int3(pixelCoord, 0)).rgb;
    float3 currentYCoCg = RGBToYCoCg(currentColor);

    // 2. Fetch motion vector
    float2 velocity = MotionVectorTex.Load(int3(pixelCoord, 0)).xy;

    // 3. Calculate reprojection UV
    float2 historyUV = uv - velocity;

    // If the reprojected UV is off-screen, we cannot use history.
    if (any(historyUV < 0.0f) || any(historyUV > 1.0f))
    {
        OutputTex[pixelCoord] = float4(currentColor, 1.0f);
        return;
    }

    // 4. Fetch history color using bilinear filtering
    float3 historyColor = HistoryColorTex.SampleLevel(LinearClampSampler, historyUV, 0).rgb;
    float3 historyYCoCg = RGBToYCoCg(historyColor);

    // 5. Neighborhood Min/Max calculation
    float3 neighborhoodMin = currentYCoCg;
    float3 neighborhoodMax = currentYCoCg;

    // Sample the 3x3 neighborhood. Unrolling this loop is highly recommended for performance.
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            if (x == 0 && y == 0)
                continue;

            int2 offsetCoord = clamp(pixelCoord + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
            float3 neighborColor = CurrentColorTex.Load(int3(offsetCoord, 0)).rgb;
            float3 neighborYCoCg = RGBToYCoCg(neighborColor);

            neighborhoodMin = min(neighborhoodMin, neighborYCoCg);
            neighborhoodMax = max(neighborhoodMax, neighborYCoCg);
        }
    }

    // 6. Clip the history color
    historyYCoCg = ClipHistory(historyYCoCg, neighborhoodMin, neighborhoodMax);

    // 7. Calculate blend weight
    // A standard blend weight is 10% new frame, 90% history.
    // In advanced setups, this weight changes based on motion velocity and luminance differences.
    float blendWeight = 0.1f;

    // 8. Blend in YCoCg space
    float3 resolvedYCoCg = lerp(historyYCoCg, currentYCoCg, blendWeight);

    // 9. Convert back to RGB and output
    float3 resolvedRGB = YCoCgToRGB(resolvedYCoCg);

    OutputTex[pixelCoord] = float4(resolvedRGB, 1.0f);
}
