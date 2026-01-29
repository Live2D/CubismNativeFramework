/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismNativeInclude_D3D9.hpp"
#include "CubismType_D3D9.hpp"
#include "Type/csmString.hpp"
#include "CubismFramework.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @bref    DX9シェーダエフェクト
 *
 */
class CubismShaderSet
{
public:
    CubismShaderSet()
        : _shaderEffect(NULL)
    {
    }

    ID3DXEffect*    _shaderEffect;
};

/**
* @brief   Cubismで使用するシェーダ管理クラス
*           CubismRenderer_D3D9のstatic変数として一つだけ実体化される
*
*/
class CubismShader_D3D9
{
    friend class CubismRenderer_D3D9;

public:

    /**
    * @brief   privateなコンストラクタ
    */
    CubismShader_D3D9();

    /**
    * @brief   privateなデストラクタ
    */
    virtual ~CubismShader_D3D9();

    /**
     * @brief   シェーダプログラムを解放する
     */
    void ReleaseShaderProgram();

    /**
     * @brief   シェーダプログラムエフェクトの取得
     *
     */
    ID3DXEffect* GetShaderEffect() const;

    /**
     * @brief   頂点宣言のデバイスへの設定、シェーダがまだ未設定ならロード
     */
    void SetupShader(LPDIRECT3DDEVICE9 pD3dDevice);

private:
    /**
     * @brief   シェーダプログラムをロード
     *
     * @param[in]   pD3dDevice      使用デバイス
     * @param[in]   shaderSrc       シェーダソースコード
     * @param[in]   effectPtr       シェーダエフェクトのポインタ
     *
     * @return  成功時はtrue、失敗時はfalse
     */
    static csmBool LoadShaderProgram(LPDIRECT3DDEVICE9 pD3dDevice, const csmString& shaderSrc, ID3DXEffect** effectPtr);

    /**
     * @brief   シェーダプログラムを初期化する
     */
    void GenerateShaders(LPDIRECT3DDEVICE9 pD3dDevice);


    ID3DXEffect*                    _shaderEffect; ///< CubismD3dでは一つのシェーダで内部テクニックの変更をする
    IDirect3DVertexDeclaration9*    _vertexFormat; ///< 描画で使用する型宣言
};

}}}}
//------------ LIVE2D NAMESPACE ------------
