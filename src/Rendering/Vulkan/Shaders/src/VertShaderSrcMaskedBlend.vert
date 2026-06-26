/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#version 460
#extension GL_GOOGLE_include_directive : enable
#include "common.glsl"

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec2 a_texCoord;

layout(location = 0) out vec2 v_texCoord;
layout(location = 1) out vec2 v_blendCoord;
layout(location = 2) out vec4 v_clipPos;

void main()
{
    vec4 pos = ubo.u_matrix * a_position;
    pos.y = -pos.y;
    v_clipPos = ubo.u_clipMatrix * a_position;
    v_clipPos.y = 1.0 - v_clipPos.y;
    v_texCoord = a_texCoord;
    v_texCoord.y = 1.0 - v_texCoord.y;
    vec2 ndcPos = pos.xy / pos.w;
    gl_Position = pos;
    v_blendCoord = ndcPos * 0.5 + 0.5;
}
