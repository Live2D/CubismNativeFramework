/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismShader_D3D11.hpp"
#include "CubismRenderState_D3D11.hpp"
#include "Rendering/D3D11/CubismOffscreenRenderTarget_D3D11.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @brief デバイスに紐づいているデータの使用箇所の数とデータを管理するクラス
 */
class CubismDeviceInfo_D3D11
{
public:
    /**
     * @brief   デバイスに紐づいているデータを取得する
     *
     * @param[in]   device         -> 使用デバイス
     *
     * @return デバイスに紐づいているデータを返す
     */
    static CubismDeviceInfo_D3D11* GetDeviceInfo(ID3D11Device* device);

    /**
     * @brief   デバイスに紐づいているデータの使用を開放する
     *
     * @param[in]   device         -> 使用デバイス
     */
    static void ReleaseDeviceInfo(ID3D11Device* device);

    /**
     * @brief   デバイスに紐づいているデータの使用を全て開放する
     */
    static void ReleaseAllDeviceInfo();

    /**
     * @brief   コンストラクタ
     */
    CubismDeviceInfo_D3D11();

    /**
     * @brief   デストラクタ
     */
    ~CubismDeviceInfo_D3D11();

    /**
     * @brief   シェーダーを取得する
     *
     * @return シェーダーを返す
     */
    CubismShader_D3D11* GetShader();

    /**
     * @brief   レンダーステートを取得する
     *
     * @return レンダーステートを返す
     */
    CubismRenderState_D3D11* GetRenderState();

    /**
     * @brief   オフスクリーンマネージャーを取得する
     *
     * @return オフスクリーンマネージャーを返す
     */
    CubismOffscreenManager_D3D11* GetOffscreenManager();

private:
    CubismShader_D3D11 _shader; ///< シェーダー
    CubismRenderState_D3D11* _renderState;   ///< レンダーステート
    CubismOffscreenManager_D3D11 _offscreenManager; ///< オフスクリーンマネージャー
};

}}}}

//------------ LIVE2D NAMESPACE ------------
