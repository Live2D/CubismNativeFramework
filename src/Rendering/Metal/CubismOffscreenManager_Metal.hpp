/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "../ICubismOffscreenManager.hpp"
#include "CubismRenderTarget_Metal.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @brief  オフスクリーン用のレンダーターゲットを管理するクラス
 */
class CubismOffscreenManager_Metal : public ICubismOffscreenManager<CubismRenderTarget_Metal>
{
public:
    /**
     * @brief   コンストラクタ
     */
    CubismOffscreenManager_Metal();

    /**
     * @brief   デストラクタ
     */
    ~CubismOffscreenManager_Metal();

    /**
     * @brief 使用可能なレンダーターゲットの取得
     *
     *  @param  device デバイス
     *  @param  width  幅
     *  @param  height 高さ
     *
     * @return 使用可能なレンダーターゲットを返す
     */
    CubismRenderTarget_Metal* GetOffscreenRenderTarget(id<MTLDevice> device, csmUint32 width, csmUint32 height);

};

}}}}

//------------ LIVE2D NAMESPACE ------------
