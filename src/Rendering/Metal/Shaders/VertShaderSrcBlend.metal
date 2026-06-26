/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "BlendShaderStructs.h"

using namespace metal;

// Include header shared between this Metal shader code and C code executing Metal API commands
#include "MetalShaderTypes.h"

vertex NormalBlendRasterizerData
VertShaderSrcBlend(uint vertexID [[ vertex_id ]],
            constant float2 *vertexArray [[ buffer(MetalVertexInputIndexVertices) ]],
            constant float2 *uvArray [[ buffer(MetalVertexInputUVs) ]],
            constant CubismShaderUniforms &uniforms  [[ buffer(MetalVertexInputIndexUniforms) ]])

{
    NormalBlendRasterizerData out;
    float2 vert = vertexArray[vertexID];
    float2 uv = uvArray[vertexID];

    float4 pos = float4(vert.x, vert.y, 0.0, 1.0);
    out.position = uniforms.matrix * pos;
    out.texCoord = uv;
    out.texCoord.y = 1.0 - out.texCoord.y;

    float2 ndcPos = out.position.xy / out.position.w;
    out.blendCoord = ndcPos * 0.5 + 0.5;
    out.blendCoord.y = 1.0 - out.blendCoord.y;

    return out;
}
