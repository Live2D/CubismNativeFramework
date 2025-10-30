/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#ifndef ALPHABLEND_METAL
#define ALPHABLEND_METAL

#include <metal_math>
#include <metal_common>

using namespace metal;

#if !defined CSM_ALPHA_BLEND_MODE
#error not define symbol: CSM_ALPHA_BLEND_MODE.

#else
float4 OverlapRgba(float3 color, float3 colorSource, float3 colorDestination, float3 parameter)
{
    float3 rgb = color * parameter.x + colorSource * parameter.y + colorDestination * parameter.z;
    float alpha = parameter.x + parameter.y + parameter.z;
    return float4(rgb, alpha);
}

#if CSM_ALPHA_BLEND_MODE == 0 // OVER
float4 AlphaBlend(float3 color, float4 colorSource, float4 colorDestination)
{
    float3 parameter = float3(colorSource.a * colorDestination.a, colorSource.a * (1.0 - colorDestination.a), colorDestination.a * (1.0 - colorSource.a));
    return OverlapRgba(color, colorSource.rgb, colorDestination.rgb, parameter);
}

#elif CSM_ALPHA_BLEND_MODE == 1 // ATOP
float4 AlphaBlend(float3 color, float4 colorSource, float4 colorDestination)
{
    float3 parameter = float3(colorSource.a * colorDestination.a, 0, colorDestination.a * (1.0 - colorSource.a));
    return OverlapRgba(color, colorSource.rgb, colorDestination.rgb, parameter);
}

#elif CSM_ALPHA_BLEND_MODE == 2 // OUT
float4 AlphaBlend(float3 color, float4 colorSource, float4 colorDestination)
{
    float3 parameter = float3(0.0, 0.0, colorDestination.a * (1.0 - colorSource.a));
    return OverlapRgba(color, colorSource.rgb, colorDestination.rgb, parameter);
}

#elif CSM_ALPHA_BLEND_MODE == 3 // CONJOINT OVER
float4 AlphaBlend(float3 color, float4 colorSource, float4 colorDestination)
{
    float3 parameter = float3(min(colorSource.a, colorDestination.a), max(colorSource.a - colorDestination.a, 0.0), max(colorDestination.a - colorSource.a, 0.0));
    return OverlapRgba(color, colorSource.rgb, colorDestination.rgb, parameter);
}

#elif CSM_ALPHA_BLEND_MODE == 4 // DISJOINT OVER
float4 AlphaBlend(float3 color, float4 colorSource, float4 colorDestination)
{
    float3 parameter = float3(max(colorSource.a + colorDestination.a - 1.0, 0.0), min(colorSource.a, 1.0 - colorDestination.a), min(colorDestination.a, 1.0 - colorSource.a));
    return OverlapRgba(color, colorSource.rgb, colorDestination.rgb, parameter);
}

#else
#error not supported blend function
#endif

#endif
#endif
