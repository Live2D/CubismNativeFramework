/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include <metal_stdlib>
#include <simd/simd.h>
#include "BlendShaderStructs.h"
#include "BlendShaderFuncs.h"

using namespace metal;

// Include header shared between this Metal shader code and C code executing Metal API commands
#include "MetalShaderTypes.h"

fragment float4
FragShaderSrcMaskInvertedBlend(MaskedBlendRasterizerData in [[stage_in]],
                    texture2d<float> texture0 [[ texture(0) ]],
                    texture2d<float> texture1 [[ texture(1) ]],
                    constant CubismShaderUniforms &uniforms  [[ buffer(MetalVertexInputIndexUniforms) ]],
                    sampler smp [[sampler(0)]],
                    texture2d<float> blendTexture [[ texture(2) ]])
{
    float4 texColor = texture0.sample(smp, in.texCoord);
    texColor.rgb = texColor.rgb * uniforms.multiplyColor.rgb;
    texColor.rgb = texColor.rgb + uniforms.screenColor.rgb - (texColor.rgb * uniforms.screenColor.rgb);
    float4 col_formask = texColor * uniforms.baseColor;
    col_formask.rgb = col_formask.rgb  * col_formask.a ;
    float4 clipMask = (1.0 - texture1.sample(smp, in.myPos.xy / in.myPos.w)) * uniforms.channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    col_formask = col_formask * (1.0 - maskVal);
    float4 colorSource = col_formask;
    float4 colorDestination = ConvertPremultipliedToStraight(blendTexture.sample(smp, in.blendCoord));
    float4 outColor = AlphaBlend(ColorBlend(colorSource.rgb, colorDestination.rgb), colorSource, colorDestination);

    return outColor;
}
