/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismDeviceInfo_D3D9.hpp"
#include "Type/csmMap.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

namespace {
    csmMap<LPDIRECT3DDEVICE9, CubismDeviceInfo_D3D9> s_deviceInfoList; ///< デバイスに紐づいているデータの管理
}

CubismDeviceInfo_D3D9* CubismDeviceInfo_D3D9::GetDeviceInfo(LPDIRECT3DDEVICE9 device)
{
    return &s_deviceInfoList[device];
}

void CubismDeviceInfo_D3D9::ReleaseDeviceInfo(LPDIRECT3DDEVICE9 device)
{
    s_deviceInfoList.Erase(device);
}

void CubismDeviceInfo_D3D9::ReleaseAllDeviceInfo()
{
    s_deviceInfoList.Clear();
}

void CubismDeviceInfo_D3D9::OnDeviceLost(LPDIRECT3DDEVICE9 device)
{
    if (s_deviceInfoList.IsExist(device))
    {
        s_deviceInfoList[device]._shader.ReleaseShaderProgram();
        s_deviceInfoList[device]._offscreenManager.ReleaseAllRenderTextures();
    }
}

CubismDeviceInfo_D3D9::CubismDeviceInfo_D3D9()
{
}

CubismDeviceInfo_D3D9::~CubismDeviceInfo_D3D9()
{
}

CubismShader_D3D9* CubismDeviceInfo_D3D9::GetShader()
{
    return &_shader;
}

CubismRenderState_D3D9* CubismDeviceInfo_D3D9::GetRenderState()
{
    return &_renderState;
}

CubismOffscreenManager_D3D9* CubismDeviceInfo_D3D9::GetOffscreenManager()
{
    return &_offscreenManager;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
