/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include <MetalKit/MetalKit.h>
#include "../CubismRenderer.hpp"
#include "../CubismClippingManager.hpp"
#include "CubismFramework.hpp"
#include "CubismRenderTarget_Metal.hpp"
#include "CubismDeviceInfo_Metal.hpp"
#include "CubismCommandBuffer_Metal.hpp"
#include "Type/csmVector.hpp"
#include "Type/csmRectF.hpp"
#include "Type/csmMap.hpp"
#include "Math/CubismVector2.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

//  前方宣言
class CubismRenderer_Metal;
class CubismClippingContext_Metal;

/**
 * @brief  クリッピングマスクの処理を実行するクラス
 *
 */
class CubismClippingManager_Metal : public CubismClippingManager<CubismClippingContext_Metal, CubismRenderTarget_Metal>
{
public:

    /**
     * @brief   クリッピングコンテキストを作成する。モデル描画時に実行する。
     *
     * @param[in]   model        ->         モデルのインスタンス
     * @param[in]   renderer     ->         レンダラのインスタンス
     * @param[in]   lastColorBuffer    ->   フレームバッファ
     * @param[in]   lastViewport ->         ビューポート
     * @param[in]   drawableObjectType ->  描画オブジェクトのタイプ
     */
    void SetupClippingContext(CubismModel& model, CubismRenderer_Metal* renderer, CubismRenderTarget_Metal* lastColorBuffer, csmRectF lastViewport, CubismRenderer::DrawableObjectType drawableObjectType);
};

/**
 * @brief   クリッピングマスクのコンテキスト
 */
class CubismClippingContext_Metal : public CubismClippingContext
{
    friend class CubismClippingManager_Metal;
    friend class CubismShader_Metal;
    friend class CubismRenderer_Metal;

public:
    /**
     * @brief   引数付きコンストラクタ
     * @param[in]   manager                                          ->  マスクを管理しているマネージャのインスタンス
     * @param[in]   model                                              ->  モデルのインスタンス
     * @param[in]   clippingDrawableIndices      ->  クリップしているDrawableのインデックスリスト
     * @param[in]   clipCount                                     ->  クリップしているDrawableの個数
     */
    CubismClippingContext_Metal(CubismClippingManager<CubismClippingContext_Metal, CubismRenderTarget_Metal>* manager, CubismModel& model, const csmInt32* clippingDrawableIndices, csmInt32 clipCount);

    /**
     * @brief   デストラクタ
     */
    virtual ~CubismClippingContext_Metal();

    /**
     * @brief   このマスクを管理するマネージャのインスタンスを取得する。
     *
     * @return  クリッピングマネージャのインスタンス
     */
    CubismClippingManager<CubismClippingContext_Metal, CubismRenderTarget_Metal>* GetClippingManager();

private:
    csmVector<CubismCommandBuffer_Metal::DrawCommandBuffer*>* _clippingCommandBufferList;
    CubismClippingManager<CubismClippingContext_Metal, CubismRenderTarget_Metal>* _owner;        ///< このマスクを管理しているマネージャのインスタンス
};

/**
 * @brief   Cubismモデルを描画する直前のMetalのステートを保持・復帰させるクラス
 *
 */
class CubismRendererProfile_Metal
{
    friend class CubismRenderer_Metal;

private:
    /**
     * @brief   privateなコンストラクタ
     */
    CubismRendererProfile_Metal() {};

    /**
     * @brief   privateなデストラクタ
     */
    virtual ~CubismRendererProfile_Metal() {};

    csmBool _lastScissorTest;             ///< モデル描画直前のGL_VERTEX_ATTRIB_ARRAY_ENABLEDパラメータ
    csmBool _lastBlend;                   ///< モデル描画直前のGL_SCISSOR_TESTパラメータ
    csmBool _lastStencilTest;             ///< モデル描画直前のGL_STENCIL_TESTパラメータ
    csmBool _lastDepthTest;               ///< モデル描画直前のGL_DEPTH_TESTパラメータ
    CubismRenderTarget_Metal* _lastColorBuffer;                         ///< モデル描画直前のフレームバッファ
    id <MTLTexture> _lastDepthBuffer;
    id <MTLTexture> _lastStencilBuffer;
    csmRectF _lastViewport;                 ///< モデル描画直前のビューポート
};

/**
 * @brief   Metal用の描画命令を実装したクラス
 *
 */
class CubismRenderer_Metal : public CubismRenderer
{
    friend class CubismRenderer;
    friend class CubismClippingManager_Metal;
    friend class CubismShader_Metal;

public:
    /**
     * @brief  レンダラを作成するための各種設定
     * 　　　　　モデルロード前に毎回設定されている必要あり
     *
     * @param[in]   device                      -> 使用デバイス
     * @param[in]   maskBufferCount  -> 1パーツに付き作成するバッファ数
     */
    static void SetConstantSettings(id<MTLDevice> device, csmUint32 maskBufferCount = 0);

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
     * @param[in]   commandBuffer                 -> コマンドバッファ
     * @param[in]   renderPassDescriptor -> ディスクリプタ
     */
    void StartFrame(id<MTLCommandBuffer> commandBuffer, MTLRenderPassDescriptor* renderPassDescriptor);

    /**
     * @brief    レンダラの初期化処理を実行する
     *           引数に渡したモデルからレンダラの初期化処理に必要な情報を取り出すことができる
     *
     * @param[in]  model -> モデルのインスタンス
     */
    void Initialize(Framework::CubismModel* model) override;

    void Initialize(Framework::CubismModel* model, csmInt32 maskBufferCount) override;

    /**
     * @brief オフスクリーンの親を探して設定する
     *
     * @param model -> モデルのインスタンス
     * @param offscreenCount -> オフスクリーンの数
     */
    void SetupParentOffscreens(const CubismModel* model, csmInt32 offscreenCount);

    /**
     * @brief   テクスチャのバインド処理
     *           CubismRendererにテクスチャを設定し、CubismRenderer中でその画像を参照するためのIndex値を戻り値とする
     *
     * @param[in]   modelTextureIndex  ->  セットするモデルテクスチャの番号
     * @param[in]   texture     ->  バックエンドテクスチャ
     *
     */
    void BindTexture(csmUint32 modelTextureIndex, id <MTLTexture> texture);

    /**
     * @brief   バインドされたテクスチャのリストを取得する
     *
     * @return  テクスチャのアドレスのリスト
     */
    const csmMap< csmInt32, id <MTLTexture> >& GetBindedTextures() const;

    /**
     * @brief   指定したIDにバインドされたテクスチャを取得する
     *
     * @return  テクスチャ
     */
    id <MTLTexture> GetBindedTextureId(csmInt32 textureId);

    /**
     * @brief  クリッピングマスクバッファのサイズを設定する
     *         マスク用のRenderTargetを破棄・再作成するため処理コストは高い。
     *
     * @param[in]  width -> クリッピングマスクバッファの幅
     * @param[in]  height -> クリッピングマスクバッファの幅
     *
     */
    void SetDrawbleClippingMaskBufferSize(csmFloat32 width, csmFloat32 height);

    /**
     * @brief  オフスクリーン用クリッピングマスクバッファのサイズを設定する<br>
     *         マスク用のRenderTargetを破棄・再作成するため処理コストは高い。
     *
     * @param[in]  width -> クリッピングマスクバッファの幅
     * @param[in]  height -> クリッピングマスクバッファの幅
     *
     */
    void SetOffscreenClippingMaskBufferSize(csmFloat32 width, csmFloat32 height);

    /**
     * @brief  描画オブジェクトのレンダーテクスチャの枚数を取得する。
     *
     * @return  描画オブジェクトのレンダーテクスチャの枚数
     *
     */
    csmInt32 GetDrawableRenderTextureCount() const;

    /**
     * @brief  オフスクリーンレンダーテクスチャの枚数を取得する。
     *
     * @return  オフスクリーンのレンダーテクスチャの枚数
     *
     */
    csmInt32 GetOffscreenRenderTextureCount() const;

    /**
     * @brief  描画オブジェクトのクリッピングマスクバッファのサイズを取得する
     *
     * @return 描画オブジェクトのクリッピングマスクバッファのサイズ
     *
     */
    CubismVector2 GetDrawableClippingMaskBufferSize() const;

    /**
     * @brief  オフスクリーンのクリッピングマスクバッファのサイズを取得する
     *
     * @return オフスクリーンのクリッピングマスクバッファのサイズ
     *
     */
    CubismVector2 GetOffscreenClippingMaskBufferSize() const;

    /**
     * @brief  描画オブジェクトのクリッピングマスクのバッファを取得する
     *
     * @param[in]   index -> 取得するバッファをインデックス
     * @return 描画オブジェクトのクリッピングマスクのバッファへのポインタ
     */
    CubismRenderTarget_Metal* GetDrawableMaskBuffer(csmInt32 index);

    /**
     * @brief  オフスクリーンのクリッピングマスクのバッファを取得する
     *
     * @param[in]   index -> 取得するバッファをインデックス
     * @return オフスクリーンのクリッピングマスクのバッファへのポインタ
     */
    CubismRenderTarget_Metal* GetOffscreenMaskBuffer(csmInt32 index);

    /**
     * @brief 現在のオフスクリーンのフレームバッファを取得する
     *
     * @return 現在のオフスクリーンのフレームバッファを返す
     */
    CubismOffscreenRenderTarget_Metal* GetCurrentOffscreen() const;

    /**
     * @brief  モデル全体を描画する先のフレームバッファを取得する
     *
     * @param[in]   index -> 取得するフレームバッファのインデックス
     * @return モデル全体を描画する先のフレームバッファへの参照
     */
    CubismRenderTarget_Metal* GetModelRenderTarget(csmInt32 index);

    /**
     * @brief  任意のバッファをコピーする
     *
     * @param srcBuffer コピー元のバッファ
     *
     * @return コピー後のバッファへのポインタ
     */
    CubismRenderTarget_Metal* CopyRenderTarget(CubismRenderTarget_Metal& srcBuffer);

    CubismCommandBuffer_Metal* GetCommandBuffer()
    {
        return &_commandBuffer;
    }

protected:
    /**
     * @brief   コンストラクタ
     *
     * @param[in]   width  ->  オフスクリーン幅
     * @param[in]   height ->  オフスクリーン高さ
     */
    CubismRenderer_Metal(csmUint32 width, csmUint32 height);

    /**
     * @brief   デストラクタ
     */
    virtual ~CubismRenderer_Metal();

    /**
     * @brief   モデルを描画する実際の処理
     *
     */
    void DoDrawModel() override;

    /**
     * @brief   描画オブジェクト（アートメッシュ、オフスクリーン）を描画するループ処理
     *
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
     * @brief   親オフスクリーンへオフスクリーンの描画結果の伝搬を試みる。
     *
     * @param[in] objectIndex 処理をするオブジェクトのインデックス
     * @param[in] objectType 処理をするオブジェクトのタイプ
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
    void DrawOffscreen(CubismOffscreenRenderTarget_Metal* currentOffscreen);

    /**
     * @brief    描画オブジェクト（アートメッシュ）を描画する。
     *           ポリゴンメッシュとテクスチャ番号をセットで渡す。
     *
     * @param[in]   drawCommandBuffer         ->  描画するテクスチャ番号
     * @param[in]   renderEncoder     ->  MTLRenderCommandEncoder
     * @param[in]   model             ->  描画対象モデル
     * @param[in]   index             ->  描画オブジェクトのインデックス
     * @param[in]   blendTexture      ->  ブレンド対象のテクスチャ
     */
    void DrawMeshMetal(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer
                    , id <MTLRenderCommandEncoder> renderEncoder
                    , const CubismModel& model, const csmInt32 index, id<MTLTexture> blendTexture);

    /**
     * @brief    オフスクリーンを描画する。<br>
     *           ポリゴンメッシュとテクスチャ番号をセットで渡す。
     *
     * @param[in]   drawCommandBuffer         ->  描画するテクスチャ番号
     * @param[in]   renderEncoder     ->  MTLRenderCommandEncoder
     * @param[in]   model             ->  描画対象モデル
     * @param[in]   offscreeen        ->  対象のオフスクリーン
     * @param[in]   blendTexture      ->  ブレンド対象のテクスチャ
     */
    void DrawOffscreenMetal(id <MTLRenderCommandEncoder> renderEncoder, const CubismModel& model, CubismOffscreenRenderTarget_Metal* offscreen, id<MTLTexture> blendTexture);

    CubismCommandBuffer_Metal::DrawCommandBuffer* GetDrawCommandBufferData(csmInt32 drawableIndex);

    /**
     * @brief デバイスを取得する。
     *
     * @return  デバイスを返す
     */
    id<MTLDevice> GetDevice();

private:

    // Prevention of copy Constructor
    CubismRenderer_Metal(const CubismRenderer_Metal&);
    CubismRenderer_Metal& operator=(const CubismRenderer_Metal&);

    /**
     * @brief   描画開始時の追加処理。
     */
    void PreDraw();

    /**
     * @brief   描画完了後の追加処理。
     *
     */
    void PostDraw();

    /**
     * @brief   デフォルト、モデル、オフスクリーンの中から現在使用するレンダーターゲットを開始する。
     *
     * @param[in]   isClearEnabled     ->  レンダーターゲットをクリアするか
     */
    void BeginRenderTarget(csmBool isClearEnabled = false);

    /**
     * @brief   BeginDrawで開始したレンダーターゲットを修了する。
     *
     */
    void EndRenderTarget();

    /**
     * @brief  インデックスがブレンドモードか判定する。
     *
     * @param[in]   index     ->  描画オブジェクトのインデックス
     * @param[in]   drawableObjectType    ->  描画オブジェクトの種別
     *
     * @return  ブレンドモードならtrueを返す
     */
    csmBool IsBlendModeIndex(csmInt32 index, DrawableObjectType drawableObjectType);

    /**
     * @brief   SuperClass対応
     */
    void SaveProfile() override;

    /**
     * @brief   SuperClass対応
     */
    void RestoreProfile() override;

    /**
     * @brief   モデル描画直前のオフスクリーン設定
     */
    void BeforeDrawModelRenderTarget() override;

    /**
     * @brief   モデル描画後のオフスクリーン設定
     */
    void AfterDrawModelRenderTarget() override;

    /**
     * @brief   ブレンドモードを使用するかどうか
     *
     * @param   対象のモデル
     *
     * @retval  true   使用する
     * @retval  false  使用しない
     */
    csmBool IsBlendMode(CubismModel* model = nullptr);

    /**
     * @brief   マスクテクスチャに描画するクリッピングコンテキストをセットする。
     */
    void SetClippingContextBufferForMask(CubismClippingContext_Metal* clip);

    /**
     * @brief   マスクテクスチャに描画するクリッピングコンテキストを取得する。
     *
     * @return  マスクテクスチャに描画するクリッピングコンテキスト
     */
    CubismClippingContext_Metal* GetClippingContextBufferForMask() const;

    /**
     * @brief   画面上に描画するクリッピングコンテキストをセットする。
     *
     * @param[in] clip  ->  画面上に描画するクリッピングコンテキスト
     */
    void SetClippingContextBufferForDrawable(CubismClippingContext_Metal* clip);

    /**
     * @brief   画面上に描画するクリッピングコンテキストを取得する。
     *
     * @return  画面上に描画するクリッピングコンテキスト
     */
    CubismClippingContext_Metal* GetClippingContextBufferForDrawable() const;

    /**
     * @brief   offscreenで画面上に描画するクリッピングコンテキストをセットする。
     *
     * @param[in]   clip     ->  offscreenで画面上に描画するクリッピングコンテキスト
     */
    void SetClippingContextBufferForOffscreen(CubismClippingContext_Metal* clip);

    /**
     * @brief   offscreenで画面上に描画するクリッピングコンテキストを取得する。
     *
     * @return  offscreenで画面上に描画するクリッピングコンテキスト
     */
    CubismClippingContext_Metal* GetClippingContextBufferForOffscreen() const;

    /**
     * @brief  マスク生成時かを判定する
     *
     * @return  判定値
     */
    const inline csmBool IsGeneratingMask() const;

    id<MTLDevice> _device; ///< デバイス
    id<MTLCommandBuffer> _mtlCommandBuffer; ///< コマンドバッファ
    id <MTLRenderCommandEncoder> _mtlCommandEncoder; ///< コマンドエンコーダー
    MTLRenderPassDescriptor* _renderPassDescriptor; ///< ディスクリプタ
    CubismDeviceInfo_Metal* _deviceInfo; ///< デバイスに紐づいている情報

    csmMap< csmInt32, id <MTLTexture> > _textures;                      ///< モデルが参照するテクスチャとレンダラでバインドしているテクスチャとのマップ
    csmVector<csmInt32> _sortedObjectsIndexList;       ///< 描画オブジェクトのインデックスを描画順に並べたリスト
    csmVector<DrawableObjectType> _sortedObjectsTypeList;       ///< 描画オブジェクトの種別を描画順に並べたリスト
    CubismRendererProfile_Metal _rendererProfile;               ///< Metalのステートを保持するオブジェクト
    CubismClippingManager_Metal* _drawableClippingManager;               ///< クリッピングマスク管理オブジェクト
    CubismClippingManager_Metal* _offscreenClippingManager;              ///< クリッピングマスク管理オブジェクト
    CubismClippingContext_Metal* _clippingContextBufferForMask;  ///< マスクテクスチャに描画するためのクリッピングコンテキスト
    CubismClippingContext_Metal* _clippingContextBufferForDrawable;  ///< 画面上描画するためのクリッピングコンテキスト
    CubismClippingContext_Metal* _clippingContextBufferForOffscreen;  ///< 画面上描画するためのクリッピングコンテキスト

    csmVector<CubismRenderTarget_Metal> _modelRenderTargets;  ///< モデル全体を描画する先のフレームバッファ

    csmVector<CubismRenderTarget_Metal> _drawableMasks;         ///< Drawableのマスク描画用のフレームバッファ
    csmVector<CubismRenderTarget_Metal> _offscreenMasks;        ///< オフスクリーン機能マスク描画用のフレームバッファ
    csmVector<CubismOffscreenRenderTarget_Metal> _offscreenList; ///< モデルのオフスクリーン

    CubismOffscreenRenderTarget_Metal* _currentOffscreen; ///< 現在のオフスクリーンのフレームバッファ

    CubismCommandBuffer_Metal _commandBuffer;
    csmVector<CubismCommandBuffer_Metal::DrawCommandBuffer*> _drawableDrawCommandBuffer;
    CubismCommandBuffer_Metal::DrawCommandBuffer* _offscreenDrawCommandBuffer;

    CubismCommandBuffer_Metal::DrawCommandBuffer* _copyCommandBuffer;

};

}}}}
//------------ LIVE2D NAMESPACE ------------
