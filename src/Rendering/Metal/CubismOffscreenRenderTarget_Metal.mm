/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismOffscreenRenderTarget_Metal.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

CubismOffscreenRenderTarget_Metal::CubismOffscreenRenderTarget_Metal()
{
}

CubismOffscreenRenderTarget_Metal::~CubismOffscreenRenderTarget_Metal()
{
}

void CubismOffscreenRenderTarget_Metal::SetOffscreenRenderTarget(id<MTLDevice> device, CubismOffscreenManager_Metal* offscreenManager, csmUint32 width, csmUint32 height)
{
    if (GetUsingRenderTextureState(offscreenManager))
    {
        // 使用中の場合はサイズだけ確認
        if (_renderTarget->GetBufferWidth() != width || _renderTarget->GetBufferHeight() != height)
        {
            _renderTarget->CreateRenderTarget(device, width, height);
        }
        return;
    }

    _renderTarget = offscreenManager->GetOffscreenRenderTarget(device, width, height);
}

csmBool CubismOffscreenRenderTarget_Metal::GetUsingRenderTextureState(CubismOffscreenManager_Metal* offscreenManager) const
{
    return offscreenManager->GetUsingRenderTextureState(_renderTarget);
}

void CubismOffscreenRenderTarget_Metal::StopUsingRenderTexture(CubismOffscreenManager_Metal* offscreenManager)
{
    offscreenManager->StopUsingRenderTexture(_renderTarget);
    _renderTarget = nullptr;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
