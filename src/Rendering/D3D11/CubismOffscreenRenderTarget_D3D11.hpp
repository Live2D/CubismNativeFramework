/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "../ICubismOffscreenRenderTarget.hpp"
#include "CubismOffscreenManager_D3D11.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @brief  オフスクリーン用のレンダーターゲットを管理するクラス
 */
class CubismOffscreenRenderTarget_D3D11 : public ICubismOffscreenRenderTarget<CubismOffscreenRenderTarget_D3D11, CubismRenderTarget_D3D11>
{
public:
    /**
     * @brief   コンストラクタ
     */
    CubismOffscreenRenderTarget_D3D11();

    /**
     * @brief   デストラクタ
     */
    ~CubismOffscreenRenderTarget_D3D11();

    /**
     * @brief オフスクリーン描画用レンダーターゲットを設定する。
     *
     *  @param  device デバイス
     *  @param  offscreenManager オフスクリーンマネージャー
     *  @param  width  幅
     *  @param  height 高さ
     */
    void SetOffscreenRenderTarget(ID3D11Device* device, CubismOffscreenManager_D3D11* offscreenManager, csmUint32 width, csmUint32 height);

    /**
     * @brief レンダーターゲットの使用状態を取得する。
     *
     *  @param  offscreenManager オフスクリーンマネージャー
     *
     * @return 使用中はtrue、未使用の場合はfalseを返す。
     */
    csmBool GetUsingRenderTextureState(CubismOffscreenManager_D3D11* offscreenManager) const;

    /**
     * @brief オフスクリーン描画用レンダーターゲットの使用を終了する。
     *
     *  @param  offscreenManager オフスクリーンマネージャー
     */
    void StopUsingRenderTexture(CubismOffscreenManager_D3D11* offscreenManager);

};

}}}}

//------------ LIVE2D NAMESPACE ------------
