/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismFramework.hpp"
#include "CubismRenderer_OpenGLES2.hpp"
#include "CubismOffscreenRenderTarget_OpenGLES2.hpp"

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

class CubismRenderer_OpenGLES2;
class CubismClippingContext_OpenGLES2;

/**
 * @brief   OpenGLES2用のシェーダプログラムを生成・破棄するクラス<br>
 *           シングルトンなクラスであり、CubismShader_OpenGLES2::GetInstance()からアクセスする。
 *
 */
class CubismShader_OpenGLES2
{
public:
    /**
     * @brief   インスタンスを取得する（シングルトン）。
     *
     * @return  インスタンスのポインタ
     */
    static CubismShader_OpenGLES2* GetInstance();

    /**
     * @brief   インスタンスを解放する（シングルトン）。
     */
    static void DeleteInstance();

    /**
     * @brief   一部の環境でこのインスタンスが管理するリソースが破棄される場合があります。
     *          このような場合に二重解放を避け無効になったリソースを破棄します。
     */
    void ReleaseInvalidShaderProgram();

    /**
     * @brief   描画用のシェーダプログラムの一連のセットアップを実行する
     *
     * @param[in]   renderer              ->  レンダラー
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index                 ->  描画対象のメッシュのインデックス
     */
    void SetupShaderProgramForDrawable(CubismRenderer_OpenGLES2* renderer, const CubismModel& model, const csmInt32 index);

    /**
     * @brief   マスク用のシェーダプログラムの一連のセットアップを実行する
     *
     * @param[in]   renderer              ->  レンダラー
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index                 ->  描画対象のメッシュのインデックス
     */
    void SetupShaderProgramForMask(CubismRenderer_OpenGLES2* renderer, const CubismModel& model, const csmInt32 index);

    /**
     * @brief   オフスクリーンからのシェーダプログラムの一連のセットアップを実行する
     *
     * @param[in]   renderer              ->  レンダラー
     */
    void SetupShaderProgramForOffscreenRenderTarget(CubismRenderer_OpenGLES2* renderer);

    /**
     * @brief   オフスクリーンからのシェーダプログラムの一連のセットアップを実行する
     *
     * @param[in]   renderer              ->  レンダラー
     * @param[in]   model               ->  描画対象のモデル
     * @param[in]   offscreen            ->  描画対象のオフスクリーン
     */
    void SetupShaderProgramForOffscreen(CubismRenderer_OpenGLES2* renderer, const CubismModel& model, const CubismOffscreenRenderTarget_OpenGLES2* offscreen);

    /**
     * @brief   シェーダを使って描画対象をコピーする
     *
     * @param[in]   texture               ->  テクスチャ
     * @param[in]   srcColor               ->  ソースカラーのブレンド係数
     * @param[in]   dstColor               ->  デスティネーションカラーのブレンド係数
     * @param[in]   srcAlpha               ->  ソースアルファのブレンド係数
     * @param[in]   dstAlpha               ->  デスティネーションアルファのブレンド係数
     */
    void CopyTexture(
        GLint texture,
        csmInt32 srcColor = GL_ONE,
        csmInt32 dstColor = GL_ZERO,
        csmInt32 srcAlpha = GL_ONE,
        csmInt32 dstAlpha = GL_ZERO,
        CubismRenderer::CubismTextureColor baseColor = CubismRenderer::CubismTextureColor()
    );

private:
    enum MaskType
    {
        MaskType_None,
        MaskType_Masked,
        MaskType_MaskedInverted,
        MaskType_PremultipliedAlpha,
        MaskType_MaskedPremultipliedAlpha,
        MaskType_MaskedInvertedPremultipliedAlpha,
    };

    enum ColorBlendMode
    {
        ColorBlendMode_None = -1,
        ColorBlendMode_Nomal,
        ColorBlendMode_Add,
        ColorBlendMode_AddGlow,
        ColorBlendMode_Darken,
        ColorBlendMode_Multiply,
        ColorBlendMode_ColorBurn,
        ColorBlendMode_LinearBurn,
        ColorBlendMode_Lighten,
        ColorBlendMode_Screen,
        ColorBlendMode_ColorDodge,
        ColorBlendMode_Overlay,
        ColorBlendMode_SoftLight,
        ColorBlendMode_HardLight,
        ColorBlendMode_LinearLight,
        ColorBlendMode_Hue,
        ColorBlendMode_Color,
        ColorBlendMode_Count,
    };

    enum AlphaBlendMode
    {
        AlphaBlendMode_None = -1,
        AlphaBlendMode_Over,
        AlphaBlendMode_Atop,
        AlphaBlendMode_Out,
        AlphaBlendMode_ConjointOver,
        AlphaBlendMode_DisjointOver,
        AlphaBlendMode_Count,
    };

    /**
    * @bref    シェーダープログラムとシェーダ変数のアドレスを保持する構造体
    *
    */
    struct CubismShaderSet
    {
        GLuint ShaderProgram;               ///< シェーダプログラムのアドレス
        GLuint AttributePositionLocation;   ///< シェーダプログラムに渡す変数のアドレス(Position)
        GLuint AttributeTexCoordLocation;   ///< シェーダプログラムに渡す変数のアドレス(TexCoord)
        GLint UniformMatrixLocation;        ///< シェーダプログラムに渡す変数のアドレス(Matrix)
        GLint UniformClipMatrixLocation;    ///< シェーダプログラムに渡す変数のアドレス(ClipMatrix)
        GLint SamplerTexture0Location;      ///< シェーダプログラムに渡す変数のアドレス(Texture0)
        GLint SamplerTexture1Location;      ///< シェーダプログラムに渡す変数のアドレス(Texture1)
        GLint SamplerBlendTextureLocation;  ///< シェーダプログラムに渡す変数のアドレス(BlendTexture)
        GLint UniformBaseColorLocation;     ///< シェーダプログラムに渡す変数のアドレス(BaseColor)
        GLint UniformMultiplyColorLocation; ///< シェーダプログラムに渡す変数のアドレス(MultiplyColor)
        GLint UniformScreenColorLocation;   ///< シェーダプログラムに渡す変数のアドレス(ScreenColor)
        GLint UnifromChannelFlagLocation;   ///< シェーダプログラムに渡す変数のアドレス(ChannelFlag)
    };
    /**
     * @brief   CubismShaderSetを設定する
     *
     * @param[in]   shaderSets   ->  シェーダの設定
     * @param[in]   maskType     ->  マスクの種類
     * @param[in]   isBlendMode  ->  ブレンドモードを使用するか
     */
    static void SetShaderSet(CubismShaderSet& shaderSets, MaskType maskType, csmBool isBlendMode = false);

    /**
     * @brief   どのシェーダーを利用するかを取得する
     *
     * @param[in]   blendMode  ->  ブレンドモード
     *
     * @return  使用するシェーダの値を返す
     */
    static csmInt32 GetShaderNamesBegin(const csmBlendMode blendMode);

    /**
     * @brief   privateなコンストラクタ
     */
    CubismShader_OpenGLES2();

    /**
     * @brief   privateなデストラクタ
     */
    virtual ~CubismShader_OpenGLES2();

    /**
     * @brief   シェーダプログラムを解放する
     */
    void ReleaseShaderProgram();

    /**
     * @brief   シェーダプログラムを初期化する
     */
    void GenerateShaders();

    /**
     * @brief   ファイルからシェーダプログラムをロードし、シェーダーオブジェクトの番号を返す。
     *
     * @param[in]   vertShaderSrc    ->  頂点シェーダのファイルパス
     * @param[in]   fragShaderSrc    ->  フラグメントシェーダのファイルパス
     * @param[in]   colorBlendMode   ->  ブレンドカラーモード
     * @param[in]   alphaBlendMode ->  オーバーラップカラーモード
     *
     * @return  シェーダーオブジェクトの番号
     */
    GLuint LoadShaderProgramFromFile(const csmChar* vertShaderPath, const csmChar* fragShaderPath, csmInt32 colorBlendMode = ColorBlendMode_None, csmInt32 alphaBlendMode = AlphaBlendMode_None);

    /**
     * @brief   シェーダプログラムをロードしてアドレス返す。
     *
     * @param[in]   vertShaderSrc   ->  頂点シェーダのソース
     * @param[in]   fragShaderSrc   ->  フラグメントシェーダのソース
     *
     * @return  シェーダプログラムのアドレス
     */
    GLuint LoadShaderProgram(const csmChar* vertShaderSrc, const csmChar* fragShaderSrc);

    /**
     * @brief   シェーダプログラムをコンパイルする
     *
     * @param[out]  outShader       ->  コンパイルされたシェーダプログラムのアドレス
     * @param[in]   shaderType      ->  シェーダタイプ(Vertex/Fragment)
     * @param[in]   shaderSource    ->  シェーダソースコード
     *
     * @retval      true         ->      コンパイル成功
     * @retval      false        ->      コンパイル失敗
     */
    csmBool CompileShaderSource(GLuint* outShader, GLenum shaderType, const csmChar* shaderSource);

    /**
     * @brief   シェーダプログラムをリンクする
     *
     * @param[in]   shaderProgram   ->  リンクするシェーダプログラムのアドレス
     *
     * @retval      true            ->  リンク成功
     * @retval      false           ->  リンク失敗
     */
    csmBool LinkProgram(GLuint shaderProgram);

    /**
     * @brief   シェーダプログラムを検証する
     *
     * @param[in]   shaderProgram   ->  検証するシェーダプログラムのアドレス
     *
     * @retval      true            ->  正常
     * @retval      false           ->  異常
     */
    csmBool ValidateProgram(GLuint shaderProgram);

    /**
     * @brief   必要な頂点属性を設定する
     *
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index                 ->  描画対象のメッシュのインデックス
     * @param[in]   shaderSet             ->  シェーダープログラムのセット
     */
    void SetVertexAttributes(const CubismModel& model, const csmInt32 index, CubismShaderSet* shaderSet);

    /**
     * @brief   テクスチャの設定を行う
     *
     * @param[in]   renderer              ->  レンダラー
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index                 ->  描画対象のメッシュのインデックス
     * @param[in]   shaderSet             ->  シェーダープログラムのセット
     */
    void SetupTexture(CubismRenderer_OpenGLES2* renderer, const CubismModel& model, const csmInt32 index, CubismShaderSet* shaderSet);

    /**
     * @brief   色関連のユニフォーム変数の設定を行う
     *
     * @param[in]   renderer              ->  レンダラー
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index                 ->  描画対象のメッシュのインデックス
     * @param[in]   shaderSet             ->  シェーダープログラムのセット
     * @param[in]   baseColor             ->  ベースカラー
     * @param[in]   multiplyColor         ->  乗算カラー
     * @param[in]   screenColor           ->  スクリーンカラー
     */
    void SetColorUniformVariables(CubismRenderer_OpenGLES2* renderer, const CubismModel& model, const csmInt32 index, CubismShaderSet* shaderSet,
                                  CubismRenderer::CubismTextureColor& baseColor, CubismRenderer::CubismTextureColor& multiplyColor, CubismRenderer::CubismTextureColor& screenColor);

    /**
     * @brief   カラーチャンネル関連のユニフォーム変数の設定を行う
     *
     * @param[in]   shaderSet             ->  シェーダープログラムのセット
     * @param[in]   contextBuffer         ->  描画コンテクスト
     */
    void SetColorChannelUniformVariables(CubismShaderSet* shaderSet, CubismClippingContext_OpenGLES2* contextBuffer);

#ifdef CSM_TARGET_ANDROID_ES2
public:
    /**
     * @brief   Tegraプロセッサ対応。拡張方式による描画の有効・無効
     *
     * @param[in]   extMode     ->  trueなら拡張方式で描画する
     * @param[in]   extPAMode   ->  trueなら拡張方式のPA設定を有効にする
     */
    static void SetExtShaderMode(csmBool extMode, csmBool extPAMode);

private:
    static csmBool  s_extMode;      ///< Tegra対応.拡張方式で描画
    static csmBool  s_extPAMode;    ///< 拡張方式のPA設定用の変数
#endif

    csmVector<CubismShaderSet*> _shaderSets;   ///< ロードしたシェーダプログラムを保持する変数

};

}}}}
//------------ LIVE2D NAMESPACE ------------
