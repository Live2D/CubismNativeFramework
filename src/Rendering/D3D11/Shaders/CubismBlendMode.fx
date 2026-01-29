/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

struct ColorInfo {
    float4 source;
    float4 destination;
};

// copy shader
VS_OUT VertCopy(VS_IN In) {
    VS_OUT Out = (VS_OUT)0;
    Out.position = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
    Out.uv = In.uv;
    return Out;
}

float4 PixelCopy(VS_OUT In) : SV_Target {
    float4 color = mainTexture.Sample(mainSampler, In.uv);
    return color * baseColor;
}

float4 ConvertPremultipliedToStraight(float4 source)
{
    if (source.a < 0.00001)
    {
        source.rgb = float3(0.0, 0.0, 0.0);
    }
    else
    {
        source.rgb /= source.a;
    }

    return source;
}

// Color blend
// normal
float3 ColorBlendNormal(float3 colorSource, float3 colorDestination) {
    return colorSource;
}

// add
float3 ColorBlendAdd(float3 colorSource, float3 colorDestination) {
    return min(colorSource + colorDestination, 1.0f);
}

// add(glow)
float3 ColorBlendAddGlow(float3 colorSource, float3 colorDestination) {
    return colorSource + colorDestination;
}

// darken
float3 ColorBlendDarken(float3 colorSource, float3 colorDestination) {
    return min(colorSource, colorDestination);
}

// multiply
float3 ColorBlendMultiply(float3 colorSource, float3 colorDestination) {
    return colorSource * colorDestination;
}

// color burn
float ColorBurn(float colorSource, float colorDestination) {
    if (abs(colorDestination - 1.0f) < 0.000001f)
    {
        return 1.0f;
    }
    else if (abs(colorSource) < 0.000001f)
    {
        return 0.0f;
    }

    return 1.0f - min(1.0f, (1.0f - colorDestination) / colorSource);
}

float3 ColorBlendColorBurn(float3 colorSource, float3 colorDestination) {
    return float3(
        ColorBurn(colorSource.r, colorDestination.r),
        ColorBurn(colorSource.g, colorDestination.g),
        ColorBurn(colorSource.b, colorDestination.b)
    );
}

// linear burn
float3 ColorBlendLinearBurn(float3 colorSource, float3 colorDestination) {
    return max(float3(0.0f, 0.0f, 0.0f), colorSource + colorDestination - 1.0f);
}

// lighten
float3 ColorBlendLighten(float3 colorSource, float3 colorDestination) {
    return max(colorSource, colorDestination);
}

// screen
float3 ColorBlendScreen(float3 colorSource, float3 colorDestination) {
    return colorSource + colorDestination - colorSource * colorDestination;
}

// color dodge
float ColorDodge(float colorSource, float colorDestination) {
    if (colorDestination <= 0.0f)
    {
        return 0.0f;
    }
    else if (colorSource == 1.0f)
    {
        return 1.0f;
    }

    return min(1.0f, colorDestination / (1.0f - colorSource));
}

float3 ColorBlendColorDodge(float3 colorSource, float3 colorDestination) {
    return float3(
        ColorDodge(colorSource.r, colorDestination.r),
        ColorDodge(colorSource.g, colorDestination.g),
        ColorDodge(colorSource.b, colorDestination.b)
    );
}

// overlay
float Overlay(float colorSource, float colorDestination) {
    float mul = 2.0f * colorSource * colorDestination;
    float scr = 1.0f - 2.0f * (1.0f - colorSource) * (1.0f - colorDestination);
    return (colorDestination < 0.5f) ? mul : scr;
}

float3 ColorBlendOverlay(float3 colorSource, float3 colorDestination) {
    return float3(
        Overlay(colorSource.r, colorDestination.r),
        Overlay(colorSource.g, colorDestination.g),
        Overlay(colorSource.b, colorDestination.b)
    );
}

// soft light
float SoftLight(float colorSource, float colorDestination) {
    float val1 = colorDestination - (1.0f - 2.0f * colorSource) * colorDestination * (1.0f - colorDestination);
    float val2 = colorDestination + (2.0f * colorSource - 1.0f) * colorDestination * ((16.0f * colorDestination - 12.0f) * colorDestination + 3.0f);
    float val3 = colorDestination + (2.0f * colorSource - 1.0f) * (sqrt(colorDestination) - colorDestination);

    if (colorSource <= 0.5f)
    {
        return val1;
    }
    else if (colorDestination <= 0.25f)
    {
        return val2;
    }
    return val3;
}

float3 ColorBlendSoftLight(float3 colorSource, float3 colorDestination) {
    return float3(
        SoftLight(colorSource.r, colorDestination.r),
        SoftLight(colorSource.g, colorDestination.g),
        SoftLight(colorSource.b, colorDestination.b)
    );
}

// hard light
float HardLight(float colorSource, float colorDestination) {
    float mul = 2.0f * colorSource * colorDestination;
    float scr = 1.0f - 2.0f * (1.0f - colorSource) * (1.0f - colorDestination);

    return (colorSource < 0.5f) ? mul : scr;
}

float3 ColorBlendHardLight(float3 colorSource, float3 colorDestination) {
    return float3(
        HardLight(colorSource.r, colorDestination.r),
        HardLight(colorSource.g, colorDestination.g),
        HardLight(colorSource.b, colorDestination.b)
    );
}

// linear light
float LinearLight(float colorSource, float colorDestination) {
    float burn  = max(0.0f, 2.0f * colorSource + colorDestination - 1.0f);
    float dodge = min(1.0f, 2.0f * (colorSource - 0.5f) + colorDestination);

    return (colorSource < 0.5f) ? burn : dodge;
}

float3 ColorBlendLinearLight(float3 colorSource, float3 colorDestination) {
    return float3(
        LinearLight(colorSource.r, colorDestination.r),
        LinearLight(colorSource.g, colorDestination.g),
        LinearLight(colorSource.b, colorDestination.b)
    );
}

// hue color
static const float rCoeff = 0.30f;
static const float gCoeff = 0.59f;
static const float bCoeff = 0.11f;

float GetMaxC(in float3 rgbC) {
    return max(rgbC.r, max(rgbC.g, rgbC.b));
}

float GetMinC(in float3 rgbC) {
    return min(rgbC.r, min(rgbC.g, rgbC.b));
}

float GetRange(in float3 rgbC) {
    return GetMaxC(rgbC) - GetMinC(rgbC);
}

float Saturation(in float3 rgbC) {
    return GetRange(rgbC);
}

float Luma(in float3 rgbC) {
    return rCoeff * rgbC.r + gCoeff * rgbC.g + bCoeff * rgbC.b;
}

float3 ClipColor(in float3 rgbC) {
    float luma = Luma(rgbC);
    float maxv = GetMaxC(rgbC);
    float minv = GetMinC(rgbC);

    float3 outputColor = rgbC;
    if (minv < 0.0f)
    {
        outputColor = luma + (outputColor - luma) * (luma / (luma - minv));
    }
    if (maxv > 1.0f)
    {
        outputColor = luma + (outputColor - luma) * ((1.0f - luma) / (maxv - luma));
    }
    return outputColor;
}

float3 SetLuma(in float3 rgbC, float luma) {
    return ClipColor(rgbC + (luma - Luma(rgbC)));
}

float3 SetSaturation(in float3 rgbC, float saturation) {
    float maxv = GetMaxC(rgbC);
    float minv = GetMinC(rgbC);
    float medv = rgbC.r + rgbC.g + rgbC.b - maxv - minv;
    float outputMax, outputMed, outputMin;

    outputMax = (minv < maxv) ? saturation : 0.0f;
    outputMed = (minv < maxv) ? (medv - minv) * saturation / (maxv - minv) : 0.0f;
    outputMin = 0.0f;

    if(rgbC.r == maxv)
    {
        if(rgbC.b < rgbC.g)
            return float3(outputMax, outputMed, outputMin);
        else
            return float3(outputMax, outputMin, outputMed);
    }
    else if(rgbC.g == maxv)
    {
        if(rgbC.r < rgbC.b)
            return float3(outputMin, outputMax, outputMed);
        else
            return float3(outputMed, outputMax, outputMin);
    }
    else
    {
        if(rgbC.g < rgbC.r)
            return float3(outputMed, outputMin, outputMax);
        else
            return float3(outputMin, outputMed, outputMax);
    }
}

// hue
float3 ColorBlendHue(float3 colorSource, float3 colorDestination) {
    return SetLuma(SetSaturation(colorSource, Saturation(colorDestination)), Luma(colorDestination));
}

// color
float3 ColorBlendColor(float3 colorSource, float3 colorDestination) {
    return SetLuma(colorSource, Luma(colorDestination));
}

// Alpha blend
float4 OverlapRgba(float3 color, float3 colorSource, float3 colorDestination, float3 parameter) {
    float3 rgb   = color * parameter.x + colorSource * parameter.y + colorDestination * parameter.z;
    float  alpha = parameter.x + parameter.y + parameter.z;
    return float4(rgb, alpha);
}

// over
float4 AlphaBlendOver(float3 color, float4 colorSource, float4 colorDestination) {
    float3 parameter = float3(colorSource.a * colorDestination.a,
                              colorSource.a * (1.0f - colorDestination.a),
                              colorDestination.a * (1.0f - colorSource.a));
    return OverlapRgba(color, colorSource.rgb, colorDestination.rgb, parameter);
}

// atop
float4 AlphaBlendAtop(float3 color, float4 colorSource, float4 colorDestination) {
    float3 parameter = float3(colorSource.a * colorDestination.a,
                              0.0f,
                              colorDestination.a * (1.0f - colorSource.a));
    return OverlapRgba(color, colorSource.rgb, colorDestination.rgb, parameter);
}

// out
float4 AlphaBlendOut(float3 color, float4 colorSource, float4 colorDestination) {
    float3 parameter = float3(0.0f,
                              0.0f,
                              colorDestination.a * (1.0f - colorSource.a));
    return OverlapRgba(color, colorSource.rgb, colorDestination.rgb, parameter);
}

// conjoint over
float4 AlphaBlendConjointOver(float3 color, float4 colorSource, float4 colorDestination) {
    float3 parameter = float3(min(colorSource.a, colorDestination.a),
                              max(colorSource.a - colorDestination.a, 0.0f),
                              max(colorDestination.a - colorSource.a, 0.0f));
    return OverlapRgba(color, colorSource.rgb, colorDestination.rgb, parameter);
}

// disjoint over
float4 AlphaBlendDisjointOver(float3 color, float4 colorSource, float4 colorDestination) {
    float3 parameter = float3(max(colorSource.a + colorDestination.a - 1.0f, 0.0f),
                              min(colorSource.a, 1.0f - colorDestination.a),
                              min(colorDestination.a, 1.0f - colorSource.a));
    return OverlapRgba(color, colorSource.rgb, colorDestination.rgb, parameter);
}

// Vertex Shader
// normal
VS_OUT VertNormalBlend(VS_IN In) {
    VS_OUT Out = VertNormal(In);
    Out.blendUv = (Out.position.xy / Out.position.w) * 0.5 + 0.5;
    Out.blendUv.y = 1.0f - Out.blendUv.y;
    return Out;
}

// masked
VS_OUT VertMaskedBlend(VS_IN In) {
    VS_OUT Out = VertMasked(In);
    Out.blendUv = (Out.position.xy / Out.position.w) * 0.5 + 0.5;
    Out.blendUv.y = 1.0f - Out.blendUv.y;
    return Out;
}

// Pixel Shader
// normal
ColorInfo GetNormalColorInfo(VS_OUT In) {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
    ColorInfo color;
    color.source = texColor * baseColor;
    color.destination = ConvertPremultipliedToStraight(blendTexture.Sample(mainSampler, In.blendUv));
    return color;
}

// normal premult alpha
ColorInfo GetNormalPremultColorInfo(VS_OUT In) {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) - (texColor.rgb * screenColor.rgb);
    ColorInfo color;
    color.source = ConvertPremultipliedToStraight(texColor * baseColor);
    color.destination = ConvertPremultipliedToStraight(blendTexture.Sample(mainSampler, In.blendUv));
    return color;
}

// masked
ColorInfo GetMaskedColorInfo(VS_OUT In) {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
    ColorInfo color;
    color.source = texColor * baseColor;
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - maskTexture.Sample(mainSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color.source = color.source * maskVal;
    color.destination = ConvertPremultipliedToStraight(blendTexture.Sample(mainSampler, In.blendUv));
    return color;
}

// masked inverted
ColorInfo GetMaskedInvertedColorInfo(VS_OUT In) {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
    ColorInfo color;
    color.source = texColor * baseColor;
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - maskTexture.Sample(mainSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color.source = color.source * (1.0f - maskVal);
    color.destination = ConvertPremultipliedToStraight(blendTexture.Sample(mainSampler, In.blendUv));
    return color;
}

// masked premult alpha
ColorInfo GetMaskedPremultColorInfo(VS_OUT In) {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) - (texColor.rgb * screenColor.rgb);
    ColorInfo color;
    color.source = ConvertPremultipliedToStraight(texColor * baseColor);
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - maskTexture.Sample(mainSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color.source = color.source * maskVal;
    color.destination = ConvertPremultipliedToStraight(blendTexture.Sample(mainSampler, In.blendUv));
    return color;
}

// masked inverted premult alpha
ColorInfo GetMaskedInvertedPremultColorInfo(VS_OUT In) {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) - (texColor.rgb * screenColor.rgb);
    ColorInfo color;
    color.source = ConvertPremultipliedToStraight(texColor * baseColor);
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - maskTexture.Sample(mainSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color.source = color.source * (1.0f - maskVal);
    color.destination = ConvertPremultipliedToStraight(blendTexture.Sample(mainSampler, In.blendUv));
    return color;
}

#define CSM_CREATE_PIXEL_FUNC(MASK, COLOR, ALPHA) \
float4 Pixel ## MASK ## Blend ## COLOR ## ALPHA(VS_OUT In) : SV_Target { \
    ColorInfo color = Get ## MASK ## ColorInfo(In); \
    float3 colorBlend = ColorBlend ## COLOR(color.source.rgb, color.destination.rgb); \
    return AlphaBlend ## ALPHA(colorBlend, color.source, color.destination); \
}

#define CSM_CREATE_BLEND_MODE(COLOR, ALPHA) \
CSM_CREATE_PIXEL_FUNC(Normal, COLOR, ALPHA) \
\
CSM_CREATE_PIXEL_FUNC(NormalPremult, COLOR, ALPHA) \
\
CSM_CREATE_PIXEL_FUNC(Masked, COLOR, ALPHA) \
\
CSM_CREATE_PIXEL_FUNC(MaskedInverted, COLOR, ALPHA) \
\
CSM_CREATE_PIXEL_FUNC(MaskedPremult, COLOR, ALPHA) \
\
CSM_CREATE_PIXEL_FUNC(MaskedInvertedPremult, COLOR, ALPHA)

#define CSM_CREATE_BLEND_OVERLAP(COLOR) \
CSM_CREATE_BLEND_MODE(COLOR, Over) \
\
CSM_CREATE_BLEND_MODE(COLOR, Atop) \
\
CSM_CREATE_BLEND_MODE(COLOR, Out) \
\
CSM_CREATE_BLEND_MODE(COLOR, ConjointOver) \
\
CSM_CREATE_BLEND_MODE(COLOR, DisjointOver)

// normal
CSM_CREATE_PIXEL_FUNC(Masked, Normal, Over)

CSM_CREATE_PIXEL_FUNC(MaskedInverted, Normal, Over)

CSM_CREATE_PIXEL_FUNC(NormalPremult, Normal, Over)

CSM_CREATE_PIXEL_FUNC(MaskedPremult, Normal, Over)

CSM_CREATE_PIXEL_FUNC(MaskedInvertedPremult, Normal, Over)

CSM_CREATE_BLEND_MODE(Normal, Atop)

CSM_CREATE_BLEND_MODE(Normal, Out)

CSM_CREATE_BLEND_MODE(Normal, ConjointOver)

CSM_CREATE_BLEND_MODE(Normal, DisjointOver)

// add
CSM_CREATE_BLEND_OVERLAP(Add)

// add(glow)
CSM_CREATE_BLEND_OVERLAP(AddGlow)

// darken
CSM_CREATE_BLEND_OVERLAP(Darken)

// multiply
CSM_CREATE_BLEND_OVERLAP(Multiply)

// color burn
CSM_CREATE_BLEND_OVERLAP(ColorBurn)

// linear burn
CSM_CREATE_BLEND_OVERLAP(LinearBurn)

// lighten
CSM_CREATE_BLEND_OVERLAP(Lighten)

// screen
CSM_CREATE_BLEND_OVERLAP(Screen)

// color dodge
CSM_CREATE_BLEND_OVERLAP(ColorDodge)

// overlay
CSM_CREATE_BLEND_OVERLAP(Overlay)

// soft light
CSM_CREATE_BLEND_OVERLAP(SoftLight)

// hard light
CSM_CREATE_BLEND_OVERLAP(HardLight)

// linear light
CSM_CREATE_BLEND_OVERLAP(LinearLight)

// hue
CSM_CREATE_BLEND_OVERLAP(Hue)

// color
CSM_CREATE_BLEND_OVERLAP(Color)

#undef CSM_CREATE_BLEND_OVERLAP
#undef CSM_CREATE_BLEND_MODE
#undef CSM_CREATE_PIXEL_FUNC
