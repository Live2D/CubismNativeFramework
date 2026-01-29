/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismDeviceInfo_D3D11.hpp"
#include "Type/csmMap.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

namespace {
    csmMap<ID3D11Device*, CubismDeviceInfo_D3D11> s_deviceInfoList; ///< デバイスに紐づいているデータの管理
}

CubismDeviceInfo_D3D11* CubismDeviceInfo_D3D11::GetDeviceInfo(ID3D11Device* device)
{
    if (!s_deviceInfoList.IsExist(device))
    {
        s_deviceInfoList[device]._renderState = CSM_NEW CubismRenderState_D3D11(device);
    }
    return &s_deviceInfoList[device];
}

void CubismDeviceInfo_D3D11::ReleaseDeviceInfo(ID3D11Device* device)
{
    s_deviceInfoList.Erase(device);
}

void CubismDeviceInfo_D3D11::ReleaseAllDeviceInfo()
{
    s_deviceInfoList.Clear();
}

CubismDeviceInfo_D3D11::CubismDeviceInfo_D3D11()
    : _renderState(nullptr)
{
}

CubismDeviceInfo_D3D11::~CubismDeviceInfo_D3D11()
{
    CSM_DELETE_SELF(CubismRenderState_D3D11, _renderState);
}

CubismShader_D3D11* CubismDeviceInfo_D3D11::GetShader()
{
    return &_shader;
}

CubismRenderState_D3D11* CubismDeviceInfo_D3D11::GetRenderState()
{
    return _renderState;
}

CubismOffscreenManager_D3D11* CubismDeviceInfo_D3D11::GetOffscreenManager()
{
    return &_offscreenManager;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
