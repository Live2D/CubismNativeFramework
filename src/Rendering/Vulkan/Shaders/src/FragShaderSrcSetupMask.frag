/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#version 460
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in vec4 v_myPos;
layout(location = 0) out vec4 outColor;

void main()
{
    float isInside = step(ubo.u_baseColor.x, v_myPos.x/v_myPos.w)*
    step(ubo.u_baseColor.y, v_myPos.y/v_myPos.w)*
    step(v_myPos.x/v_myPos.w, ubo.u_baseColor.z)*
    step(v_myPos.y/v_myPos.w, ubo.u_baseColor.w);

    outColor = ubo.u_channelFlag * texture(s_texture0 , v_texCoord).a * isInside;
}
