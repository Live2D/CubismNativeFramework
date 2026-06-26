/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "../CubismRenderer.hpp"
#include "../CubismClippingManager.hpp"
#include "CubismFramework.hpp"
#include "CubismRenderTarget_OpenGLES2.hpp"
#include "CubismOffscreenRenderTarget_OpenGLES2.hpp"
#include "CubismShader_OpenGLES2.hpp"
#include "Type/csmVector.hpp"
#include "Type/csmRectF.hpp"
#include "Math/CubismVector2.hpp"
#include "Type/csmMap.hpp"

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

#ifdef CSM_TARGET_HARMONYOS_ES3
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#endif

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

//  前方宣言
class CubismRenderer_OpenGLES2;
class CubismClippingContext_OpenGLES2;
class CubismShader_OpenGLES2;

/**
 * @brief  クリッピングマスクの処理を実行するクラス
 *
 */
class CubismClippingManager_OpenGLES2 : public CubismClippingManager<CubismClippingContext_OpenGLES2, CubismRenderTarget_OpenGLES2>
{
public:

    /**
     * @brief   クリッピングコンテキストを作成する。モデル描画時に実行する。
     *
     * @param[in]   model              ->  モデルのインスタンス
     * @param[in]   renderer           ->  レンダラのインスタンス
     * @param[in]   lastFBO            ->  フレームバッファ
     * @param[in]   lastViewport       ->  ビューポート
     * @param[in]   drawableObjectType ->  描画オブジェクトのタイプ
     */
    void SetupClippingContext(CubismModel& model, CubismRenderer_OpenGLES2* renderer, GLint lastFBO, GLint lastViewport[4], CubismRenderer::DrawableObjectType drawableObjectType);
};

/**
 * @brief   クリッピングマスクのコンテキスト
 */
class CubismClippingContext_OpenGLES2 : public CubismClippingContext
{
    friend class CubismClippingManager_OpenGLES2;
    friend class CubismRenderer_OpenGLES2;

public:
    /**
     * @brief   引数付きコンストラクタ
     *
     * @param[in]   manager                  ->  マスクを管理しているマネージャのインスタンス
     * @param[in]   model                    ->  モデルのインスタンス
     * @param[in]   clippingDrawableIndices  ->  クリップしているDrawableのインデックスリスト
     * @param[in]   clipCount                ->  クリップしているDrawableの個数
     */
    CubismClippingContext_OpenGLES2(CubismClippingManager<CubismClippingContext_OpenGLES2, CubismRenderTarget_OpenGLES2>* manager, CubismModel& model, const csmInt32* clippingDrawableIndices, csmInt32 clipCount);

    /**
     * @brief   デストラクタ
     */
    virtual ~CubismClippingContext_OpenGLES2();

    /**
     * @brief   このマスクを管理するマネージャのインスタンスを取得する。
     *
     * @return  クリッピングマネージャのインスタンス
     */
    CubismClippingManager<CubismClippingContext_OpenGLES2, CubismRenderTarget_OpenGLES2>* GetClippingManager();

    CubismClippingManager<CubismClippingContext_OpenGLES2, CubismRenderTarget_OpenGLES2>* _owner;        ///< このマスクを管理しているマネージャのインスタンス
};

/**
 * @brief   Cubismモデルを描画する直前のOpenGLES2のステートを保持・復帰させるクラス
 *
 */
class CubismRendererProfile_OpenGLES2
{
    friend class CubismRenderer_OpenGLES2;

private:
    /**
     * @biref   privateなコンストラクタ
     */
    CubismRendererProfile_OpenGLES2() {};

    /**
     * @biref   privateなデストラクタ
     */
    virtual ~CubismRendererProfile_OpenGLES2() {};

    /**
     * @brief   OpenGLES2のステートを保持する
     */
    void Save();

    /**
     * @brief   保持したOpenGLES2のステートを復帰させる
     *
     */
    void Restore();

    /**
     * @brief   OpenGLES2の機能の有効・無効をセットする
     *
     * @param[in]   index   ->  有効・無効にする機能
     * @param[in]   enabled ->  trueなら有効にする
     */
    void SetGlEnable(GLenum index, GLboolean enabled);

    /**
     * @brief   OpenGLES2のVertex Attribute Array機能の有効・無効をセットする
     *
     * @param[in]   index   ->  有効・無効にする機能
     * @param[in]   enabled ->  trueなら有効にする
     */
    void SetGlEnableVertexAttribArray(GLuint index, GLint enabled);

    GLint _lastArrayBufferBinding;          ///< モデル描画直前の頂点バッファ
    GLint _lastElementArrayBufferBinding;   ///< モデル描画直前のElementバッファ
    GLint _lastProgram;                     ///< モデル描画直前のシェーダプログラムバッファ
    GLint _lastActiveTexture;               ///< モデル描画直前のアクティブなテクスチャ
    GLint _lastTexture0Binding2D;           ///< モデル描画直前のテクスチャユニット0
    GLint _lastTexture1Binding2D;           ///< モデル描画直前のテクスチャユニット1
    GLint _lastVertexAttribArrayEnabled[4]; ///< モデル描画直前のテクスチャユニット1
    GLboolean _lastScissorTest;             ///< モデル描画直前のGL_VERTEX_ATTRIB_ARRAY_ENABLEDパラメータ
    GLboolean _lastBlend;                   ///< モデル描画直前のGL_SCISSOR_TESTパラメータ
    GLboolean _lastStencilTest;             ///< モデル描画直前のGL_STENCIL_TESTパラメータ
    GLboolean _lastDepthTest;               ///< モデル描画直前のGL_DEPTH_TESTパラメータ
    GLboolean _lastCullFace;                ///< モデル描画直前のGL_CULL_FACEパラメータ
    GLint _lastFrontFace;                   ///< モデル描画直前のGL_CULL_FACEパラメータ
    GLboolean _lastColorMask[4];            ///< モデル描画直前のGL_COLOR_WRITEMASKパラメータ
    GLint _lastBlending[4];                 ///< モデル描画直前のカラーブレンディングパラメータ
};

/**
 * @brief   OpenGLES2用の描画命令を実装したクラス
 *
 */
class CubismRenderer_OpenGLES2 : public CubismRenderer
{
    friend class CubismRenderer;
    friend class CubismClippingManager_OpenGLES2;
    friend class CubismShader_OpenGLES2;

public:
    /**
     * @brief    レンダラの初期化処理を実行する
     *           引数に渡したモデルからレンダラの初期化処理に必要な情報を取り出すことができる
     *
     * @param[in]  model -> モデルのインスタンス
     */
    void Initialize(Framework::CubismModel* model) override;

    /**
     * @brief   レンダラの初期化処理を実行する
     *           引数に渡したモデルからレンダラの初期化処理に必要な情報を取り出すことができる
     *
     * @param[in]  model           -> モデルのインスタンス
     * @param[in]  maskBufferCount -> マスクの分割数
     */
    void Initialize(Framework::CubismModel* model, csmInt32 maskBufferCount) override;

    /**
     * @bref オフスクリーンの親を探して設定する
     *
     * @param model -> モデルのインスタンス
     * @param offscreenCount -> オフスクリーンの数
     */
    void SetupParentOffscreens(const CubismModel* model, csmInt32 offscreenCount);

    /**
     * @brief   OpenGLテクスチャのバインド処理<br>
     *           CubismRendererにテクスチャを設定し、CubismRenderer中でその画像を参照するためのIndex値を戻り値とする
     *
     * @param[in]   modelTextureIndex  ->  セットするモデルテクスチャの番号
     * @param[in]   glTextureIndex     ->  OpenGLテクスチャの番号
     *
     */
    void BindTexture(csmUint32 modelTextureIndex, GLuint glTextureIndex);

    /**
     * @brief   OpenGLにバインドされたテクスチャのリストを取得する
     *
     * @return  テクスチャのアドレスのリスト
     */
    const csmMap<csmInt32, GLuint>& GetBindedTextures() const;

    /**
     * @brief  クリッピングマスクバッファのサイズを設定する<br>
     *         マスク用のRenderTargetを破棄・再作成するため処理コストは高い。
     *
     * @param[in]  width  -> クリッピングマスクバッファの幅
     * @param[in]  height -> クリッピングマスクバッファの高さ
     */
    void SetDrawableClippingMaskBufferSize(csmFloat32 width, csmFloat32 height);

    /**
     * @brief  オフスクリーン用クリッピングマスクバッファのサイズを設定する<br>
     *         マスク用のRenderTargetを破棄・再作成するため処理コストは高い。
     *
     * @param[in]  width  -> クリッピングマスクバッファの幅
     * @param[in]  height -> クリッピングマスクバッファの高さ
     */
    void SetOffscreenClippingMaskBufferSize(csmFloat32 width, csmFloat32 height);

    /**
     * @brief  描画オブジェクトのレンダーテクスチャの枚数を取得する。
     *
     * @return  描画オブジェクトのレンダーテクスチャの枚数
     */
    csmInt32 GetDrawableRenderTextureCount() const;

    /**
     * @brief  オフスクリーンのレンダーテクスチャの枚数を取得する。
     *
     * @return  オフスクリーンのレンダーテクスチャの枚数
     */
    csmInt32 GetOffscreenRenderTextureCount() const;

    /**
     * @brief  描画オブジェクトのクリッピングマスクバッファのサイズを取得する
     *
     * @return 描画オブジェクトのクリッピングマスクバッファのサイズ
     */
    CubismVector2 GetDrawableClippingMaskBufferSize() const;

    /**
     * @brief  オフスクリーンのクリッピングマスクバッファのサイズを取得する
     *
     * @return オフスクリーンのクリッピングマスクバッファのサイズ
     */
    CubismVector2 GetOffscreenClippingMaskBufferSize() const;

    /**
     * @brief  オフスクリーンのバッファをコピーする
     *
     * @return オフスクリーンのバッファへのポインタ
     */
    const CubismRenderTarget_OpenGLES2* CopyOffscreenRenderTarget();

    /**
     * @brief  任意のバッファをコピーする
     *
     * @param srcBuffer コピー元のバッファ
     *
     * @return コピー後のバッファへのポインタ
     */
    const CubismRenderTarget_OpenGLES2* CopyRenderTarget(const CubismRenderTarget_OpenGLES2& srcBuffer);

    /**
     * @brief  描画オブジェクトのクリッピングマスクのバッファを取得する
     *
     * @return 描画オブジェクトのクリッピングマスクのバッファへのポインタ
     */
    CubismRenderTarget_OpenGLES2* GetDrawableMaskBuffer(csmInt32 index);

    /**
     * @brief  オフスクリーンのクリッピングマスクのバッファを取得する
     *
     * @return オフスクリーンのクリッピングマスクのバッファへのポインタ
     */
    CubismRenderTarget_OpenGLES2* GetOffscreenMaskBuffer(csmInt32 index);

    /**
     * @brief  現在のオフスクリーンのフレームバッファを取得する
     *
     * @return 現在のオフスクリーンのフレームバッファを返す
     */
    CubismOffscreenRenderTarget_OpenGLES2* GetCurrentOffscreen() const;

protected:
    /**
     * @brief   コンストラクタ
     *
     * @param[in] width -> モデルを描画したバッファの幅
     * @param[in] height -> モデルを描画したバッファの高さ
     */
    CubismRenderer_OpenGLES2(csmUint32 width, csmUint32 height);

    /**
     * @brief   デストラクタ
     */
    virtual ~CubismRenderer_OpenGLES2();

    /**
     * @brief   モデルを描画する実際の処理
     *
     */
    virtual void DoDrawModel() override;

    /**
     * @brief   描画オブジェクト（アートメッシュ、オフスクリーン）を描画するループ処理
     *
     * @param[in]   lastFBO        ->  モデル描画直前のフレームバッファ
     * @param[in]   lastViewport   ->  モデル描画直前のビューポート
     */
    void DrawObjectLoop(GLint lastFBO, GLint lastViewport[4]);

    /**
     * @brief 各オブジェクトの描画処理を呼ぶ。
     *
     * @param[in]   objectIndex  ->  描画対象のオブジェクトのインデックス
     * @param[in]   objectType  ->  描画対象のオブジェクトのタイプ
     */
    void RenderObject(csmInt32 objectIndex, csmInt32 objectType);

    /**
     * @brief   描画オブジェクト（アートメッシュ）を描画する。
     *
     * @param[in]   drawableIndex ->  描画対象のメッシュのインデックス
     */
    void DrawDrawable(csmInt32 drawableIndex);

    /**
     * @brief   親オフスクリーンへオフスクリーンの描画結果の伝搬を試みる。
     *
     * @param[in] objectIndex 処理をするオブジェクトのインデックス
     * @param objectType 処理をするオブジェクトのタイプ
     */
    void SubmitDrawToParentOffscreen(csmInt32 objectIndex, DrawableObjectType objectType);

    /**
     * @brief   描画オブジェクト（オフスクリーン）を追加する。
     *
     * @param[in]   offscreenIndex ->  描画対象のオフスクリーンのインデックス
     */
    void AddOffscreen(csmInt32 offscreenIndex);

    /**
     * @brief   描画オブジェクト（オフスクリーン）を描画する。
     *
     * @param[in]   currentOffscreen ->  描画対象のオフスクリーン
     */
    void DrawOffscreen(CubismOffscreenRenderTarget_OpenGLES2* currentOffscreen);

    /**
     * @brief    描画オブジェクト（アートメッシュ）を描画する。
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のメッシュのインデックス
     */
    void DrawMeshOpenGL(const CubismModel& model, const csmInt32 index);

    /**
     * @brief   オフスクリーンを描画する。
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   offscreen ->  描画対象のオフスクリーン
     */
    void DrawOffscreenOpenGL(const CubismModel& model, CubismOffscreenRenderTarget_OpenGLES2* offscreen);

#ifdef CSM_TARGET_ANDROID_ES2
public:
    /**
     * @brief   Tegraプロセッサ対応。拡張方式による描画の有効・無効
     *
     * @param[in]   extMode     ->  trueなら拡張方式で描画する
     * @param[in]   extPAMode   ->  trueなら拡張方式のPA設定を有効にする
     */
    static void SetExtShaderMode(csmBool extMdoe, csmBool extPAMode = false);

    /**
     * @brief   Android-Tegra対応. シェーダプログラムをリロードする。
     */
    static void ReloadShader();
#endif

private:
    // Prevention of copy Constructor
    CubismRenderer_OpenGLES2(const CubismRenderer_OpenGLES2&);
    CubismRenderer_OpenGLES2& operator=(const CubismRenderer_OpenGLES2&);

    /**
     * @brief   レンダラが保持する静的なリソースを解放する<br>
     *           OpenGLES2の静的なシェーダプログラムを解放する
     */
    static void DoStaticRelease();

    /**
     * @brief   テクスチャバリアが使えるか判定する
     *
     * @return テクスチャバリアが使える場合はtrueを返す
     */
    static csmBool CanUseTextureBarrier();

    /**
     * @brief   描画開始時の追加処理。<br>
     *           モデルを描画する前にクリッピングマスクに必要な処理を実装している。
     */
    void PreDraw();

    /**
     * @brief   描画完了後の追加処理。
     *
     */
    void PostDraw(){};

    /**
     * @brief   モデル描画直前のOpenGLES2のステートを保持する
     */
    void SaveProfile() override;

    /**
     * @brief   モデル描画直前のOpenGLES2のステートを保持する
     */
    void RestoreProfile() override;

    /**
     * @brief   モデル描画直前のオフスクリーン設定
     */
    virtual void BeforeDrawModelRenderTarget();

    /**
     * @brief   モデル描画後のオフスクリーン設定
     */
    virtual void AfterDrawModelRenderTarget();

    /**
     * @brief   マスクテクスチャに描画するクリッピングコンテキストをセットする。
     *
     * @param[in]   clip     ->  マスクテクスチャに描画するクリッピングコンテキスト
     */
    void SetClippingContextBufferForMask(CubismClippingContext_OpenGLES2* clip);

    /**
     * @brief   マスクテクスチャに描画するクリッピングコンテキストを取得する。
     *
     * @return  マスクテクスチャに描画するクリッピングコンテキスト
     */
    CubismClippingContext_OpenGLES2* GetClippingContextBufferForMask() const;

    /**
     * @brief   drawableで画面上に描画するクリッピングコンテキストをセットする。
     *
     * @param[in]   clip     ->  drawableで画面上に描画するクリッピングコンテキスト
     */
    void SetClippingContextBufferForDrawable(CubismClippingContext_OpenGLES2* clip);

    /**
     * @brief   drawableで画面上に描画するクリッピングコンテキストを取得する。
     *
     * @return  drawableで画面上に描画するクリッピングコンテキスト
     */
    CubismClippingContext_OpenGLES2* GetClippingContextBufferForDrawable() const;

    /**
     * @brief   offscreenで画面上に描画するクリッピングコンテキストをセットする。
     *
     * @param[in]   clip     ->  offscreenで画面上に描画するクリッピングコンテキスト
     */
    void SetClippingContextBufferForOffscreen(CubismClippingContext_OpenGLES2* clip);

    /**
     * @brief   offscreenで画面上に描画するクリッピングコンテキストを取得する。
     *
     * @return  offscreenで画面上に描画するクリッピングコンテキスト
     */
    CubismClippingContext_OpenGLES2* GetClippingContextBufferForOffscreen() const;

    /**
     * @brief   マスク生成時かを判定する
     *
     * @return  判定値
     */
    const csmBool inline IsGeneratingMask() const;

    /**
     * @brief   テクスチャマップにバインドされたテクスチャIDを取得する。<br>
     *          バインドされていなければダミーとして-1が返される。
     *
     * @param[in]   textureId  -> テクスチャID
     *
     * @return  バインドされたテクスチャID
     */
    GLuint GetBindedTextureId(csmInt32 textureId);

#ifdef CSM_TARGET_WIN_GL
    /**
     * @brief   Windows対応。OpenGL命令のバインドを行う。
     */
    void  InitializeGlFunctions();

    /**
     * @brief   Windows対応。OpenGL命令のバインドする際に関数ポインタを取得する。
     *
     * @param[in]   name  -> ポインタを取得したいOpenGLの関数名
     */
    void* WinGlGetProcAddress(const csmChar* name);

    /**
     * @brief   Windows対応。OpenGLのエラーチェック。
     *
     * @param[in]   message  -> 追加メッセージ
     */
    void  CheckGlError(const csmChar* message);

#endif

    csmMap<csmInt32, GLuint> _textures;                      ///< モデルが参照するテクスチャとレンダラでバインドしているテクスチャとのマップ
    csmVector<csmInt32> _sortedObjectsIndexList;       ///< 描画オブジェクトのインデックスを描画順に並べたリスト
    csmVector<DrawableObjectType> _sortedObjectsTypeList;       ///< 描画オブジェクトの種別を描画順に並べたリスト
    CubismRendererProfile_OpenGLES2 _rendererProfile;               ///< OpenGLのステートを保持するオブジェクト
    CubismClippingManager_OpenGLES2* _drawableClippingManager;               ///< クリッピングマスク管理オブジェクト
    CubismClippingManager_OpenGLES2* _offscreenClippingManager;               ///< クリッピングマスク管理オブジェクト
    CubismClippingContext_OpenGLES2* _clippingContextBufferForMask;  ///< マスクテクスチャに描画するためのクリッピングコンテキスト
    CubismClippingContext_OpenGLES2* _clippingContextBufferForDrawable;  ///< 画面上描画するためのクリッピングコンテキスト
    CubismClippingContext_OpenGLES2* _clippingContextBufferForOffscreen;  ///< 画面上描画するためのクリッピングコンテキスト

    csmVector<CubismRenderTarget_OpenGLES2> _modelRenderTargets; ///< モデル全体を描画する先のフレームバッファ

    csmVector<CubismRenderTarget_OpenGLES2> _drawableMasks; ///< Drawableのマスク描画用のフレームバッファ
    csmVector<CubismRenderTarget_OpenGLES2> _offscreenMasks; ///< オフスクリーン機能マスク描画用のフレームバッファ

    csmVector<CubismOffscreenRenderTarget_OpenGLES2> _offscreenList; ///< モデルのオフスクリーン
    GLint _currentFBO; ///< 現在のフレームバッファオブジェクト
    CubismOffscreenRenderTarget_OpenGLES2* _currentOffscreen; ///< 現在のオフスクリーンのフレームバッファ

    GLint _modelRootFBO; ///< モデル描画のルートフレームバッファ
};

}}}}
//------------ LIVE2D NAMESPACE ------------
