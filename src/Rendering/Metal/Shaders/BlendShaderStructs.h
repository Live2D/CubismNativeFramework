/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once
#ifndef _BlendShaderStructs
#define _BlendShaderStructs

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct NormalBlendRasterizerData
{
    float4 position [[ position ]];
    float2 texCoord; //v2f.texcoord
    float2 blendCoord;
};

struct MaskedBlendRasterizerData
{
    float4 position [[ position ]];
    float2 texCoord; //v2f.texcoord
    float2 blendCoord;
    float4 myPos;
};
#endif
