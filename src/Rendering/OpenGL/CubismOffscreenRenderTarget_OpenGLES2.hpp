/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "../ICubismOffscreenRenderTarget.hpp"
#include "CubismRenderTarget_OpenGLES2.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @brief  オフスクリーン用のレンダーターゲットを管理するクラス
 */
class CubismOffscreenRenderTarget_OpenGLES2 : public ICubismOffscreenRenderTarget<CubismOffscreenRenderTarget_OpenGLES2, CubismRenderTarget_OpenGLES2>
{
public:
    /**
     * @brief   コンストラクタ
     */
    CubismOffscreenRenderTarget_OpenGLES2();

    /**
     * @brief   デストラクタ
     */
    ~CubismOffscreenRenderTarget_OpenGLES2();

    /**
     * @brief オフスクリーン描画用レンダーターゲットを設定する。
     *
     *  @param  width  幅
     *  @param  height 高さ
     */
    void SetOffscreenRenderTarget(csmUint32 width, csmUint32 height);

    /**
     * @brief レンダーターゲットの使用状態を取得する。
     *
     * @return 使用中はtrue、未使用の場合はfalseを返す。
     */
    csmBool GetUsingRenderTextureState() const;

    /**
     * @brief オフスクリーン描画用レンダーターゲットの使用を終了する。
     */
    void StopUsingRenderTexture();

};

}}}}

//------------ LIVE2D NAMESPACE ------------
