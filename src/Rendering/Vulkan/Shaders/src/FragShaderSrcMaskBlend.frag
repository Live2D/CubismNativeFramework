/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#version 460
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"
#include "FragShaderSrcColorBlend.glsl"
#include "FragShaderSrcAlphaBlend.glsl"

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in vec2 v_blendCoord;
layout(location = 2) in vec4 v_clipPos;

layout(location = 0) out vec4 outColor;

vec4 ConvertPremultipliedToStraight(vec4 source);
vec3 ColorBlend(vec3 colorSource, vec3 colorDestination);
vec4 AlphaBlend(vec3 color, vec4 colorSource, vec4 colorDestination);

void main()
{
    vec4 texColor = texture(s_texture0, v_texCoord);
    texColor.rgb = texColor.rgb * ubo.u_multiplyColor.rgb;
    texColor.rgb = texColor.rgb + ubo.u_screenColor.rgb - (texColor.rgb * ubo.u_screenColor.rgb);
    vec4 col_formask = texColor * ubo.u_baseColor;
    vec4 clipMask = (1.0 - texture(s_texture1, v_clipPos.xy / v_clipPos.w)) * ubo.u_channelFlag;
    float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;
    vec4 colorSource = col_formask * maskVal;
    vec4 colorDestination = ConvertPremultipliedToStraight(texture(s_blendTexture, v_blendCoord));
    outColor = AlphaBlend(ColorBlend(colorSource.rgb, colorDestination.rgb), colorSource, colorDestination);
}
