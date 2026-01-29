/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

cbuffer ConstantBuffer {
    float4x4 projectMatrix;
    float4x4 clipMatrix;
    float4 baseColor;
    float4 multiplyColor;
    float4 screenColor;
    float4 channelFlag;
}

SamplerState mainSampler : register(s0);
Texture2D mainTexture : register(t0);
Texture2D maskTexture : register(t1);
Texture2D blendTexture : register(t2);

// Vertex shader input
struct VS_IN {
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
};

// Vertex shader output
struct VS_OUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 clipPosition : TEXCOORD1;
    float2 blendUv : TEXCOORD2;
};


// Mask shader
VS_OUT VertSetupMask(VS_IN In) {
    VS_OUT Out = (VS_OUT)0;
    Out.position = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
    Out.clipPosition = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
    Out.uv.x = In.uv.x;
    Out.uv.y = 1.0f - In.uv.y;
    return Out;
}

float4 PixelSetupMask(VS_OUT In) : SV_Target {
    float isInside =
    step(baseColor.x, In.clipPosition.x / In.clipPosition.w)
    * step(baseColor.y, In.clipPosition.y / In.clipPosition.w)
    * step(In.clipPosition.x / In.clipPosition.w, baseColor.z)
    * step(In.clipPosition.y / In.clipPosition.w, baseColor.w);
    return channelFlag * mainTexture.Sample(mainSampler, In.uv).a * isInside;
}

// Vertex shader
// normal
VS_OUT VertNormal(VS_IN In) {
    VS_OUT Out = (VS_OUT)0;
    Out.position = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
    Out.uv.x = In.uv.x;
    Out.uv.y = 1.0f - In.uv.y;
    return Out;
}

// masked
VS_OUT VertMasked(VS_IN In) {
    VS_OUT Out = (VS_OUT)0;
    Out.position = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
    Out.clipPosition = mul(float4(In.pos, 0.0f, 1.0f), clipMatrix);
    Out.uv.x = In.uv.x;
    Out.uv.y = 1.0f - In.uv.y;
    return Out;
}

// Pixel Shader
// normal
float4 PixelNormal(VS_OUT In) : SV_Target {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    color.xyz *= color.w;
    return color;
}

// normal premult alpha
float4 PixelNormalPremult(VS_OUT In) : SV_Target {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    return color;
}

// masked
float4 PixelMasked(VS_OUT In) : SV_Target {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    color.xyz *= color.w;
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - maskTexture.Sample(mainSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color = color * maskVal;
    return color;
}

// masked inverted
float4 PixelMaskedInverted(VS_OUT In) : SV_Target {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    color.xyz *= color.w;
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - maskTexture.Sample(mainSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color = color * (1.0f - maskVal);
    return color;
}

// masked premult alpha
float4 PixelMaskedPremult(VS_OUT In) : SV_Target {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - maskTexture.Sample(mainSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color = color * maskVal;
    return color;
}

// masked inverted premult alpha
float4 PixelMaskedInvertedPremult(VS_OUT In) : SV_Target {
    float4 texColor = mainTexture.Sample(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - maskTexture.Sample(mainSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color = color * (1.0f - maskVal);
    return color;
}
