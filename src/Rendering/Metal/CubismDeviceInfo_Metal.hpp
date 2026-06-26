/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismShader_Metal.hpp"
#include "Rendering/Metal/CubismOffscreenRenderTarget_Metal.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @brief デバイスに紐づいているデータの使用箇所の数とデータを管理するクラス
 */
class CubismDeviceInfo_Metal
{
public:
    /**
     * @brief   デバイスに紐づいているデータを取得する
     *
     * @param[in]   device         -> 使用デバイス
     *
     * @return デバイスに紐づいているデータを返す
     */
    static CubismDeviceInfo_Metal* GetDeviceInfo(id<MTLDevice> device);

    /**
     * @brief   デバイスに紐づいているデータの使用を開放する
     *
     * @param[in]   device         -> 使用デバイス
     */
    static void ReleaseDeviceInfo(id<MTLDevice> device);

    /**
     * @brief   デバイスに紐づいているデータの使用を全て開放する
     */
    static void ReleaseAllDeviceInfo();

    /**
     * @brief   コンストラクタ
     */
    CubismDeviceInfo_Metal();

    /**
     * @brief   デストラクタ
     */
    ~CubismDeviceInfo_Metal();

    /**
     * @brief   シェーダーを取得する
     *
     * @return シェーダーを返す
     */
    CubismShader_Metal* GetShader();

    /**
     * @brief   オフスクリーンマネージャーを取得する
     *
     * @return オフスクリーンマネージャーを返す
     */
    CubismOffscreenManager_Metal* GetOffscreenManager();

private:
    CubismShader_Metal _shader; ///< シェーダー
    CubismOffscreenManager_Metal _offscreenManager; ///< オフスクリーンマネージャー
};

}}}}

//------------ LIVE2D NAMESPACE ------------
