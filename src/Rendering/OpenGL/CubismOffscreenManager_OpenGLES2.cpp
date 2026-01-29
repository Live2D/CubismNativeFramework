/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismOffscreenManager_OpenGLES2.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

namespace {
    CubismOffscreenManager_OpenGLES2* s_instance = nullptr;
}

CubismOffscreenManager_OpenGLES2* CubismOffscreenManager_OpenGLES2::GetInstance()
{
    if (s_instance == NULL)
    {
        s_instance = new CubismOffscreenManager_OpenGLES2();
    }

    return s_instance;
}

void CubismOffscreenManager_OpenGLES2::ReleaseInstance()
{
    if (s_instance != NULL)
    {
        delete s_instance;
    }

    s_instance = NULL;
}

CubismRenderTarget_OpenGLES2* CubismOffscreenManager_OpenGLES2::GetOffscreenRenderTarget(csmUint32 width, csmUint32 height)
{
    // 使用数を更新
    UpdateRenderTargetCount();

    // 使われていないリソースコンテナがあればそれを返す
    CubismRenderTarget_OpenGLES2* offscreenRenderTarget = GetUnusedOffscreenRenderTarget();
    if (offscreenRenderTarget != nullptr)
    {
        // サイズが違う場合は再作成する
        if (offscreenRenderTarget->GetBufferWidth() != width || offscreenRenderTarget->GetBufferHeight() != height)
        {
            offscreenRenderTarget->CreateRenderTarget(width, height);
        }
        // 既存の未使用レンダーターゲットを返す
        return offscreenRenderTarget;
    }

    // 新規にレンダーターゲットを作成して登録する
    offscreenRenderTarget = CreateOffscreenRenderTarget();
    offscreenRenderTarget->CreateRenderTarget(width, height);
    return offscreenRenderTarget;
}

CubismOffscreenManager_OpenGLES2::CubismOffscreenManager_OpenGLES2()
{
}

CubismOffscreenManager_OpenGLES2::~CubismOffscreenManager_OpenGLES2()
{
}

}}}}

//------------ LIVE2D NAMESPACE ------------
