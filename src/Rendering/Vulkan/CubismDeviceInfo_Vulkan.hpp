/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismRenderer_Vulkan.hpp"
#include "CubismOffscreenManager_Vulkan.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @brief デバイスに紐づいているデータの使用箇所の数とデータを管理するクラス
 */
class CubismDeviceInfo_Vulkan
{
public:
    /**
     * @brief   デバイスに紐づいているデータを取得する
     *
     * @param[in]   device         -> 使用デバイス
     *
     * @return デバイスに紐づいているデータを返す
     */
    static CubismDeviceInfo_Vulkan* GetDeviceInfo(VkDevice device);

    /**
     * @brief   デバイスに紐づいているデータの使用を開放する
     *
     * @param[in]   device         -> 使用デバイス
     */
    static void ReleaseDeviceInfo(VkDevice device);

    /**
     * @brief   デバイスに紐づいているデータの使用を全て開放する
     */
    static void ReleaseAllDeviceInfo();

    /**
     * @brief   コンストラクタ
     */
    CubismDeviceInfo_Vulkan();

    /**
     * @brief   デストラクタ
     */
    ~CubismDeviceInfo_Vulkan();

    /**
     * @brief   パイプラインを取得する
     *
     * @return パイプラインを返す
     */
    CubismPipeline_Vulkan* GetPipeline();

    /**
     * @brief   オフスクリーンマネージャーを取得する
     *
     * @return オフスクリーンマネージャーを返す
     */
    CubismOffscreenManager_Vulkan* GetOffscreenManager();

private:
    CubismPipeline_Vulkan _pipeline; ///< パイプライン
    CubismOffscreenManager_Vulkan _offscreenManager; ///< オフスクリーンマネージャー
};

}}}}

//------------ LIVE2D NAMESPACE ------------
