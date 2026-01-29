/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include <MetalKit/MetalKit.h>
#include "CubismFramework.hpp"
#include "Model/CubismModel.hpp"
#include "CubismCommandBuffer_Metal.hpp"
#include "Type/csmVector.hpp"
#include "Shaders/MetalShaderTypes.h"
#include "CubismOffscreenRenderTarget_Metal.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

// 前方宣言
class CubismRenderer_Metal;
class CubismClippingContext_Metal;

/**
 * @brief   Metal用のシェーダプログラムを生成・破棄するクラス
 *
 */
class CubismShader_Metal
{
public:
    /**
     * @brief   コンストラクタ
     */
    CubismShader_Metal();

    /**
     * @brief   デストラクタ
     */
    virtual ~CubismShader_Metal();

    /**
     * @brief   描画用のシェーダプログラムの一連のセットアップを実行する
     *
     * @param[in]   drawCommandBuffer     ->  コマンドバッファ
     * @param[in]   renderEncoder         ->  MTLRenderCommandEncoder
     * @param[in]   renderer              ->  レンダラのインスタンス
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index                 ->  描画オブジェクトのインデックス
     * @param[in]   blendTexture          ->  ブレンド対象のテクスチャ
     */
    void SetupShaderProgramForDrawable(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                                , CubismRenderer_Metal* renderer, const CubismModel& model, const csmInt32 index, id<MTLTexture> blendTexture);

    /**
     * @brief   マスク用のシェーダプログラムの一連のセットアップを実行する
     *
     * @param[in]   drawCommandBuffer     ->  コマンドバッファ
     * @param[in]   renderEncoder         ->  MTLRenderCommandEncoder
     * @param[in]   renderer              ->  レンダラのインスタンス
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index                 ->  描画オブジェクトのインデックス
     */
    void SetupShaderProgramForMask(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                                , CubismRenderer_Metal* renderer, const CubismModel& model, const csmInt32 index);

    /**
     * @brief   オフスクリーンからのコピー用のシェーダプログラムの一連のセットアップを実行する
     *
     * @param[in]   drawCommandBuffer     ->  コマンドバッファ
     * @param[in]   renderEncoder         ->  MTLRenderCommandEncoder
     * @param[in]   renderer              ->  レンダラのインスタンス
     */
    void SetupShaderProgramForRenderTarget(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                                                           , CubismRenderer_Metal* renderer);

    /**
     * @brief   オフスクリーンからのシェーダプログラムの一連のセットアップを実行する
     *
     * @param[in]   drawCommandBuffer     ->  コマンドバッファ
     * @param[in]   renderEncoder         ->  MTLRenderCommandEncoder
     * @param[in]   renderer              ->  レンダラのインスタンス
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index                 ->  描画オブジェクトのインデックス
     * @param[in]   offscreen             ->  描画対象のオフスクリーン
     * @param[in]   blendTexture          ->  ブレンド対象のテクスチャ
     */
    void SetupShaderProgramForOffscreen(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                                , CubismRenderer_Metal* renderer, const CubismModel& model, const CubismOffscreenRenderTarget_Metal* offscreen, id<MTLTexture> blendTexture);
    /**
     * @brief   シェーダを使って描画対象をコピーする
     *
     * @param[in]   texture               ->  テクスチャ
     * @param[in]   drawCommandBuffer     ->  コマンドバッファ
     * @param[in]   renderEncoder         ->  MTLRenderCommandEncoder
     * @param[in]   renderer              ->  レンダラのインスタンス
     */
    void CopyTexture(id<MTLTexture> texture, CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                                , CubismRenderer_Metal* renderer);

private:
    struct ShaderProgram
    {
        id <MTLFunction> vertexFunction;
        id <MTLFunction> fragmentFunction;
    };

    /**
    * @bref    シェーダープログラムとシェーダ変数のアドレスを保持する構造体
    *
    */
    struct CubismShaderSet
    {
        ShaderProgram *ShaderProgram; ///< シェーダプログラムのアドレス
        id<MTLRenderPipelineState> RenderPipelineState;
        id<MTLDepthStencilState> DepthStencilState;
        id<MTLSamplerState> SamplerState;
    };

    enum ShaderNames
    {
#define CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(NAME) \
        CSM_CREATE_SHADER_NAMES(NAME ## Over), \
        CSM_CREATE_SHADER_NAMES(NAME ## Atop), \
        CSM_CREATE_SHADER_NAMES(NAME ## Out), \
        CSM_CREATE_SHADER_NAMES(NAME ## ConjointOver), \
        CSM_CREATE_SHADER_NAMES(NAME ## DisjointOver)

#define CSM_CREATE_SHADER_NAMES(NAME) \
        ShaderNames_ ## NAME, \
        ShaderNames_ ## NAME ## Masked, \
        ShaderNames_ ## NAME ## MaskedInverted, \
        ShaderNames_ ## NAME ## PremultipliedAlpha, \
        ShaderNames_ ## NAME ## MaskedPremultipliedAlpha, \
        ShaderNames_ ## NAME ## MaskedInvertedPremultipliedAlpha

        // Copy
        ShaderNames_Copy,

        // SetupMask
        ShaderNames_SetupMask,

        // Normal
        CSM_CREATE_SHADER_NAMES(Normal),
        // Add
        CSM_CREATE_SHADER_NAMES(Add),
        // Mult
        CSM_CREATE_SHADER_NAMES(Mult),

        //Normal
        CSM_CREATE_SHADER_NAMES(NormalAtop),
        CSM_CREATE_SHADER_NAMES(NormalOut),
        CSM_CREATE_SHADER_NAMES(NormalConjointOver),
        CSM_CREATE_SHADER_NAMES(NormalDisjointOver),
        // 加算
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Add),
        // 加算(発光)
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(AddGlow),
        // 比較(暗)
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Darken),
        // 乗算
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Multiply),
        // 焼き込みカラー
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(ColorBurn),
        // 焼き込み(リニア)
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(LinearBurn),
        // 比較(明)
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Lighten),
        // スクリーン
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Screen),
        // 覆い焼きカラー
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(ColorDodge),
        // オーバーレイ
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Overlay),
        // ソフトライト
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(SoftLight),
        // ハードライト
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(HardLight),
        // リニアライト
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(LinearLight),
        // 色相
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Hue),
        // カラー
        CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Color),

        // ブレンドモードの組み合わせ数 = 加算(5.2以前) + 乗算(5.2以前) + (通常 + 加算 + 加算(発光) + 比較(暗) + 乗算 + 焼き込みカラー + 焼き込み(リニア) + 比較(明) + スクリーン + 覆い焼きカラー + オーバーレイ + ソフトライト + ハードライト + リニアライト + 色相 + カラー) * (over + atop + out + conjoint over + disjoint over)
        // シェーダの数 = コピー用 + マスク生成用 + (通常 + 加算 + 乗算 + ブレンドモードの組み合わせ数) * (マスク無 + マスク有 + マスク有反転 + マスク無の乗算済アルファ対応版 + マスク有の乗算済アルファ対応版 + マスク有反転の乗算済アルファ対応版)
        ShaderNames_ShaderCount,

#undef CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES
#undef CSM_CREATE_SHADER_NAMES
    };

    /**
     * @brief   どのシェーダーを利用するかを取得する
    */
    static csmInt32 GetShaderNamesBegin(const csmBlendMode blendMode);

    /**
     * @brief   シェーダプログラムを初期化する
     */
    void GenerateShaders(CubismRenderer_Metal* renderer);

    /**
     * @brief   ブレンドモード用シェーダプログラムを初期化する
     *
     * @param[in,out]   shaderSet            ->  初期化するシェーダーセット
     * @param[in]       vertShaderFileName   ->  頂点シェーダーファイル名
     * @param[in]       fragShaderFileName   ->  フラグメントシェーダーファイル名
     * @param[in]       vertShaderSrc        ->  頂点シェーダーのソース
     * @param[in]       fragShaderSrc        ->  フラグメントシェーダーのソース
     * @param[in]       device               ->  デバイスのインスタンス
     * @param[in]       renderer             ->  レンダラのインスタンス
     */
    void GenerateBlendShader(CubismShaderSet* shaderSet, const csmString& vertShaderFileName, const csmString& fragShaderFileName, const csmString& vertShaderSrc, const csmString& fragShaderSrc, id<MTLDevice> device, CubismRenderer_Metal* renderer);

    /**
     * @brief   CubismMatrix44をsimd::float4x4の形式に変換する
     */
    simd::float4x4 ConvertCubismMatrix44IntoSimdFloat4x4(CubismMatrix44& matrix);

    /**
     * @brief   使用するカラーチャンネルを設定
     *
     * @param[in]   shaderUniforms     ->  シェーダー用ユニフォームバッファ
     * @param[in]   contextBuffer      ->  クリッピングマスクのコンテキスト
     */
    void SetColorChannel(CubismShaderUniforms& shaderUniforms, CubismClippingContext_Metal* contextBuffer);

    /**
     * @brief   モデルにバインドされているテクスチャを、フラグメントシェーダーのテクスチャに設定する
     *
     * @param[in]   drawCommandBuffer     ->  コマンドバッファ
     * @param[in]   renderEncoder         ->  MTLRenderCommandEncoder
     * @param[in]   renderer              ->  レンダラのインスタンス
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index                 ->  描画オブジェクトのインデックス
     */
    void SetFragmentModelTexture(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                          , CubismRenderer_Metal* renderer, const CubismModel& model, const csmInt32 index);

    /**
     * @brief   頂点シェーダーに頂点インデックスと UV 座標を設定する
     *
     * @param[in]   drawCommandBuffer     ->  コマンドバッファ
     */
    void SetVertexBufferForVerticesAndUvs(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder);

    /**
     * @brief   シェーダプログラムをロードしてアドレス返す。
     *
     * @param[in]   vertShaderSrc   ->  頂点シェーダのソース
     * @param[in]   fragShaderSrc   ->  フラグメントシェーダのソース
     * @param[in]   shaderLib       ->  シェーダーライブラリ
     *
     * @return  シェーダプログラムのアドレス
     */
    ShaderProgram* LoadShaderProgram(const csmChar* vertShaderSrc, const csmChar* fragShaderSrc, id<MTLLibrary> shaderLib);

    /**
    * @brief   シェーダプログラムをロードしてアドレス返す。
    *
    * @param[in]   vertShaderSrc       ->  頂点シェーダのソース
    * @param[in]   fragShaderSrc       ->  フラグメントシェーダのソース
    * @param[in]   vertShaderLib       ->  頂点シェーダーライブラリ
    * @param[in]   fragShaderLib       ->  フラグメントシェーダーライブラリ
    *
    * @return  シェーダプログラムのアドレス
    */
    ShaderProgram* LoadShaderProgram(const csmString& vertShaderSrc, const csmString& fragShaderSrc, id<MTLLibrary> vertShaderLib, id<MTLLibrary> fragShaderLib);

    /**
    * @brief   シェーダーファイルからシェーダプログラムをロードしてアドレス返す。
    *
    * @param[in]   vertShaderFileName   ->  頂点シェーダのファイル名
    * @param[in]   vertShaderSrc        ->  頂点シェーダのソース
    * @param[in]   fragShaderFileName   ->  フラグメントシェーダのファイル名
    * @param[in]   fragShaderSrc        ->  フラグメントシェーダのソース
    * @param[in]   device               ->  デバイスのインスタンス
    *
    * @return  シェーダプログラムのアドレス
    */
    ShaderProgram* LoadShaderProgramFromFile(const csmString& vertShaderFileName, const csmString& fragShaderFileName, const csmString& vertShaderSrc, const csmString& fragShaderSrc, id<MTLDevice> device);

    id<MTLRenderPipelineState> MakeRenderPipelineState(id<MTLDevice> device, ShaderProgram* shaderProgram, int blendMode);
    id<MTLDepthStencilState> MakeDepthStencilState(id<MTLDevice> device);
    id<MTLSamplerState> MakeSamplerState(id<MTLDevice> device, CubismRenderer_Metal* renderer);


    csmVector<CubismShaderSet*> _shaderSets;   ///< ロードしたシェーダプログラムを保持する変数

};

}}}}
//------------ LIVE2D NAMESPACE ------------
