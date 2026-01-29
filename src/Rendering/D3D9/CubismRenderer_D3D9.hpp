/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismNativeInclude_D3D9.hpp"

#include "../CubismRenderer.hpp"
#include "../CubismClippingManager.hpp"
#include "CubismFramework.hpp"
#include "Type/csmVector.hpp"
#include "Type/csmRectF.hpp"
#include "Math/CubismVector2.hpp"
#include "Type/csmMap.hpp"
#include "Rendering/D3D9/CubismRenderTarget_D3D9.hpp"
#include "CubismShader_D3D9.hpp"
#include "CubismRenderState_D3D9.hpp"
#include "CubismDeviceInfo_D3D9.hpp"
#include "CubismType_D3D9.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

//  前方宣言
class CubismRenderer_D3D9;
class CubismClippingContext_D3D9;

/**
 * @brief D3DXMATRIXに変換
 */
D3DXMATRIX ConvertToD3DX(CubismMatrix44& mtx);

/**
 * @brief  クリッピングマスクの処理を実行するクラス
 */
class CubismClippingManager_DX9 : public CubismClippingManager<CubismClippingContext_D3D9, CubismRenderTarget_D3D9>
{
public:
    /**
     * @brief   クリッピングコンテキストを作成する。モデル描画時に実行する。
     *
     * @param[in]   device              ->  デバイス
     * @param[in]   renderState         ->  レンダーステート
     * @param[in]   model               ->  モデルのインスタンス
     * @param[in]   renderer            ->  レンダラのインスタンス
     * @param[in]   currentRenderTarget ->  現在使用中のバッファ
     * @param[in]   drawableObjectType  ->  描画オブジェクトのタイプ
     */
    void SetupClippingContext(LPDIRECT3DDEVICE9 device, CubismRenderState_D3D9* renderState, CubismModel& model, CubismRenderer_D3D9* renderer, csmInt32 currentRenderTarget, CubismRenderer::DrawableObjectType drawableObjectType);
};

/**
 * @brief   クリッピングマスクのコンテキスト
 */
class CubismClippingContext_D3D9 : public CubismClippingContext
{
    friend class CubismClippingManager_DX9;
    friend class CubismShader_D3D9;
    friend class CubismRenderer_D3D9;

public:
    /**
     * @brief   引数付きコンストラクタ
     *
     * @param[in]   manager                  ->  マスクを管理しているマネージャのインスタンス
     * @param[in]   model                    ->  モデルのインスタンス
     * @param[in]   clippingDrawableIndices  ->  クリップしているDrawableのインデックスリスト
     * @param[in]   clipCount                ->  クリップしているDrawableの個数
     */
    CubismClippingContext_D3D9(CubismClippingManager<CubismClippingContext_D3D9, CubismRenderTarget_D3D9>* manager, CubismModel& model, const csmInt32* clippingDrawableIndices, csmInt32 clipCount);

    /**
     * @brief   デストラクタ
     */
    virtual ~CubismClippingContext_D3D9();

    /**
     * @brief   このマスクを管理するマネージャのインスタンスを取得する。
     *
     * @return  クリッピングマネージャのインスタンス
     */
    CubismClippingManager<CubismClippingContext_D3D9, CubismRenderTarget_D3D9>* GetClippingManager();

private:
    CubismClippingManager<CubismClippingContext_D3D9, CubismRenderTarget_D3D9>* _owner;        ///< このマスクを管理しているマネージャのインスタンス
};

/**
 * @brief   DirectX9用の描画命令を実装したクラス
 *
 */
class CubismRenderer_D3D9 : public CubismRenderer
{
    friend class CubismRenderer;
    friend class CubismClippingManager_DX9;
    friend class CubismShader_D3D9;

public:
    /**
     * @brief    レンダラを作成するための各種設定
     * 　　　　　モデルロード前に毎回設定されている必要あり
     *
     * @param[in]   bufferSetNum -> 1パーツに付き作成するバッファ数
     * @param[in]   device       -> 使用デバイス
     */
    static void SetConstantSettings(csmUint32 bufferSetNum, LPDIRECT3DDEVICE9 device);

    /**
     * @brief    デバイスを設定する
     *
     * @return 設定に成功したらtrueを返す
     */
    csmBool OnDeviceChanged();

    /**
     * @brief  Cubism描画関連の先頭で行う処理。
     *          各フレームでのCubism処理前にこれを呼んでもらう
     */
    void StartFrame();

    /**
     * @brief  Cubism描画関連の終了時行う処理。
     *          各フレームでのCubism処理前にこれを呼んでもらう
     */
    void EndFrame();

    /**
     * @brief   レンダラの初期化処理を実行する
     *           引数に渡したモデルからレンダラの初期化処理に必要な情報を取り出すことができる
     *
     * @param[in]  model -> モデルのインスタンス
     */
    virtual void CubismRenderer_D3D9::Initialize(CubismModel* model);

    /**
     * @brief   レンダラの初期化処理を実行する
     *           引数に渡したモデルからレンダラの初期化処理に必要な情報を取り出すことができる
     *
     * @param[in]  model           -> モデルのインスタンス
     * @param[in]  maskBufferCount -> マスクの分割数
     */
    virtual void Initialize(Framework::CubismModel* model, csmInt32 maskBufferCount) override;

    /**
     * @bref オフスクリーンの親を探して設定する
     *
     * @param model -> モデルのインスタンス
     * @param offscreenCount -> オフスクリーンの数
     */
    void SetupParentOffscreens(const CubismModel* model, csmInt32 offscreenCount);

    /**
     * @brief   レンダリング前のフレーム先頭で呼ばれる
     */
    void FrameRenderingInit();

    /**
     * @brief   テクスチャのバインド処理<br>
     *           CubismRendererにテクスチャを設定し、CubismRenderer中でその画像を参照するためのIndex値を戻り値とする
     *
     * @param[in]   modelTextureAssign  ->  セットするモデルテクスチャのアサイン番号
     * @param[in]   texture            ->  描画に使用するテクスチャ
     *
     */
    void BindTexture(csmUint32 modelTextureAssign, const LPDIRECT3DTEXTURE9 texture);

    /**
     * @brief   バインドされたテクスチャのリストを取得する
     *
     * @return  テクスチャのアドレスのリスト
     */
    const csmMap<csmInt32, LPDIRECT3DTEXTURE9>& GetBindedTextures() const;

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
     * @brief  モデルで使用するレンダーターゲットのバッファをコピーする
     *
     * @return モデルで使用するレンダーターゲットのバッファへのポインタ
     */
    LPDIRECT3DTEXTURE9 CopyModelRenderTarget();

    /**
     * @brief  任意のバッファをコピーする
     *
     * @param srcBuffer コピー元のバッファ
     *
     * @return コピー後のバッファへのポインタ
     */
    LPDIRECT3DTEXTURE9 CopyRenderTarget(CubismRenderTarget_D3D9& srcBuffer);

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
     * @brief  描画オブジェクトのクリッピングマスクのバッファを取得する
     *
     * @param[in]  backbufferNum -> バッファの番号
     * @param[in]  index         -> インデックス
     *
     * @return 描画オブジェクトのクリッピングマスクのバッファへのポインタ
     */
    CubismRenderTarget_D3D9* GetDrawableMaskBuffer(csmUint32 backbufferNum, csmInt32 index);

    /**
     * @brief  オフスクリーンのクリッピングマスクのバッファを取得する
     *
     * @param[in]  backbufferNum -> バッファの番号
     * @param[in]  index         -> インデックス
     *
     * @return オフスクリーンのクリッピングマスクのバッファへのポインタ
     */
    CubismRenderTarget_D3D9* GetOffscreenMaskBuffer(csmUint32 backbufferNum, csmInt32 index);

    /**
     * @brief  現在のオフスクリーンのフレームバッファを取得する
     *
     * @return 現在のオフスクリーンのフレームバッファを返す
     */
    CubismOffscreenRenderTarget_D3D9* GetCurrentOffscreen() const;

protected:
    /**
     *  @brief  CubismRenderStateにデフォルトの設定をセットする
     *
     * @param[in]   width         -> ウインドウの幅
     * @param[in]   height        -> ウインドウの高さ
     */
    void SetDefaultRenderState(csmUint32 width, csmUint32 height);

    /**
     * @brief   コンストラクタ
     *
     * @param[in] width -> モデルを描画したバッファの幅
     * @param[in] height -> モデルを描画したバッファの高さ
     */
    CubismRenderer_D3D9(csmUint32 width, csmUint32 height);

    /**
     * @brief   デストラクタ
     */
    virtual ~CubismRenderer_D3D9();

    /**
     * @brief   モデルを描画する実際の処理
     *
     */
    virtual void DoDrawModel() override;

    /**
     * @brief   描画オブジェクト（アートメッシュ、オフスクリーン）を描画するループ処理
     */
    void DrawObjectLoop();

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
     * @brief   Drawable が属していないオフスクリーンを確定描画し、親オフスクリーンへ辿る。
     *
     * @param[in]   drawableIndex ->  描画対象のメッシュのインデックス
     */
    void FlushOffscreenChainForDrawable(csmInt32 drawableIndex);

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
    void DrawOffscreen(CubismOffscreenRenderTarget_D3D9* offscreen);

    /**
     * @brief   描画オブジェクト（アートメッシュ）を描画する。<br>
     *           ポリゴンメッシュとテクスチャ番号をセットで渡す。
     *
     * @param[in]   model        ->  描画対象のモデル
     * @param[in]   index        ->  描画すべき対象のインデックス
     */
    void DrawMeshDX9(const CubismModel& model, const csmInt32 index);

    /**
     * @brief   オフスクリーンを描画する。
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   offscreen ->  描画対象のオフスクリーン
     */
    void DrawOffscreenDX9(const CubismModel& model, CubismOffscreenRenderTarget_D3D9* offscreen);

    /**
     * @brief  描画用に使用するシェーダの設定・コンスタントバッファの設定などを行い、描画を実行
     *
     * @param[in]   model           ->  描画対象のモデル
     * @param[in]   index           ->  描画すべき対象のインデックス
     */
    void ExecuteDrawForDrawable(const CubismModel& model, const csmInt32 index);

    /**
     * @brief  オフスクリーン用に使用するシェーダの設定・コンスタントバッファの設定などを行い、描画を実行
     *
     * @param[in]   model           ->  描画対象のモデル
     * @param[in]   offscreen       ->  描画すべき対象
     */
    void ExecuteDrawForOffscreen(const CubismModel& model, CubismOffscreenRenderTarget_D3D9* offscreen);

    /**
     * @brief  マスク作成用に使用するシェーダの設定・コンスタントバッファの設定などを行い、描画を実行
     *
     * @param[in]   model           ->  描画対象のモデル
     * @param[in]   index           ->  描画すべき対象のインデックス
     */
    void ExecuteDrawForMask(const CubismModel& model, const csmInt32 index);

    /**
     * @brief   レンダーターゲットからのシェーダプログラムの一連のセットアップを実行する
     *
     */
    void ExecuteDrawForRenderTarget();

private:
    // Prevention of copy Constructor
    CubismRenderer_D3D9(const CubismRenderer_D3D9&);

    /**
     * @brief   バッファ作成
     */
    void CreateMeshBuffer();

    /**
     * @brief   描画開始時の追加処理。
     *           モデルを描画する前にクリッピングマスクに必要な処理を実装している。
     */
    void PreDraw();

    /**
     * @brief   描画完了後の追加処理。
     *           ダブル・トリプルバッファリングに必要な処理を実装している。
     */
    void PostDraw();

    /**
     * @brief   モデル描画直前のステートを保持する
     */
    virtual void SaveProfile() override;

    /**
     * @brief   モデル描画直前のステートを保持する
     */
    virtual void RestoreProfile() override;

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
    void SetClippingContextBufferForMask(CubismClippingContext_D3D9* clip);

    /**
     * @brief   マスクテクスチャに描画するクリッピングコンテキストを取得する。
     *
     * @return  マスクテクスチャに描画するクリッピングコンテキスト
     */
    CubismClippingContext_D3D9* GetClippingContextBufferForMask() const;

    /**
     * @brief   drawableで画面上に描画するクリッピングコンテキストをセットする。
     *
     * @param[in]   clip     ->  drawableで画面上に描画するクリッピングコンテキスト
     */
    void SetClippingContextBufferForDrawable(CubismClippingContext_D3D9* clip);

    /**
     * @brief   drawableで画面上に描画するクリッピングコンテキストを取得する。
     *
     * @return  drawableで画面上に描画するクリッピングコンテキスト
     */
    CubismClippingContext_D3D9* GetClippingContextBufferForDrawable() const;

    /**
     * @brief   offscreenで画面上に描画するクリッピングコンテキストをセットする。
     *
     * @param[in]   clip     ->  offscreenで画面上に描画するクリッピングコンテキスト
     */
    void SetClippingContextBufferForOffscreen(CubismClippingContext_D3D9* clip);

    /**
     * @brief   offscreenで画面上に描画するクリッピングコンテキストを取得する。
     *
     * @return  offscreenで画面上に描画するクリッピングコンテキスト
     */
    CubismClippingContext_D3D9* GetClippingContextBufferForOffscreen() const;

    /**
     * @brief   GetDrawableVertices,GetDrawableVertexUvsの内容を内部バッファへコピー
     */
    void CopyToBuffer(csmInt32 drawAssign, const csmInt32 vcount, const csmFloat32* varray, const csmFloat32* uvarray);

    /**
     * @brief   modelのindexにバインドされているテクスチャを返す。<br>
     *          もしバインドしていないindexを指定した場合NULLが返る。
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のインデックス
     *
     * @return  バインドされたテクスチャ
     */
    LPDIRECT3DTEXTURE9 GetTextureWithIndex(const CubismModel& model, const csmInt32 index);

    /**
     * @brief   マスク生成時かを判定する
     *
     * @return  判定値
     */
    const csmBool inline IsGeneratingMask() const;

    /**
     * @brief   ブレンドモードを設定する
     *
     * @param[in]   blendMode    ->  ブレンドモード
     */
    void SetBlendMode(CubismBlendMode blendMode) const;

    /**
     * @brief   ブレンドモードを設定する
     *
     * @param[in]   blendMode      ->  ブレンドモード
     * @param[in]   isBlendMode    ->  ブレンドモードを利用するかを格納する
     */
    void SetBlendMode(csmBlendMode blendMode, csmBool& isBlendMode) const;

    /**
     * @brief   drawable用のD3D9のテクニックを設定する
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のインデックス
     * @param[in]   isBlendMode ->  ブレンドモードを利用するか
     * @param[in]   blendMode   ->  ブレンドモード
     */
    void SetTechniqueForDrawable(const CubismModel& model, csmInt32 index, csmBool isBlendMode = false, csmBlendMode blendMode = csmBlendMode()) const;

    /**
     * @brief   offscreen用のD3D9のテクニックを設定する
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のインデックス
     * @param[in]   isBlendMode ->  ブレンドモードを利用するか
     * @param[in]   blendMode   ->  ブレンドモード
     */
    void SetTechniqueForOffscreen(const CubismModel& model, csmInt32 index, csmBool isBlendMode = false, csmBlendMode blendMode = csmBlendMode()) const;

    /**
     * @brief   RenderStateManagerにテクスチャフィルターを設定する
     */
    void SetTextureFilter() const;

    /**
     * @brief   RenderStateManagerにオフスクリーン用テクスチャフィルターを設定する
     */
    void SetOffscreenTextureFilter() const;

    /**
     * @brief  色関連の定数バッファを設定
     *
     * @param[in]   shaderEffect    -> シェーダーエフェクト
     * @param[in]   baseColor       ->  ベースカラー
     * @param[in]   multiplyColor   ->  乗算カラー
     * @param[in]   screenColor     ->  スクリーンカラー
     */
    void SetColorVectors(ID3DXEffect* shaderEffect, CubismTextureColor& baseColor, CubismTextureColor& multiplyColor, CubismTextureColor& screenColor);

    /**
     * @brief  drawable用の描画実行時のテクスチャを設定
     *
     * @param[in]   model           ->  描画対象のモデル
     * @param[in]   index           ->  描画すべき対象のインデックス
     * @param[in]   shaderEffect    -> シェーダーエフェクト
     * @param[in]   isBlendMode ->  ブレンドモードを利用するか
     */
    void SetExecutionTexturesForDrawable(const CubismModel& model, const csmInt32 index, ID3DXEffect* shaderEffect, csmBool isBlendMode = false);

    /**
     * @brief  offscreen用の描画実行時のテクスチャを設定
     *
     * @param[in]   model           ->  描画対象のモデル
     * @param[in]   offscreen       ->  描画すべき対象のオフスクリーン
     * @param[in]   shaderEffect    -> シェーダーエフェクト
     */
    void SetExecutionTexturesForOffscreen(const CubismModel& model, const CubismOffscreenRenderTarget_D3D9* offscreen, ID3DXEffect* shaderEffect);

    /**
     * @brief  描画に使用するカラーチャンネルを設定
     *
     * @param[in]   shaderEffect    -> シェーダーエフェクト
     * @param[in]   contextBuffer   -> 描画コンテキスト
     */
    void SetColorChannel(ID3DXEffect* shaderEffect, CubismClippingContext_D3D9* contextBuffer);

    /**
     * @brief  描画に使用するプロジェクション行列を更新する
     *
     * @param[in]   shaderEffect    -> シェーダーエフェクト
     * @param[in]   matrix          ->  設定するプロジェクション行列
     */
    void SetProjectionMatrix(ID3DXEffect* shaderEffect, CubismMatrix44& matrix);

    /**
     * @brief  引数を指定して DrawIndexedPrimitiveUP 描画命令を実行
     *
     * @param[in]   model           ->  描画対象のモデル
     * @param[in]   index           ->  描画すべき対象のインデックス
     */
    void DrawIndexedPrimiteveWithSetup(const CubismModel& model, const csmInt32 index);

    LPDIRECT3DDEVICE9 _device;   ///< 使用デバイス。モデルロード前に設定されている必要あり。
    CubismDeviceInfo_D3D9* _deviceInfo; ///< デバイスに紐づいている情報
    csmUint32 _drawableNum;         ///< _vertexBuffers, _indexBuffersの確保数

    /**
     * @note D3D9ではDrawPrimitiveUPで描画コマンドを積んでおり、
     * この場合は頂点メモリをユーザー側で保存し続ける必要がないため、一本の配列のみ持つ実装となっている。
     */
    CubismVertexD3D9** _vertexStore;           ///< 頂点をストアしておく領域
    csmUint16** _indexStore;            ///< インデックスをストアしておく領域

    csmInt32 _commandBufferNum;      ///< 描画バッファを複数作成する場合の数
    csmInt32 _commandBufferCurrent;  ///< 現在使用中のバッファ番号

    csmMap<csmInt32, LPDIRECT3DTEXTURE9> _textures;                      ///< モデルが参照するテクスチャとレンダラでバインドしているテクスチャとのマップ

    csmVector<csmInt32> _sortedObjectsIndexList;           ///< 描画オブジェクトのインデックスを描画順に並べたリスト
    csmVector<DrawableObjectType> _sortedObjectsTypeList;  ///< 描画オブジェクトの種別を描画順に並べたリスト

    IDirect3DVertexBuffer9* _copyVertexBuffer; ///< コピー用の頂点バッファ
    IDirect3DVertexBuffer9* _offscreenVertexBuffer; ///< オフスクリーン用の頂点バッファ
    IDirect3DIndexBuffer9* _copyIndexBuffer; ///< コピー用のインデックスバッファ
    csmVector<CubismRenderTarget_D3D9> _modelRenderTargets; ///< モデル全体を描画する先のフレームバッファ
    csmVector<csmVector<CubismRenderTarget_D3D9> > _drawableMasks;  ///< Drawableのマスク描画用のフレームバッファ
    csmVector<csmVector<CubismRenderTarget_D3D9> > _offscreenMasks; ///< オフスクリーン機能マスク描画用のフレームバッファ

    CubismClippingManager_DX9* _drawableClippingManager;        ///< クリッピングマスク管理オブジェクト
    CubismClippingManager_DX9* _offscreenClippingManager;       ///< クリッピングマスク管理オブジェクト
    CubismClippingContext_D3D9* _clippingContextBufferForMask;  ///< マスクテクスチャに描画するためのクリッピングコンテキスト
    CubismClippingContext_D3D9* _clippingContextBufferForDrawable;   ///< 画面上描画するためのクリッピングコンテキスト
    CubismClippingContext_D3D9* _clippingContextBufferForOffscreen;  ///< 画面上描画するためのクリッピングコンテキスト

    csmVector<CubismOffscreenRenderTarget_D3D9> _offscreenList; ///< モデルのオフスクリーン
    CubismOffscreenRenderTarget_D3D9* _currentOffscreen;        ///< 現在のオフスクリーンのフレームバッファ
};

}}}}
//------------ LIVE2D NAMESPACE ------------
