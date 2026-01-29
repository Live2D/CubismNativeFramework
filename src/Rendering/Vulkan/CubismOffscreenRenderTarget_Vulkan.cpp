/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismOffscreenRenderTarget_Vulkan.hpp"
#include "CubismOffscreenManager_Vulkan.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

CubismOffscreenRenderTarget_Vulkan::CubismOffscreenRenderTarget_Vulkan()
{
}

CubismOffscreenRenderTarget_Vulkan::~CubismOffscreenRenderTarget_Vulkan()
{
}

void CubismOffscreenRenderTarget_Vulkan::SetOffscreenRenderTarget(
    VkDevice device, VkPhysicalDevice physicalDevice,
    csmUint32 displayBufferWidth, csmUint32 displayBufferHeight,
    VkFormat surfaceFormat, VkFormat depthFormat)
{
    if (GetUsingRenderTextureState())
    {

        if (_renderTarget->GetBufferWidth() != displayBufferWidth ||
            _renderTarget->GetBufferHeight() != displayBufferHeight)
        {
            _renderTarget->DestroyRenderTarget();
            _renderTarget->CreateRenderTarget(device, physicalDevice,
                displayBufferWidth, displayBufferHeight, surfaceFormat, depthFormat);
        }
        return;
    }

    _renderTarget = CubismOffscreenManager_Vulkan::GetInstance()->GetOffscreenRenderTarget(
        device, physicalDevice, displayBufferWidth, displayBufferHeight, surfaceFormat, depthFormat);
}

csmBool CubismOffscreenRenderTarget_Vulkan::GetUsingRenderTextureState() const
{
    return CubismOffscreenManager_Vulkan::GetInstance()->GetUsingRenderTextureState(_renderTarget);
}

void CubismOffscreenRenderTarget_Vulkan::StopUsingRenderTexture()
{
    CubismOffscreenManager_Vulkan::GetInstance()->StopUsingRenderTexture(_renderTarget);
    _renderTarget = nullptr;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
