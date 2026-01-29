/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismNativeInclude_D3D11.hpp"
#include "CubismType_D3D11.hpp"
#include "Type/csmString.hpp"
#include "CubismFramework.hpp"
#include "Type/csmVector.hpp"

namespace Live2D { namespace Cubism { namespace Framework {
    enum ShaderNames
    {
#define CSM_CREATE_SHADER_NAMES(NAME) \
    ShaderNames_ ## NAME, \
    ShaderNames_ ## NAME ## Masked, \
    ShaderNames_ ## NAME ## MaskedInverted, \
    ShaderNames_ ## NAME ## PremultipliedAlpha, \
    ShaderNames_ ## NAME ## MaskedPremultipliedAlpha, \
    ShaderNames_ ## NAME ## MaskedInvertedPremultipliedAlpha

#define CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(COLOR) \
    CSM_CREATE_SHADER_NAMES(COLOR ## Over), \
    CSM_CREATE_SHADER_NAMES(COLOR ## Atop), \
    CSM_CREATE_SHADER_NAMES(COLOR ## Out), \
    CSM_CREATE_SHADER_NAMES(COLOR ## ConjointOver), \
    CSM_CREATE_SHADER_NAMES(COLOR ## DisjointOver)

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
        // 通常
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

        ShaderNames_Max,

#undef CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES
#undef CSM_CREATE_SHADER_NAMES
    };
}}}

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @bref    D3D11シェーダ管理
 */
class CubismShaderSet
{
public:
    CubismShaderSet()
        : VertexShader(NULL)
        , PixelShader(NULL)
        , IsOriginalVertexData(false)
    {
    }

    ID3D11VertexShader*     VertexShader;
    ID3D11PixelShader*      PixelShader;
    csmBool IsOriginalVertexData; // 頂点シェーダの解放可能性
};

/**
* @brief   Cubismで使用するシェーダ管理クラス
*           CubismRenderer_D3D11のstatic変数として一つだけ実体化される
*
*/
class CubismShader_D3D11
{
    friend class CubismRenderer_D3D11;

public:

    /**
    * @brief   privateなコンストラクタ
    */
    CubismShader_D3D11();

    /**
    * @brief   privateなデストラクタ
    */
    virtual ~CubismShader_D3D11();

    /**
     * @brief   シェーダプログラムを解放する
     */
    void ReleaseShaderProgram();

    /**
     * @brief   頂点シェーダの取得
     *
     */
    ID3D11VertexShader* GetVertexShader(csmUint32 assign);

    /**
     * @brief   ピクセルシェーダの取得
     *
     */
    ID3D11PixelShader* GetPixelShader(csmUint32 assign);

    /**
     * @brief   頂点宣言のデバイスへの設定、シェーダがまだ未設定ならロード
     */
    void SetupShader(ID3D11Device* device, ID3D11DeviceContext* renderContext);

private:
    /**
     * @brief   シェーダプログラムを初期化する
     */
    void GenerateShaders(ID3D11Device* device);

    /**
     * @brief   シェーダプログラムをロード
     *
     * @param[in]   device      使用デバイス
     *
     * @return  成功時はtrue、失敗時はfalse
     */
    csmBool LoadShaderProgram(ID3D11Device* device, bool isPs, csmInt32 assign, const csmChar* entryPoint, const csmString& shaderSrc, csmBool isFormat = false);

    csmVector<CubismShaderSet> _shaderSets;   ///< ロードしたシェーダプログラムを保持する変数
    ID3D11InputLayout*    _vertexFormat; ///< 描画で使用する型宣言
};

}}}}
//------------ LIVE2D NAMESPACE ------------
