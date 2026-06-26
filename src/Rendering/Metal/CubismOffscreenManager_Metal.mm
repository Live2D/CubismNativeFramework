/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismOffscreenManager_Metal.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

CubismOffscreenManager_Metal::CubismOffscreenManager_Metal()
{
}

CubismOffscreenManager_Metal::~CubismOffscreenManager_Metal()
{
}

CubismRenderTarget_Metal* CubismOffscreenManager_Metal::GetOffscreenRenderTarget(id<MTLDevice> device, csmUint32 width, csmUint32 height)
{
    // 使用数を更新
    UpdateRenderTargetCount();

    // 使われていないリソースコンテナがあればそれを返す
    CubismRenderTarget_Metal* offscreenRenderTarget = GetUnusedOffscreenRenderTarget();
    if (offscreenRenderTarget != nullptr)
    {
        // サイズが違う場合は再作成する
        if (offscreenRenderTarget->GetBufferWidth() != width || offscreenRenderTarget->GetBufferHeight() != height)
        {
            offscreenRenderTarget->CreateRenderTarget(device, width, height);
        }
        // 既存の未使用レンダーターゲットを返す
        return offscreenRenderTarget;
    }

    // 新規にレンダーターゲットを作成して登録する
    offscreenRenderTarget = CreateOffscreenRenderTarget();
    offscreenRenderTarget->SetMTLPixelFormat(MTLPixelFormatBGRA8Unorm);
    offscreenRenderTarget->CreateRenderTarget(device, width, height);
    return offscreenRenderTarget;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
