/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismOffscreenRenderTarget_D3D9.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

CubismOffscreenRenderTarget_D3D9::CubismOffscreenRenderTarget_D3D9()
{
}

CubismOffscreenRenderTarget_D3D9::~CubismOffscreenRenderTarget_D3D9()
{
}

void CubismOffscreenRenderTarget_D3D9::SetOffscreenRenderTarget(LPDIRECT3DDEVICE9 device, CubismOffscreenManager_D3D9* offscreenManager, csmUint32 width, csmUint32 height)
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

csmBool CubismOffscreenRenderTarget_D3D9::GetUsingRenderTextureState(CubismOffscreenManager_D3D9* offscreenManager) const
{
    return offscreenManager->GetUsingRenderTextureState(_renderTarget);
}

void CubismOffscreenRenderTarget_D3D9::StopUsingRenderTexture(CubismOffscreenManager_D3D9* offscreenManager)
{
    offscreenManager->StopUsingRenderTexture(_renderTarget);
    _renderTarget = nullptr;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
