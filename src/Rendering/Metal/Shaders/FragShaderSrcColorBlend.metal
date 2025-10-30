/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#ifndef COLORBLEND_METAL
#define COLORBLEND_METAL

#include <metal_math>
#include <metal_common>

using namespace metal;

float4 ConvertPremultipliedToStraight(float4 source)
{
    if (abs(source.a) < 0.00001)
    {
        source.rgb = float3(0.0, 0.0, 0.0);
    }
    else
    {
        source.rgb /= source.a;
    }

    return source;
}

#if !defined CSM_COLOR_BLEND_MODE
#error not define symbol: CSM_COLOR_BLEND_MODE.

#elif CSM_COLOR_BLEND_MODE == 0 // NORMAL
float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return colorSource;
}

#elif CSM_COLOR_BLEND_MODE == 1 // ADD
float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return min(colorSource + colorDestination, 1.0);
}

#elif CSM_COLOR_BLEND_MODE == 2 // ADD(GLOW)
float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return colorSource + colorDestination;
}

#elif CSM_COLOR_BLEND_MODE == 3 // DARKEN
float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return min(colorSource, colorDestination);
}

#elif CSM_COLOR_BLEND_MODE == 4 // MULTIPLY
float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return colorSource * colorDestination;
}

#elif CSM_COLOR_BLEND_MODE == 5 // COLOR BURN
float ColorBurn(float colorSource, float colorDestination)
{
    if (abs(colorDestination - 1.0) < 0.000001)
    {
        return 1.0;
    }
    else if (abs(colorSource) < 0.000001)
    {
        return 0.0;
    }

    return 1.0 - min(1.0, (1.0 - colorDestination) / colorSource);
}

float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return float3(
        ColorBurn(colorSource.r, colorDestination.r),
        ColorBurn(colorSource.g, colorDestination.g),
        ColorBurn(colorSource.b, colorDestination.b)
    );
}

#elif CSM_COLOR_BLEND_MODE == 6 // LINEAR BURN
float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return max(float3(0.0), colorSource + colorDestination - 1.0);
}

#elif CSM_COLOR_BLEND_MODE == 7 // LIGHTEN
float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return max(colorSource, colorDestination);
}

#elif CSM_COLOR_BLEND_MODE == 8 // SCREEN
float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return colorSource + colorDestination - colorSource * colorDestination;
}

#elif CSM_COLOR_BLEND_MODE == 9 // COLOR DODGE
float ColorDodge(float colorSource, float colorDestination)
{
    if (colorDestination <= 0.0)
    {
        return 0.0;
    }
    else if (colorSource == 1.0)
    {
        return 1.0;
    }

    return min(1.0, colorDestination / (1.0 - colorSource));
}

float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return float3(
        ColorDodge(colorSource.r, colorDestination.r),
        ColorDodge(colorSource.g, colorDestination.g),
        ColorDodge(colorSource.b, colorDestination.b)
    );
}

#elif CSM_COLOR_BLEND_MODE == 10 // OVERLAY
float Overlay(float colorSource, float colorDestination)
{
    float mul = 2.0 * colorSource * colorDestination;
    float scr = 1.0 - 2.0 * (1.0 - colorSource) * (1.0 - colorDestination) ;
    return colorDestination < 0.5 ? mul : scr ;
}

float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return float3(
        Overlay(colorSource.r, colorDestination.r),
        Overlay(colorSource.g, colorDestination.g),
        Overlay(colorSource.b, colorDestination.b)
    );
}

#elif CSM_COLOR_BLEND_MODE == 11 // SOFT LIGHT
float SoftLight(float colorSource, float colorDestination)
{
    float val1 = colorDestination - (1.0 - 2.0 * colorSource) * colorDestination * (1.0 - colorDestination);
    float val2 = colorDestination + (2.0 * colorSource - 1.0) * colorDestination * ((16.0 * colorDestination - 12.0) * colorDestination + 3.0);
    float val3 = colorDestination + (2.0 * colorSource - 1.0) * (sqrt(colorDestination) - colorDestination);

    if (colorSource <= 0.5)
    {
        return val1;
    }
    else if (colorDestination <= 0.25)
    {
        return val2;
    }

    return val3;
}

float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return float3(
        SoftLight(colorSource.r, colorDestination.r),
        SoftLight(colorSource.g, colorDestination.g),
        SoftLight(colorSource.b, colorDestination.b)
    );
}

#elif CSM_COLOR_BLEND_MODE == 12 // HARD LIGHT
float HardLight(float colorSource, float colorDestination)
{
    float mul = 2.0 * colorSource * colorDestination;
    float scr = 1.0 - 2.0 * (1.0 - colorSource) * (1.0 - colorDestination);

    if (colorSource < 0.5)
    {
        return mul;
    }

    return scr;
}

float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return float3(
        HardLight(colorSource.r, colorDestination.r),
        HardLight(colorSource.g, colorDestination.g),
        HardLight(colorSource.b, colorDestination.b)
    );
}

#elif CSM_COLOR_BLEND_MODE == 13 // LINEAR LIGHT
float LinearLight(float colorSource, float colorDestination)
{
    float burn = max(0.0, 2.0 * colorSource + colorDestination - 1.0);
    float dodge = min(1.0, 2.0 * (colorSource - 0.5) + colorDestination);

    if (colorSource < 0.5)
    {
        return burn;
    }

    return dodge;
}

float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return float3(
        LinearLight(colorSource.r, colorDestination.r),
        LinearLight(colorSource.g, colorDestination.g),
        LinearLight(colorSource.b, colorDestination.b)
    );
}

#elif CSM_COLOR_BLEND_MODE == 14 || CSM_COLOR_BLEND_MODE == 15
constant float rCoeff = 0.30;
constant float gCoeff = 0.59;
constant float bCoeff = 0.11;

float GetMax(float3 rgbC)
{
    return max(rgbC.r, max(rgbC.g, rgbC.b));
}

float GetMin(float3 rgbC)
{
    return min(rgbC.r, min(rgbC.g, rgbC.b));
}

float GetRange(float3 rgbC)
{
    return max(rgbC.r, max(rgbC.g, rgbC.b)) - min(rgbC.r, min(rgbC.g, rgbC.b));
}

float Saturation(float3 rgbC)
{
    return GetRange(rgbC);
}

float Luma(float3 rgbC)
{
    return rCoeff * rgbC.r + gCoeff * rgbC.g + bCoeff * rgbC.b;
}

float3 ClipColor(float3 rgbC)
{
    float   luma = Luma(rgbC);
    float   maxv = GetMax(rgbC);
    float   minv = GetMin(rgbC);
    float3    outputColor = rgbC;

    outputColor = minv < 0.0 ? luma + (outputColor - luma) * luma / (luma - minv) : outputColor;
    outputColor = maxv > 1.0 ? luma + (outputColor - luma) * (1.0 - luma) / (maxv - luma) : outputColor;

    return outputColor;
}

float3 SetLuma(float3 rgbC, float luma)
{
    return ClipColor(rgbC + (luma - Luma(rgbC)));
}

float3 SetSaturation(float3 rgbC, float saturation)
{
    float maxv = GetMax(rgbC);
    float minv = GetMin(rgbC);
    float medv = rgbC.r + rgbC.g + rgbC.b - maxv - minv;
    float outputMax, outputMed, outputMin;

    outputMax = minv < maxv ? saturation : 0.0;
    outputMed = minv < maxv ? (medv - minv) * saturation / (maxv - minv) : 0.0;
    outputMin = 0.0;

    if(rgbC.r == maxv)
    {
        return rgbC.b < rgbC.g ? float3(outputMax, outputMed, outputMin) : float3(outputMax, outputMin, outputMed);
    }
    else if(rgbC.g == maxv)
    {
        return rgbC.r < rgbC.b ? float3(outputMin, outputMax, outputMed) : float3(outputMed, outputMax, outputMin);
    }
    else
    {
        return rgbC.g < rgbC.r ? float3(outputMed, outputMin, outputMax) : float3(outputMin, outputMed, outputMax);
    }
}

#if CSM_COLOR_BLEND_MODE == 14 // HUE
float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return SetLuma(SetSaturation(colorSource, Saturation(colorDestination)), Luma(colorDestination));
}

#elif CSM_COLOR_BLEND_MODE == 15 // COLOR
float3 ColorBlend(float3 colorSource, float3 colorDestination)
{
    return SetLuma(colorSource, Luma(colorDestination)) ;
}

#endif

#else
#error not supported blend function.

#endif

#endif
