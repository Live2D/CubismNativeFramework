/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismDeviceInfo_Metal.hpp"
#include "Type/csmMap.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

namespace {
    csmMap<id<MTLDevice>, CubismDeviceInfo_Metal> s_deviceInfoList; ///< デバイスに紐づいているデータの管理
}

CubismDeviceInfo_Metal* CubismDeviceInfo_Metal::GetDeviceInfo(id<MTLDevice> device)
{
    return &s_deviceInfoList[device];
}

void CubismDeviceInfo_Metal::ReleaseDeviceInfo(id<MTLDevice> device)
{
    s_deviceInfoList.Erase(device);
}

void CubismDeviceInfo_Metal::ReleaseAllDeviceInfo()
{
    s_deviceInfoList.Clear();
}

CubismDeviceInfo_Metal::CubismDeviceInfo_Metal()
{
}

CubismDeviceInfo_Metal::~CubismDeviceInfo_Metal()
{
}

CubismShader_Metal* CubismDeviceInfo_Metal::GetShader()
{
    return &_shader;
}

CubismOffscreenManager_Metal* CubismDeviceInfo_Metal::GetOffscreenManager()
{
    return &_offscreenManager;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
