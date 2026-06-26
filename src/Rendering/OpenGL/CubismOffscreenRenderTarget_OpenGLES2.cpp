/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismOffscreenRenderTarget_OpenGLES2.hpp"
#include "CubismOffscreenManager_OpenGLES2.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

CubismOffscreenRenderTarget_OpenGLES2::CubismOffscreenRenderTarget_OpenGLES2()
{
}

CubismOffscreenRenderTarget_OpenGLES2::~CubismOffscreenRenderTarget_OpenGLES2()
{
}

void CubismOffscreenRenderTarget_OpenGLES2::SetOffscreenRenderTarget(csmUint32 width, csmUint32 height)
{
    if (GetUsingRenderTextureState())
    {
        // 使用中の場合はサイズだけ確認
        if (_renderTarget->GetBufferWidth() != width || _renderTarget->GetBufferHeight() != height)
        {
            _renderTarget->CreateRenderTarget(width, height);
        }
        return;
    }

    _renderTarget = CubismOffscreenManager_OpenGLES2::GetInstance()->GetOffscreenRenderTarget(width, height);
}

csmBool CubismOffscreenRenderTarget_OpenGLES2::GetUsingRenderTextureState() const
{
    return CubismOffscreenManager_OpenGLES2::GetInstance()->GetUsingRenderTextureState(_renderTarget);
}

void CubismOffscreenRenderTarget_OpenGLES2::StopUsingRenderTexture()
{
    CubismOffscreenManager_OpenGLES2::GetInstance()->StopUsingRenderTexture(_renderTarget);
    _renderTarget = nullptr;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
