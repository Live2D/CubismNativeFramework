/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismNativeInclude_D3D11.hpp"

#include "../CubismRenderer.hpp"
#include "../CubismClippingManager.hpp"
#include "CubismFramework.hpp"
#include "CubismType_D3D11.hpp"
#include "Type/csmVector.hpp"
#include "Type/csmRectF.hpp"
#include "Math/CubismVector2.hpp"
#include "Type/csmMap.hpp"
#include "Rendering/D3D11/CubismRenderTarget_D3D11.hpp"
#include "Rendering/D3D11/CubismShader_D3D11.hpp"
#include "Rendering/D3D11/CubismDeviceInfo_D3D11.hpp"
#include "CubismRenderState_D3D11.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

//  前方宣言
class CubismRenderer_D3D11;
class CubismClippingContext_D3D11;

/**
 * @brief DirectX::XMMATRIXに変換
 */
DirectX::XMMATRIX ConvertToD3DX(CubismMatrix44& mtx);

/**
 * @brief  クリッピングマスクの処理を実行するクラス
 *
 */
class CubismClippingManager_D3D11 : public CubismClippingManager<CubismClippingContext_D3D11, CubismRenderTarget_D3D11>
{
public:
    /**
     * @brief   クリッピングコンテキストを作成する。モデル描画時に実行する。
     *
     * @param[in]   device              ->  デバイス
     * @param[in]   context             ->  コンテキスト
     * @param[in]   renderState         ->  レンダーステート
     * @param[in]   model               ->  モデルのインスタンス
     * @param[in]   renderer            ->  レンダラのインスタンス
     * @param[in]   currentRenderTarget ->  現在使用中のバッファ
     * @param[in]   drawableObjectType  ->  描画オブジェクトのタイプ
     */
    void SetupClippingContext(ID3D11Device* device, ID3D11DeviceContext* context, CubismRenderState_D3D11* renderState, CubismModel& model, CubismRenderer_D3D11* renderer, csmInt32 currentRenderTarget, CubismRenderer::DrawableObjectType drawableObjectType);
};

/**
 * @brief   クリッピングマスクのコンテキスト
 */
class CubismClippingContext_D3D11 : public CubismClippingContext
{
    friend class CubismClippingManager_D3D11;
    friend class CubismShader_D3D11;
    friend class CubismRenderer_D3D11;

public:
    /**
     * @brief   引数付きコンストラクタ
     *
     */
    CubismClippingContext_D3D11(CubismClippingManager<CubismClippingContext_D3D11, CubismRenderTarget_D3D11>* manager, CubismModel& model, const csmInt32* clippingDrawableIndices, csmInt32 clipCount);

    /**
     * @brief   デストラクタ
     */
    virtual ~CubismClippingContext_D3D11();

    /**
     * @brief   このマスクを管理するマネージャのインスタンスを取得する。
     *
     * @return  クリッピングマネージャのインスタンス
     */
    CubismClippingManager<CubismClippingContext_D3D11, CubismRenderTarget_D3D11>* GetClippingManager();

    CubismClippingManager<CubismClippingContext_D3D11, CubismRenderTarget_D3D11>* _owner;        ///< このマスクを管理しているマネージャのインスタンス
};

/**
 * @brief   DirectX11用の描画命令を実装したクラス
 *
 */
class CubismRenderer_D3D11 : public CubismRenderer
{
    friend class CubismRenderer;
    friend class CubismClippingManager_D3D11;
    friend class CubismShader_D3D11;

public:
    /**
     * @brief    レンダラを作成するための各種設定
     * 　　　　　モデルロード前に毎回設定されている必要あり
     *
     * @param[in]   bufferSetNum -> 1パーツに付き作成するバッファ数
     * @param[in]   device       -> 使用デバイス
     */
    static void SetConstantSettings(csmUint32 bufferSetNum, ID3D11Device* device);

    /**
     * @brief    デバイスを設定する
     *
     * @return 設定に成功したらtrueを返す
     */
    csmBool OnDeviceChanged();

    /**
     * @brief  Cubism描画関連の先頭で行う処理。
     *          各フレームでのCubism処理前にこれを呼んでもらう
     *
     * @param[in]  context -> コンテキスト
     */
    void StartFrame(ID3D11DeviceContext* context);

    /**
     * @brief  Cubism描画関連の終了時行う処理。
     *          各フレームでのCubism処理前にこれを呼んでもらう
     */
    void EndFrame();

    /**
     * @brief   レンダラの初期化処理を実行する<br>
     *           引数に渡したモデルからレンダラの初期化処理に必要な情報を取り出すことができる
     *
     * @param[in]  model -> モデルのインスタンス
     */
    virtual void Initialize(Framework::CubismModel* model) override;

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
     * @brief   コマンドバッファーの開放
     */
    void ReleaseCommandBuffer();

    /**
     * @brief   OpenGLテクスチャのバインド処理
     *           CubismRendererにテクスチャを設定し、CubismRenderer中でその画像を参照するためのIndex値を戻り値とする
     *
     * @param[in]   modelTextureAssign  ->  セットするモデルテクスチャのアサイン番号
     * @param[in]   textureView            ->  描画に使用するテクスチャ
     *
     */
    void BindTexture(csmUint32 modelTextureAssign, ID3D11ShaderResourceView* textureView);

    /**
     * @brief   D3D11にバインドされたテクスチャのリストを取得する
     *
     * @return  テクスチャのアドレスのリスト
     */
    const csmMap<csmInt32, ID3D11ShaderResourceView*>& GetBindedTextures() const;

    /**
     * @brief  drawable用のクリッピングマスクバッファのサイズを設定する<br>
     *         マスク用のRenderTargetを破棄・再作成するため処理コストは高い。
     *
     * @param[in]   width         -> ウインドウの幅
     * @param[in]   height        -> ウインドウの高さ
     */
    void SetDrawableClippingMaskBufferSize(csmFloat32 width, csmFloat32 height);

    /**
     * @brief  drawable用のレンダーテクスチャの枚数を取得する。
     *
     * @return  レンダーテクスチャの枚数
     */
    csmInt32 GetDrawableRenderTextureCount() const;

    /**
     * @brief  drawable用のクリッピングマスクバッファのサイズを取得する
     *
     * @return クリッピングマスクバッファのサイズ
     */
    CubismVector2 GetDrawableClippingMaskBufferSize() const;

    /**
     * @brief  offscreen用のクリッピングマスクバッファのサイズを設定する<br>
     *         マスク用のRenderTargetを破棄・再作成するため処理コストは高い。
     *
     * @param[in]   width         -> ウインドウの幅
     * @param[in]   height        -> ウインドウの高さ
     */
    void SetOffscreenClippingMaskBufferSize(csmFloat32 width, csmFloat32 height);

    /**
     * @brief  offscreen用のレンダーテクスチャの枚数を取得する。
     *
     * @return  レンダーテクスチャの枚数
     */
    csmInt32 GetOffscreenRenderTextureCount() const;

    /**
     * @brief  offscreen用のクリッピングマスクバッファのサイズを取得する
     *
     * @return クリッピングマスクバッファのサイズ
     */
    CubismVector2 GetOffscreenClippingMaskBufferSize() const;

    /**
     * @brief  drawable用のクリッピングマスクのバッファを取得する
     *
     * @param[in]  backbufferNum -> バッファの番号
     * @param[in]  index         -> インデックス
     *
     * @return クリッピングマスクのバッファへの参照
     */
    CubismRenderTarget_D3D11* GetDrawableMaskBuffer(csmUint32 backbufferNum, csmInt32 index);

    /**
     * @brief  offscreen用のクリッピングマスクのバッファを取得する
     *
     * @param[in]  backbufferNum -> バッファの番号
     * @param[in]  index         -> インデックス
     *
     * @return クリッピングマスクのバッファへの参照
     */
    CubismRenderTarget_D3D11* GetOffscreenMaskBuffer(csmUint32 backbufferNum, csmInt32 index);

   /**
     * @brief  現在のオフスクリーンのフレームバッファを取得する
     *
     * @return 現在のオフスクリーンのフレームバッファを返す
     */
    CubismOffscreenRenderTarget_D3D11* GetCurrentOffscreen() const;

    /**
     * @brief  モデルで使用するレンダーターゲットのバッファをコピーする
     *
     * @return モデルで使用するレンダーターゲットのバッファへのポインタ
     */
    ID3D11ShaderResourceView* CopyModelRenderTarget();

    /**
     * @brief  任意のバッファをコピーする
     *
     * @param srcBuffer コピー元のバッファ
     *
     * @return コピー後のバッファへのポインタ
     */
    ID3D11ShaderResourceView* CopyRenderTarget(CubismRenderTarget_D3D11& srcBuffer);

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
     * @param[in]   width         -> ウインドウの幅
     * @param[in]   height        -> ウインドウの高さ
     */
    CubismRenderer_D3D11(csmUint32 width, csmUint32 height);

    /**
     * @brief   デストラクタ
     */
    virtual ~CubismRenderer_D3D11() override;

    /**
     * @brief   モデルを描画する実際の処理
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
    void DrawOffscreen(CubismOffscreenRenderTarget_D3D11* offscreen);

    /**
     * @brief   描画オブジェクト（アートメッシュ）を描画する。
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のインデックス
     */
    void DrawMeshDX11(const CubismModel& model, const csmInt32 index);

    /**
     * @brief   オフスクリーンを描画する。
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   offscreen ->  描画対象のオフスクリーン
     */
    void DrawOffscreenDX11(const CubismModel& model, CubismOffscreenRenderTarget_D3D11* offscreen);

private:
    /**
     * @brief  描画用に使用するシェーダの設定・コンスタントバッファの設定などを行い、マスク描画を実行
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のインデックス
     */
    void ExecuteDrawForMask(const CubismModel& model, const csmInt32 index);

    /**
     * @brief  マスク作成用に使用するシェーダの設定・コンスタントバッファの設定などを行い、描画を実行
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のインデックス
     */
    void ExecuteDrawForDrawable(const CubismModel& model, const csmInt32 index);

    /**
     * @brief  オフスクリーン用に使用するシェーダの設定・コンスタントバッファの設定などを行い、描画を実行
     *
     * @param[in]   model           ->  描画対象のモデル
     * @param[in]   offscreen       ->  描画すべき対象
     */
    void ExecuteDrawForOffscreen(const CubismModel& model, CubismOffscreenRenderTarget_D3D11* offscreen);

    /**
     * @brief  指定されたメッシュインデックスに対して描画命令を実行する
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のインデックス
     */
    void DrawDrawableIndexed(const CubismModel& model, const csmInt32 index);

    /**
     * @brief   モデルのレンダーターゲットからのシェーダプログラムの一連のセットアップを実行する
     */
    void ExecuteDrawForRenderTarget();

    // Prevention of copy Constructor
    CubismRenderer_D3D11(const CubismRenderer_D3D11&);

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
    virtual void BeforeDrawModelRenderTarget() override;

    /**
     * @brief   モデル描画後のオフスクリーン設定
     */
    virtual void AfterDrawModelRenderTarget() override;

    /**
     * @brief   マスクテクスチャに描画するクリッピングコンテキストをセットする。
     */
    void SetClippingContextBufferForMask(CubismClippingContext_D3D11* clip);

    /**
     * @brief   マスクテクスチャに描画するクリッピングコンテキストを取得する。
     *
     * @return  マスクテクスチャに描画するクリッピングコンテキスト
     */
    CubismClippingContext_D3D11* GetClippingContextBufferForMask() const;

    /**
     * @brief   drawable用の画面上に描画するクリッピングコンテキストをセットする。
     */
    void SetClippingContextBufferForDrawable(CubismClippingContext_D3D11* clip);

    /**
     * @brief   drawable用の画面上に描画するクリッピングコンテキストを取得する。
     *
     * @return  画面上に描画するクリッピングコンテキスト
     */
    CubismClippingContext_D3D11* GetClippingContextBufferForDrawable() const;

    /**
     * @brief   offscreen用の画面上に描画するクリッピングコンテキストをセットする。
     */
    void SetClippingContextBufferForOffscreen(CubismClippingContext_D3D11* clip);

    /**
     * @brief   offscreen用の画面上に描画するクリッピングコンテキストを取得する。
     *
     * @return  画面上に描画するクリッピングコンテキスト
     */
    CubismClippingContext_D3D11* GetClippingContextBufferForOffscreen() const;

    /**
     * @brief   GetDrawableVertices,GetDrawableVertexUvsの内容をバッファへコピー
     */
    void CopyToBuffer(ID3D11DeviceContext* renderContext, csmInt32 drawAssign, const csmInt32 vcount, const csmFloat32* varray, const csmFloat32* uvarray);

    /**
     * @brief   modelのindexにバインドされているテクスチャを返す。<br>
     *          もしバインドしていないindexを指定した場合NULLが返る。
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のインデックス
     */
    ID3D11ShaderResourceView* GetTextureViewWithIndex(const CubismModel& model, const csmInt32 index);

    /**
     *  @brief   ブレンドステートをセットする。
     */
    void SetBlendState(const CubismBlendMode blendMode) const;

    /**
     * @brief   ブレンドモードを設定する
     *
     * @param[in]   blendMode      ->  ブレンドモード
     * @param[in]   isBlendMode    ->  ブレンドモードを利用するかを格納する
     */
    void SetBlendMode(csmBlendMode blendMode, csmBool& isBlendMode) const;

    /**
     * @brief   drawable用のシェーダをコンテキストにセットする
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のインデックス
     * @param[in]   blendMode   ->  ブレンドモード
     */
    void SetShaderForDrawable(const CubismModel& model, const csmInt32 index, csmBlendMode blendMode);

    /**
     * @brief   offscreen用のシェーダをコンテキストにセットする
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のインデックス
     * @param[in]   blendMode   ->  ブレンドモード
     */
    void SetShaderForOffscreen(const CubismModel& model, const csmInt32 index, csmBlendMode blendMode);

    /**
     * @brief  drawable用の描画に使用するテクスチャを設定する。
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   index       ->  描画対象のインデックス
     * @param[in]   isBlendMode ->  ブレンドモードを使用するか
     */
    void SetTextureViewForDrawable(const CubismModel& model, const csmInt32 index, csmBool isBlendMode = false);

    /**
     * @brief  offscreen用の描画に使用するテクスチャを設定する。
     *
     * @param[in]   model       ->  描画対象のモデル
     * @param[in]   offscreen   ->  オフスクリーン
     */
    void SetTextureViewForOffscreen(const CubismModel& model, const CubismOffscreenRenderTarget_D3D11* offscreen);

    /**
     * @brief  色関連の定数バッファを設定する
     *
     * @param[in]   cb             ->  設定する定数バッファ
     * @param[in]   baseColor      ->  ベースカラー
     * @param[in]   multiplyColor  ->  乗算カラー
     * @param[in]   screenColor    ->  スクリーンカラー
     */
    void SetColorConstantBuffer(CubismConstantBufferD3D11& cb, CubismTextureColor& baseColor, CubismTextureColor& multiplyColor, CubismTextureColor& screenColor);

    /**
     * @brief  描画に使用するカラーチャンネルを設定
     *
     * @param[in]   cb            ->  設定する定数バッファ
     * @param[in]   contextBuffer ->  描画コンテキスト
     */
    void SetColorChannel(CubismConstantBufferD3D11& cb, CubismClippingContext_D3D11* contextBuffer);

    /**
     * @brief  描画に使用するプロジェクション行列を更新する
     *
     * @param[in]   cb          ->  設定する定数バッファ
     * @param[in]   matrix      ->  設定するプロジェクション行列
     */
    void SetProjectionMatrix(CubismConstantBufferD3D11& cb, CubismMatrix44 matrix);

    /**
     * @brief  定数バッファを更新する
     *
     * @param[in]   cb          ->  書き込む定数バッファ情報
     * @param[in]   index       ->  描画対象のインデックス
     */
    void UpdateConstantBuffer(CubismConstantBufferD3D11& cb, csmInt32 index);

    /**
     * @brief   異方向フィルタリングの値によってサンプラーを設定する。
     */
    void SetSamplerAccordingToAnisotropy();

    /**
     * @brief   マスク生成時かを判定する
     *
     * @return  判定値
     */
    const csmBool inline IsGeneratingMask() const;

    ID3D11Device* _device;        ///< 使用デバイス。モデルロード前に設定されている必要あり。
    CubismDeviceInfo_D3D11* _deviceInfo; ///< デバイスに紐づいている情報
    ID3D11DeviceContext* _context;  ///< 使用描画コンテキスト

    ID3D11Buffer*** _vertexBuffers;         ///< 描画バッファ カラー無し、位置+UV
    ID3D11Buffer*** _indexBuffers;          ///< インデックスのバッファ
    ID3D11Buffer*** _constantBuffers;       ///< 定数のバッファ
    ID3D11Buffer* _offscreenVertexBuffer;       ///< オフスクリーン用描画バッファ カラー無し、位置+UV
    ID3D11Buffer* _offscreenConstantBuffer;     ///< コピーシェーダー用定数のバッファ
    ID3D11Buffer* _copyVertexBuffer;       ///< コピーシェーダー用描画バッファ カラー無し、位置+UV
    ID3D11Buffer* _copyIndexBuffer;        ///< コピーシェーダー用インデックスのバッファ
    ID3D11Buffer* _copyConstantBuffer;     ///< コピーシェーダー用定数のバッファ
    csmUint32 _drawableNum;           ///< _vertexBuffers, _indexBuffersの確保数

    csmInt32 _commandBufferNum;
    csmInt32 _commandBufferCurrent;

    csmVector<csmInt32> _sortedObjectsIndexList;           ///< 描画オブジェクトのインデックスを描画順に並べたリスト
    csmVector<DrawableObjectType> _sortedObjectsTypeList;  ///< 描画オブジェクトの種別を描画順に並べたリスト

    csmMap<csmInt32, ID3D11ShaderResourceView*> _textures;              ///< モデルが参照するテクスチャとレンダラでバインドしているテクスチャとのマップ

    csmVector<CubismRenderTarget_D3D11> _modelRenderTargets; ///< 描画先のフレームバッファ
    csmVector<csmVector<CubismRenderTarget_D3D11> > _drawableMasks;  ///< Drawable用のマスク描画用のフレームバッファ
    csmVector<csmVector<CubismRenderTarget_D3D11> > _offscreenMasks; ///< offscreen用のマスク描画用のフレームバッファ

    CubismClippingManager_D3D11* _drawableClippingManager;            ///< drawable用のクリッピングマスク管理オブジェクト
    CubismClippingManager_D3D11* _offscreenClippingManager;           ///< offscreen用のクリッピングマスク管理オブジェクト
    CubismClippingContext_D3D11* _clippingContextBufferForMask;       ///< マスクテクスチャに描画するためのクリッピングコンテキスト
    CubismClippingContext_D3D11* _clippingContextBufferForDrawable;   ///< drawable用の画面上描画するためのクリッピングコンテキスト
    CubismClippingContext_D3D11* _clippingContextBufferForOffscreen;  ///< offscreen用の画面上描画するためのクリッピングコンテキスト

    csmVector<CubismOffscreenRenderTarget_D3D11> _offscreenList; ///< モデルのオフスクリーン
    CubismOffscreenRenderTarget_D3D11* _currentOffscreen;        ///< 現在のオフスクリーンのフレームバッファ
};

}}}}
//------------ LIVE2D NAMESPACE ------------
