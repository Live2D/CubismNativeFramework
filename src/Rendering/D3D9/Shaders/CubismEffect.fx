/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

float4x4 projectMatrix;
float4x4 clipMatrix;
float4 baseColor;
float4 multiplyColor;
float4 screenColor;
float4 channelFlag;
texture mainTexture;
texture maskTexture;
float4 screenSize;

sampler mainSampler = sampler_state {
    texture = <mainTexture>;
};
sampler maskSampler = sampler_state {
    texture = <maskTexture>;
};

struct VS_IN {
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct VS_OUT {
    float4 position : POSITION0;
    float2 uv : TEXCOORD0;
    float4 clipPosition : TEXCOORD1;
    float2 blendUv : TEXCOORD2;
};

float2 AlignPixel(float2 uv) {
    if (screenSize.x > 0.0f && screenSize.y > 0.0f)
    {
        uv += float2(0.5f / screenSize.x, 0.5f / screenSize.y);
    }
    return uv;
}

// Setup mask shader
VS_OUT VertSetupMask(VS_IN In) {
    VS_OUT Out = (VS_OUT)0;
    Out.position = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
    Out.clipPosition = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
    Out.uv.x = In.uv.x;
    Out.uv.y = 1.0f - In.uv.y;
    Out.uv = AlignPixel(Out.uv);
    return Out;
}

float4 PixelSetupMask(VS_OUT In) : COLOR0 {
    float isInside =
    step(baseColor.x, In.clipPosition.x / In.clipPosition.w)
    * step(baseColor.y, In.clipPosition.y / In.clipPosition.w)
    * step(In.clipPosition.x / In.clipPosition.w, baseColor.z)
    * step(In.clipPosition.y / In.clipPosition.w, baseColor.w);
    return channelFlag * tex2D(mainSampler, In.uv).a * isInside;
}

// Vertex Shader
// normal
VS_OUT VertNormal(VS_IN In) {
    VS_OUT Out = (VS_OUT)0;
    Out.position = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
    Out.uv.x = In.uv.x;
    Out.uv.y = 1.0f - In.uv.y;
    Out.uv = AlignPixel(Out.uv);
    return Out;
}

// masked
VS_OUT VertMasked(VS_IN In) {
    VS_OUT Out = (VS_OUT)0;
    Out.position = mul(float4(In.pos, 0.0f, 1.0f), projectMatrix);
    Out.clipPosition = mul(float4(In.pos, 0.0f, 1.0f), clipMatrix);
    Out.uv.x = In.uv.x;
    Out.uv.y = 1.0f - In.uv.y;
    Out.uv = AlignPixel(Out.uv);
    return Out;
}

// Pixel Shader
// normal
float4 PixelNormal(VS_OUT In) : COLOR0 {
    float4 texColor = tex2D(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    color.xyz *= color.w;
    return color;
}

// normal premult alpha
float4 PixelNormalPremult(VS_OUT In) : COLOR0 {
    float4 texColor = tex2D(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    return color;
}

// masked
float4 PixelMasked(VS_OUT In) : COLOR0{
    float4 texColor = tex2D(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    color.xyz *= color.w;
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - tex2D(maskSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color = color * maskVal;
    return color;
}

// masked inverted
float4 PixelMaskedInverted(VS_OUT In) : COLOR0 {
    float4 texColor = tex2D(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    color.xyz *= color.w;
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - tex2D(maskSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color = color * (1.0f - maskVal);
    return color;
}

// masked premult alpha
float4 PixelMaskedPremult(VS_OUT In) : COLOR0 {
    float4 texColor = tex2D(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - tex2D(maskSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color = color * maskVal;
    return color;
}

// masked inverted premult alpha
float4 PixelMaskedInvertedPremult(VS_OUT In) : COLOR0 {
    float4 texColor = tex2D(mainSampler, In.uv);
    texColor.rgb = texColor.rgb * multiplyColor.rgb;
    texColor.rgb = (texColor.rgb + screenColor.rgb * texColor.a) - (texColor.rgb * screenColor.rgb);
    float4 color = texColor * baseColor;
    float2 maskUv = In.clipPosition.xy / In.clipPosition.w;
    maskUv.y = 1.0f + maskUv.y;
    float4 clipMask = (1.0f - tex2D(maskSampler, maskUv)) * channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    color = color * (1.0f - maskVal);
    return color;
}

// Technique
technique ShaderNames_SetupMask {
    pass p0 {
        VertexShader = compile vs_2_0 VertSetupMask();
        PixelShader = compile ps_2_0 PixelSetupMask();
    }
}

technique ShaderNames_Normal {
    pass p0 {
        VertexShader = compile vs_2_0 VertNormal();
        PixelShader = compile ps_2_0 PixelNormal();
    }
}

technique ShaderNames_Masked {
    pass p0 {
        VertexShader = compile vs_2_0 VertMasked();
        PixelShader = compile ps_2_0 PixelMasked();
    }
}

technique ShaderNames_MaskedInverted {
    pass p0 {
        VertexShader = compile vs_2_0 VertMasked();
        PixelShader = compile ps_2_0 PixelMaskedInverted();
    }
}

technique ShaderNames_NormalPremultipliedAlpha {
    pass p0 {
        VertexShader = compile vs_2_0 VertNormal();
        PixelShader = compile ps_2_0 PixelNormalPremult();
    }
}

technique ShaderNames_MaskedPremultipliedAlpha {
    pass p0 {
        VertexShader = compile vs_2_0 VertMasked();
        PixelShader = compile ps_2_0 PixelMaskedPremult();
    }
}

technique ShaderNames_MaskedInvertedPremultipliedAlpha {
    pass p0 {
        VertexShader = compile vs_2_0 VertMasked();
        PixelShader = compile ps_2_0 PixelMaskedInvertedPremult();
    }
}
