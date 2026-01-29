/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "../ICubismOffscreenManager.hpp"
#include "CubismRenderTarget_OpenGLES2.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @brief  オフスクリーン用のレンダーターゲットを管理するクラス
 */
class CubismOffscreenManager_OpenGLES2 : public ICubismOffscreenManager<CubismRenderTarget_OpenGLES2>
{
public:
    /**
    * @brief   クラスのインスタンス（シングルトン）を返す。
    *           インスタンスが生成されていない場合は内部でインスタンを生成する。
    *
    * @return  クラスのインスタンス
    */
    static CubismOffscreenManager_OpenGLES2* GetInstance();

    /**
    * @brief   クラスのインスタンス（シングルトン）を解放する。
    *
    */
    static void ReleaseInstance();

    /**
     * @brief 使用可能なレンダーターゲットの取得
     *
     *  @param  width  幅
     *  @param  height 高さ
     *
     * @return 使用可能なレンダーターゲットを返す
     */
    CubismRenderTarget_OpenGLES2* GetOffscreenRenderTarget(csmUint32 width, csmUint32 height);

private:
    /**
     * @brief   コンストラクタ
     */
    CubismOffscreenManager_OpenGLES2();

    /**
     * @brief   デストラクタ
     */
    ~CubismOffscreenManager_OpenGLES2();

};

}}}}

//------------ LIVE2D NAMESPACE ------------
