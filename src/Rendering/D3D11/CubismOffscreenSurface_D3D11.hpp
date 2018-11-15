/*
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at http://live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismNativeInclude_D3D11.hpp"

#include "Math/CubismMatrix44.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {


/**
 * @brief  オフスクリーン描画用構造体
 */
struct CubismOffscreenFrame_D3D11
{
public:

    CubismOffscreenFrame_D3D11();

    /**
     * @brief   指定の描画ターゲットに向けて描画開始
     * 
     * @param   renderContext[in]     描画コンテキスト
     */
    void BeginDraw(ID3D11DeviceContext* renderContext);

    /**
     * @brief   描画終了
     *
     * @param   renderContext[in]     描画コンテキスト
     */
    void EndDraw(ID3D11DeviceContext* renderContext);

    /**
     *  @brief  CubismOffscreenFrame作成
     *  @param  device[in]                 D3dデバイス
     *  @param  displayBufferWidth[in]     作成するバッファ幅
     *  @param  displayBufferHeight[in]    作成するバッファ高さ
     */
    csmBool CreateOffscreenFrame(ID3D11Device* device, csmUint32 displayBufferWidth, csmUint32 displayBufferHeight);

    /**
     * @brief   CubismOffscreenFrameの削除
     */
    void DestroyOffscreenFrame();


    ID3D11Texture2D*            _texture;          ///< 生成テクスチャ 
    ID3D11ShaderResourceView*   _textureView;      ///< テクスチャのシェーダーリソースとしてのビュー 
    ID3D11RenderTargetView*     _renderTargetView; ///< テクスチャのレンダーターゲットとしてのビュー 
    ID3D11Texture2D*            _depthTexture;     ///< Zテクスチャ 
    ID3D11DepthStencilView*     _depthView;        ///< Zビュー 

    ID3D11RenderTargetView*     _backupRender;     ///< 元々のターゲットを退避 
    ID3D11DepthStencilView*     _backupDepth;      ///< 元々のZを退避 

};


}}}}

//------------ LIVE2D NAMESPACE ------------
