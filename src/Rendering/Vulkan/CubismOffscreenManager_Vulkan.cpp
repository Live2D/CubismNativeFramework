/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismOffscreenManager_Vulkan.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

CubismOffscreenManager_Vulkan::CubismOffscreenManager_Vulkan()
{
}

CubismOffscreenManager_Vulkan::~CubismOffscreenManager_Vulkan()
{
}

CubismRenderTarget_Vulkan* CubismOffscreenManager_Vulkan::GetOffscreenRenderTarget(
    VkDevice device, VkPhysicalDevice physicalDevice,
    csmUint32 displayBufferWidth, csmUint32 displayBufferHeight,
    VkFormat surfaceFormat, VkFormat depthFormat)
{
    // 使用数を更新
    UpdateRenderTargetCount();

    // 使われていないリソースコンテナがあればそれを返す
    CubismRenderTarget_Vulkan* offscreenRenderTarget = GetUnusedOffscreenRenderTarget();
    if (offscreenRenderTarget != nullptr)
    {
        // サイズが違う場合は再作成する
        if (offscreenRenderTarget->GetBufferWidth() != displayBufferWidth ||
            offscreenRenderTarget->GetBufferHeight() != displayBufferHeight)
        {
            offscreenRenderTarget->DestroyRenderTarget();
            offscreenRenderTarget->CreateRenderTarget(device, physicalDevice,
                displayBufferWidth, displayBufferHeight, surfaceFormat, depthFormat);
        }
        // 既存の未使用レンダーターゲットを返す
        return offscreenRenderTarget;
    }

    // 新規にレンダーターゲットを作成して登録する
    offscreenRenderTarget = CreateOffscreenRenderTarget();
    offscreenRenderTarget->CreateRenderTarget(device, physicalDevice,
        displayBufferWidth, displayBufferHeight, surfaceFormat, depthFormat);

    return offscreenRenderTarget;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
