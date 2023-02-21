﻿/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismFramework.hpp"
#include "Type/csmVector.hpp"
#include "Type/csmRectF.hpp"
#include "Type/csmMap.hpp"
#include <float.h>

#ifdef CSM_TARGET_ANDROID_ES2
#include <jni.h>
#include <errno.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#ifdef CSM_TARGET_IPHONE_ES2
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#endif

#if defined(CSM_TARGET_WIN_GL) || defined(CSM_TARGET_LINUX_GL)
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#ifdef CSM_TARGET_MAC_GL
#ifndef CSM_TARGET_COCOS
#include <GL/glew.h>
#endif
#include <OpenGL/gl.h>
#endif

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {


/**
 * @brief  オフスクリーン描画用構造体
 */
class CubismOffscreenFrame_OpenGLES2
{
public:

    CubismOffscreenFrame_OpenGLES2();

    /**
     * @brief   指定の描画ターゲットに向けて描画開始
     *
     * @param   restoreFBO        0以上の場合、EndDrawでこの値をglBindFramebufferする
     */
    void BeginDraw(GLint restoreFBO = -1);

    /**
     * @brief   描画終了
     *
     */
    void EndDraw();

    /**
     * @brief   レンダリングターゲットのクリア
     *           呼ぶ場合はBeginDrawの後で呼ぶこと
     * @param   r   赤(0.0~1.0)
     * @param   g   緑(0.0~1.0)
     * @param   b   青(0.0~1.0)
     * @param   a   α(0.0~1.0)
     */
    void Clear(float r, float g, float b, float a);

    /**
     *  @brief  CubismOffscreenFrame作成
     *  @param  displayBufferWidth     作成するバッファ幅
     *  @param  displayBufferHeight    作成するバッファ高さ
     *  @param  colorBuffer            0以外の場合、ピクセル格納領域としてcolorBufferを使用する
     */
    csmBool CreateOffscreenFrame(csmUint32 displayBufferWidth, csmUint32 displayBufferHeight, GLuint colorBuffer = 0);

    /**
     * @brief   CubismOffscreenFrameの削除
     */
    void DestroyOffscreenFrame();

    /**
     * @brief   レンダーテクスチャメンバーへのアクセッサ
     */
    GLuint GetRenderTexture() const;

    /**
     * @brief   カラーバッファメンバーへのアクセッサ
     */
    GLuint GetColorBuffer() const;

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

private:
    GLuint      _renderTexture;         ///< レンダリングターゲットとしてのアドレス
    GLuint      _colorBuffer;           ///< 描画の際使用するテクスチャとしてのアドレス

    GLint       _oldFBO;                ///< 旧フレームバッファ

    csmUint32   _bufferWidth;           ///< Create時に指定された幅
    csmUint32   _bufferHeight;          ///< Create時に指定された高さ
    csmBool     _isColorBufferInherited;    ///< 引数によって設定されたカラーバッファか？
};


}}}}

//------------ LIVE2D NAMESPACE ------------
