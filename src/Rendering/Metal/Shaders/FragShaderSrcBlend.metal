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
FragShaderSrcBlend(NormalBlendRasterizerData in [[stage_in]],
              texture2d<float> texture [[ texture(0) ]],
              constant CubismShaderUniforms &uniforms  [[ buffer(MetalVertexInputIndexUniforms) ]],
              sampler smp [[sampler(0)]],
              texture2d<float> blendTexture [[ texture(1) ]])
{
    float4 texColor = texture.sample(smp, in.texCoord);
    texColor.rgb = texColor.rgb * uniforms.multiplyColor.rgb;
    texColor.rgb = texColor.rgb + uniforms.screenColor.rgb - (texColor.rgb * uniforms.screenColor.rgb);
    float4 colorSource = texColor * uniforms.baseColor;
    float4 colorDestination = ConvertPremultipliedToStraight(blendTexture.sample(smp, in.blendCoord));
    float4 outColor = AlphaBlend(ColorBlend(colorSource.rgb, colorDestination.rgb), colorSource, colorDestination);

    return outColor;
}
