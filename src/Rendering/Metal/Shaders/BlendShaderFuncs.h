/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "FragShaderSrcAlphaBlend.metal"
#include "FragShaderSrcColorBlend.metal"

float4 AlphaBlend(float3 color, float4 colorSource, float4 colorDestination);
float3 ColorBlend(float3 colorSource, float3 colorDestination);
float4 ConvertPremultipliedToStraight(float4 source);
