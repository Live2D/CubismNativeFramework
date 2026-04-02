/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismDeviceInfo_Vulkan.hpp"
#include "Type/csmMap.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

namespace {
    csmMap<VkDevice, CubismDeviceInfo_Vulkan> s_deviceInfoList; ///< デバイスに紐づいているデータの管理
}

CubismDeviceInfo_Vulkan* CubismDeviceInfo_Vulkan::GetDeviceInfo(VkDevice device)
{
    return &s_deviceInfoList[device];
}

void CubismDeviceInfo_Vulkan::ReleaseDeviceInfo(VkDevice device)
{
    s_deviceInfoList.Erase(device);
}

void CubismDeviceInfo_Vulkan::ReleaseAllDeviceInfo()
{
    s_deviceInfoList.Clear();
}

CubismDeviceInfo_Vulkan::CubismDeviceInfo_Vulkan()
{
}

CubismDeviceInfo_Vulkan::~CubismDeviceInfo_Vulkan()
{
}

CubismPipeline_Vulkan* CubismDeviceInfo_Vulkan::GetPipeline()
{
    return &_pipeline;
}

CubismOffscreenManager_Vulkan* CubismDeviceInfo_Vulkan::GetOffscreenManager()
{
    return &_offscreenManager;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
