/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include <MetalKit/MetalKit.h>
#include "CubismFramework.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @brief  オフスクリーン描画用構造体
 */
class CubismRenderTarget_Metal
{
public:

    /**
    * @brief オフスクリーンサーフェス間でカラーバッファをコピーする
    *
    * @param  src コピー元
    * @param  dst コピー先
    * @param  commandBuffer コマンドバッファ
    */
    static void CopyBuffer(CubismRenderTarget_Metal& src, CubismRenderTarget_Metal& dst, id<MTLCommandBuffer> commandBuffer);

    /**
     * @brief   コンストラクタ
     */
    CubismRenderTarget_Metal();

    /**
     * @brief   指定の描画ターゲットに向けて描画開始
     *
     * @param  commandBuffer コマンドバッファ
     */
    void BeginDraw(id<MTLCommandBuffer> commandBuffer);

    /**
     * @brief   描画終了
     *
     */
    void EndDraw();

    /**
     * @brief   レンダリングターゲットのクリア
     *           呼ぶ場合はBeginDrawの前で呼ぶこと
     *
     * @param   r   赤(0.0~1.0)
     * @param   g   緑(0.0~1.0)
     * @param   b   青(0.0~1.0)
     * @param   a   α(0.0~1.0)
     */
    void Clear(float r, float g, float b, float a);

    /**
     * @brief  CubismRenderTarget作成
     * @param  device   デバイス
     * @param  displayBufferWidth     作成するバッファ幅
     * @param  displayBufferHeight    作成するバッファ高さ
     * @param  colorBuffer            0以外の場合、ピクセル格納領域としてcolorBufferを使用する
     * @param  depthBuffer            0以外の場合、深度納領域としてdepthBufferを使用する
     * @return 成功時はtrue、失敗時はfalse
     */
    csmBool CreateRenderTarget(id<MTLDevice> device, csmUint32 displayBufferWidth, csmUint32 displayBufferHeight, id <MTLTexture> colorBuffer = nil, id <MTLTexture> depthBuffer = nil);

    /**
     * @brief   CubismRenderTargetの削除
     */
    void DestroyRenderTarget();

    /**
     * @brief   カラーバッファメンバーへのアクセッサ
     */
    id <MTLTexture> GetColorBuffer() const;

    /**
     * @brief   深度バッファメンバーへのアクセッサ
     */
    id <MTLTexture> GetDepthBuffer() const;

    /**
     * @brief  コマンドエンコーダーを取得する
     */
    id <MTLRenderCommandEncoder> GetCommandEncoder() const;

    /**
     * @brief   バッファ幅取得
     */
    csmUint32 GetBufferWidth() const;

    /**
     * @brief   バッファ高さ取得
     */
    csmUint32 GetBufferHeight() const;

    /**
     * @brief   現在有効かどうか
     */
    csmBool IsValid() const;

    /**
     * @brief   MTLViewport取得
     */
    const MTLViewport* GetViewport() const;

    /**
     * @brief   MTLRenderPassDescriptor取得
     */
    MTLRenderPassDescriptor* GetRenderPassDescriptor() const;

    /**
     * @brief   MTLPixelFormat指定
     */
    void SetMTLPixelFormat(MTLPixelFormat pixelFormat);

private:
    id <MTLTexture>  _colorBuffer; ///レンダーテクスチャ
    id <MTLTexture>  _depthBuffer; ///深度用テクスチャ
    id <MTLRenderCommandEncoder> _commandEncoder; // コマンドエンコーダー
    MTLRenderPassDescriptor *_renderPassDescriptor;
    csmUint32   _bufferWidth;           ///< Create時に指定された幅
    csmUint32   _bufferHeight;          ///< Create時に指定された高さ
    MTLViewport _viewPort;
    MTLPixelFormat _pixelFormat;
    csmFloat32 _clearColorR;
    csmFloat32 _clearColorG;
    csmFloat32 _clearColorB;
    csmFloat32 _clearColorA;
};

}}}}

//------------ LIVE2D NAMESPACE ------------
