/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once
#include <string>
#include "../CubismRenderer.hpp"
#include "../CubismClippingManager.hpp"
#include <vulkan/vulkan.h>
#include "CubismFramework.hpp"
#include "CubismRenderTarget_Vulkan.hpp"
#include "CubismOffscreenRenderTarget_Vulkan.hpp"
#include "CubismClass_Vulkan.hpp"
#include "Type/csmVector.hpp"
#include "Type/csmMap.hpp"
#include "Math/CubismVector2.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @brief   viewportのサイズを設定する
 *
 * @param[in]  width    -> 横幅
 * @param[in]  height   -> 縦幅
 * @param[in]  minDepth -> 最小深度
 * @param[in]  maxDepth -> 最大深度
 */
VkViewport GetViewport(csmFloat32 width, csmFloat32 height, csmFloat32 minDepth, csmFloat32 maxDepth);

/**
 * @brief   scissorのサイズを設定する
 *
 * @param[in]  offsetX  -> x方向のオフセット
 * @param[in]  offsetY  -> y方向のオフセット
 * @param[in]  width    -> 横幅
 * @param[in]  height   -> 縦幅
 */
VkRect2D GetScissor(csmFloat32 offsetX, csmFloat32 offsetY, csmFloat32 width, csmFloat32 height);

/*********************************************************************************************************************
*                                      CubismRenderer_Vulkan
********************************************************************************************************************/
//  前方宣言
class CubismRenderer_Vulkan;
class CubismClippingContext_Vulkan;

/**
 * @brief  クリッピングマスクの処理を実行するクラス
 */
class CubismClippingManager_Vulkan : public CubismClippingManager<
            CubismClippingContext_Vulkan, CubismRenderTarget_Vulkan>
{
public:
    /**
     * @brief   クリッピングコンテキストを作成する。モデル描画時に実行する。
     *
     * @param[in]   model          ->  モデルのインスタンス
     * @param[in]   commandBuffer  ->  コマンドバッファ
     * @param[in]   updateCommandBuffer  ->  更新用コマンドバッファ
     * @param[in]   renderer       ->  レンダラのインスタンス
     * @param[in]   commandBufferCurrent       -> スワップチェインに使用中のバッファインデックス
     * @param[in]   drawableObjectType  ->  描画オブジェクトのタイプ
     */
    void SetupClippingContext(CubismModel& model, VkCommandBuffer commandBuffer, VkCommandBuffer updateCommandBuffer,
                              CubismRenderer_Vulkan* renderer, csmInt32 commandBufferCurrent,
                              CubismRenderer::DrawableObjectType drawableObjectType);
};

/**
 * @brief   クリッピングマスクのコンテキスト
 */
class CubismClippingContext_Vulkan : public CubismClippingContext
{
    friend class CubismClippingManager_Vulkan;
    friend class CubismRenderer_Vulkan;

public:
    /**
     * @brief   引数付きコンストラクタ
     *
     */
    CubismClippingContext_Vulkan(
        CubismClippingManager<CubismClippingContext_Vulkan, CubismRenderTarget_Vulkan>* manager, CubismModel& model,
        const csmInt32* clippingDrawableIndices, csmInt32 clipCount);

    /**
     * @brief   デストラクタ
     */
    virtual ~CubismClippingContext_Vulkan();

    /**
     * @brief   このマスクを管理するマネージャのインスタンスを取得する。
     *
     * @return  クリッピングマネージャのインスタンス
     */
    CubismClippingManager<CubismClippingContext_Vulkan, CubismRenderTarget_Vulkan>* GetClippingManager();

    CubismClippingManager<CubismClippingContext_Vulkan, CubismRenderTarget_Vulkan>* _owner;
    ///< このマスクを管理しているマネージャのインスタンス
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

enum CompatibleBlend
{
    Blend_Normal,
    Blend_Add,
    Blend_Mult,
    Blend_Mask
};

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
    ColorBlendMode_Normal,
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
 * @brief   どのシェーダーを利用するかを取得する
 */
csmInt32 GetShaderNamesBegin(const csmBlendMode blendMode);

/**
 * @brief   頂点情報を保持する構造体
 *
 */
struct ModelVertex
{
    CubismVector2 pos; // Position
    CubismVector2 texCoord; // UVs

    static VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(ModelVertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static void GetAttributeDescriptions(VkVertexInputAttributeDescription attributeDescriptions[2])
    {
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(ModelVertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(ModelVertex, texCoord);
    }
};

/**
 * @brief  シェーダー関連のプログラムを生成・破棄するクラス<br>
 *         シングルトンなクラスであり、CubismShader_Vulkan::GetInstance()からアクセスする。
 */
class CubismPipeline_Vulkan
{
public:
    /**
     * @brief   コンストラクタ
     */
    CubismPipeline_Vulkan();

    /**
     * @brief   デストラクタ
     */
    ~CubismPipeline_Vulkan();

    struct PipelineResource
    {
        /**
         * @brief   シェーダーモジュールを作成する
         *
         * @param[in]   device        ->  論理デバイス
         * @param[in]   filename      ->  ファイル名
         */
        VkShaderModule CreateShaderModule(VkDevice device, csmString filename);

        /**
         * @brief   パイプラインを作成する
         *
         * @param[in]   vertFileName        ->  Vertexシェーダーのファイル
         * @param[in]   fragFileName        ->  Fragmentシェーダーのファイル
         * @param[in]   descriptorSetLayout ->  ディスクリプタセットレイアウト
         * @param[in]   colorBlendMode      ->  ブレンドカラーモード
         * @param[in]   alphaBlendMode      ->  オーバーラップカラーモード
         *
         * @return     シェーダーの作成に成功: true 失敗: false
         */
        csmBool CreateGraphicsPipeline(csmString vertFileName, csmString fragFileName,
                                    VkDescriptorSetLayout descriptorSetLayout,
                                    csmUint32 shaderName,
                                    csmInt32 colorBlendMode = ColorBlendMode_None, csmInt32 alphaBlendMode = AlphaBlendMode_None);
        void Release();
        VkPipeline GetPipeline(csmInt32 index)
        {
             return _pipeline[index];
        }
        VkPipelineLayout GetPipelineLayout(csmInt32 index) { return _pipelineLayout[index]; }

    private:
        csmVector<VkPipeline> _pipeline; ///< normal, add, multi, maskそれぞれのパイプライン
        csmVector<VkPipelineLayout> _pipelineLayout; ///< normal, add, multi, maskそれぞれのパイプラインレイアウト
    };

    /**
     * @brief   シェーダーごとにPipelineResourceのインスタンスを作成する
     *
     * @param[in]   descriptorSetLayout ->  ディスクリプタセットレイアウト(DrawableとOffscreen用)
     * @param[in]   copyDescriptorSetLayout ->  ディスクリプタセットレイアウト(Copy用)
     */
    void CreatePipelines(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSetLayout copyDescriptorSetLayout);

    /**
     * @brief   PipelineResourceのインスタンスを作成する
     *
     * @param[in]   shaderName ->  シェーダー名
     * @param[in]   vertShaderFileName ->  Vertexシェーダーファイル名
     * @param[in]   fragShaderFileName ->  Fragmentシェーダーファイル名
     * @param[in]   descriptorSetLayout ->  ディスクリプタセットレイアウト
     * @param[in]   colorBlendMode      ->  ブレンドカラーモード
     * @param[in]   alphaBlendMode      ->  オーバーラップカラーモード
     */
    void CreatePipelineResource(csmUint32 const& shaderName, csmString const& vertShaderFileName, csmString const& fragShaderFileName, VkDescriptorSetLayout const& descriptorSetLayout, csmInt32 const& colorBlendMode=ColorBlendMode_None, csmInt32 const& alphaBlendMode=AlphaBlendMode_None);

    /**
     * @brief   指定したシェーダーのグラフィックスパイプラインを取得する
     *
     * @param[in]   shaderIndex         ->  シェーダインデックス
     * @param[in]   blendIndex          ->  ブレンドモードのインデックス
     *
     * @return  指定したシェーダーのグラフィックスパイプライン
     */
    VkPipeline GetPipeline(csmInt32 shaderIndex, csmInt32 blendIndex)
    {
        if(_pipelineResource[shaderIndex] == NULL)
        {
            return NULL;
        }
        return _pipelineResource[shaderIndex]->GetPipeline(blendIndex);
    }

    /**
     * @brief   指定したブレンド方法のパイプラインレイアウトを取得する
     *
     * @param[in]   shaderIndex         ->  シェーダインデックス
     * @param[in]   blendIndex          ->  ブレンドモードのインデックス
     *
     * @return  指定したブレンド方法のパイプラインレイアウト
     */
    VkPipelineLayout GetPipelineLayout(csmInt32 shaderIndex, csmInt32 blendIndex)
    {
        if(_pipelineResource[shaderIndex] == NULL)
        {
            return NULL;
        }
        return _pipelineResource[shaderIndex]->GetPipelineLayout(blendIndex);
    }

    /**
     * @brief   インスタンスを取得する（シングルトン）
     *
     * @return  インスタンスのポインタ
     */
    static CubismPipeline_Vulkan* GetInstance();

    /**
     * @brief   リソースを開放する
     */
    void ReleaseShaderProgram();

private:
    csmVector<PipelineResource*> _pipelineResource;
};

/**
 * @brief   Vulkan用の描画命令を実装したクラス
 *
 */
class CubismRenderer_Vulkan : public CubismRenderer
{
    friend class CubismClippingManager_Vulkan;
    friend class CubismRenderer;

    /**
     * @brief   モデル用ユニフォームバッファオブジェクトの中身を保持する構造体
     */
    struct ModelUBO
    {
        csmFloat32 projectionMatrix[16]; ///< シェーダープログラムに渡すデータ(ProjectionMatrix)
        csmFloat32 clipMatrix[16]; ///< シェーダープログラムに渡すデータ(ClipMatrix)
        csmFloat32 baseColor[4]; ///< シェーダープログラムに渡すデータ(BaseColor)
        csmFloat32 multiplyColor[4]; ///< シェーダープログラムに渡すデータ(MultiplyColor)
        csmFloat32 screenColor[4]; ///< シェーダープログラムに渡すデータ(ScreenColor)
        csmFloat32 channelFlag[4]; ///< シェーダープログラムに渡すデータ(ChannelFlag)
    };

    /**
     * @brief   ディスクリプタセットとUBOを保持する構造体
     *          コマンドバッファでまとめて描画する場合、ディスクリプタセットをバインド後に
     *          更新してはならない。<br>
     *          マスクされる描画対象のみ、フレームバッファをディスクリプタセットに設定する必要があるが、
     *          その際バインド済みのディスクリプタセットを更新しないよう、別でマスクされる用のディスクリプタセットを用意する。
     */
    struct Descriptor
    {
        Descriptor()
            : descriptorSet(VK_NULL_HANDLE)
            , isDescriptorSetUpdated(false)
            , descriptorSetMasked(VK_NULL_HANDLE)
            , isDescriptorSetMaskedUpdated(false)
            , descriptorSetForMask(VK_NULL_HANDLE)
            , isDescriptorSetForMaskUpdated(false)
        {}

        CubismBufferVulkan uniformBuffer; ///< ユニフォームバッファ
        VkDescriptorSet descriptorSet; ///< ディスクリプタセット
        csmBool isDescriptorSetUpdated; ///< ディスクリプタセットが更新されたか
        VkDescriptorSet descriptorSetMasked; ///< マスクされる描画対象用のディスクリプタセット
        csmBool isDescriptorSetMaskedUpdated; ///< マスクされる描画対象用のディスクリプタセットが更新されたか
        CubismBufferVulkan uniformBufferMask; ///< マスク生成時用のユニフォームバッファ
        VkDescriptorSet descriptorSetForMask; ///< マスク生成時用のディスクリプタセット
        csmBool isDescriptorSetForMaskUpdated; ///< マスク生成時用のディスクリプタセットが更新されたか
    };

protected:
    /**
     * @brief   コンストラクタ
     */
    CubismRenderer_Vulkan(csmUint32 width, csmUint32 height);

    /**
     * @brief   デストラクタ
     */
    ~CubismRenderer_Vulkan() override;

    /**
     * @brief   レンダラが保持する静的なリソースを解放する。
     */
    static void DoStaticRelease();

public:

    /**
     * @brief   コマンドの記録を開始する。
     *
     * @return  記録を開始したコマンドバッファ
     */
    VkCommandBuffer BeginSingleTimeCommands();

    /**
     * @brief   コマンドを実行する。
     *
     * @param[in]   commandBuffer                   -> コマンドバッファ
     * @param[in]   signalUpdateFinishedSemaphore   -> コマンド終了時にシグナルを出すセマフォ
     * @param[in]   waitUpdateFinishedSemaphore     -> コマンド実行前に待つセマフォ
     */
    void SubmitCommand(VkCommandBuffer commandBuffer, VkSemaphore signalUpdateFinishedSemaphore = VK_NULL_HANDLE, VkSemaphore waitUpdateFinishedSemaphore = VK_NULL_HANDLE);

    /**
     * @brief    レンダラを作成するための各種設定
     *
     *           モデルを読み込む前に一度だけ呼び出す
     *
     * @param[in]   device              -> 論理デバイス
     * @param[in]   physicalDevice      -> 物理デバイス
     * @param[in]   commandPool         -> コマンドプール
     * @param[in]   queue               -> キュー
     * @param[in]   swapchainImageCount -> スワップチェーンのイメージ数
     * @param[in]   extent              -> 描画解像度
     * @param[in]   imageView           -> イメージビュー
     * @param[in]   imageFormat         -> 描画対象のフォーマット
     * @param[in]   depthFormat         -> 深度フォーマット
     */
    static void InitializeConstantSettings(VkDevice device, VkPhysicalDevice physicalDevice,
                                           VkCommandPool commandPool, VkQueue queue,
                                           csmUint32 swapchainImageCount, VkExtent2D extent,
                                           VkImageView imageView, VkFormat imageFormat, VkFormat depthFormat);

    /**
     * @brief    レンダーターゲット変更を有効にする
     */
    static void EnableChangeRenderTarget();

    /**
     * @brief    レンダリング対象の指定
     *
     * @param[in]   image          -> イメージ
     * @param[in]   view           -> イメージビュー
     * @param[in]   format         -> フォーマット
     * @param[in]   extent         -> 描画解像度
     */
    static void SetRenderTarget(VkImage image, VkImageView view, VkFormat format, VkExtent2D extent);

    /**
     * @brief   描画対象の解像度を設定する
     *
     * @param[in]  extent 　　-> 描画解像度
     */
    static void SetImageExtent(VkExtent2D extent);

    /**
     * @brief   コマンドバッファを作成する。
     */
    void CreateCommandBuffer();

    /**
     * @brief   空の頂点バッファを作成する。
     */
    void CreateVertexBuffer();

    /**
     * @brief   インデックスバッファを作成する。
     */
    void CreateIndexBuffer();

    /**
     * @brief   ユニフォームバッファとディスクリプタセットを作成する。
     *          ディスクリプタセットレイアウトはユニフォームバッファ1つとモデル用テクスチャ1つとマスク用テクスチャ1つを指定する。
     */
    void CreateDescriptorSets();

    /**
     * @brief  深度バッファを作成する。
     */
    void CreateDepthBuffer();

    /**
     * @brief   レンダラーを初期化する。
     */
    void InitializeRenderer();

    /**
     * @brief    レンダラの初期化処理を実行する<br>
     *           引数に渡したモデルからレンダラの初期化処理に必要な情報を取り出すことができる
     *
     * @param[in]  model -> モデルのインスタンス
     */
    void Initialize(Framework::CubismModel* model) override;

    /**
     * @brief    レンダラの初期化処理を実行する<br>
     *           引数に渡したモデルからレンダラの初期化処理に必要な情報を取り出すことができる
     *
     * @param[in]  model -> モデルのインスタンス
     * @param[in]  maskBufferCount -> マスクバッファの数
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
     * @brief   頂点バッファを更新する。
     * @param[in]   drawAssign    -> 描画インデックス
     * @param[in]   vcount        -> 頂点数
     * @param[in]   varray        -> 頂点配列
     * @param[in]   uvarray       -> uv配列
     * @param[in]   commandBuffer -> コマンドバッファ
     */
    void CopyToBuffer(csmInt32 drawAssign, const csmInt32 vcount, const csmFloat32* varray, const csmFloat32* uvarray,
                      VkCommandBuffer commandBuffer);

    /**
     * @brief   行列を更新する
     *
     * @param[in]   vkMat4     -> 更新する行列
     * @param[in]   cubismMat  -> 新しい値
     */
    static void UpdateMatrix(csmFloat32 vkMat4[16], CubismMatrix44 cubismMat);

    /**
     * @brief   カラーベクトルを更新する
     *
     * @param[in]   vkVec4     -> 更新するカラーベクトル
     * @param[in]   r, g, b, a -> 新しい値
     */
    void UpdateColor(csmFloat32 vkVec4[4], csmFloat32 r, csmFloat32 g, csmFloat32 b, csmFloat32 a);

    /**
     * @brief  ディスクリプタセットを更新する
     * @param[in]   descriptor    -> 1つのシェーダーが使用するディスクリプタセットとUBO
     * @param[in]   textureIndex  -> テクスチャインデックス
     * @param[in]   isMasked      -> 描画対象がマスクされるか
     */
    void UpdateDescriptorSet(Descriptor& descriptor, csmUint32 textureIndex, csmBool isMasked);

    /**
     * @brief  マスク生成時用ディスクリプタセットを更新する
     * @param[in]   descriptor    -> 1つのシェーダーが使用するディスクリプタセットとUBO
     * @param[in]   textureIndex  -> テクスチャインデックス
     */
    void UpdateDescriptorSetForMask(Descriptor& descriptor, csmUint32 textureIndex);

    /**
     * @brief  オフスクリーン用ディスクリプタセットを更新する
     * @param[in]   descriptor    -> 1つのシェーダーが使用するディスクリプタセットとUBO
     * @param[in]   offscreen     -> 描画対象のオフスクリーン
     * @param[in]   isMasked      -> 描画対象がマスクされるか
     */
    void UpdateDescriptorSetForOffscreen(Descriptor& descriptor, CubismOffscreenRenderTarget_Vulkan* offscreen, csmBool isMasked);

    /**
     * @brief  コピー用ディスクリプタセットを更新する
     * @param[in]   descriptor      -> 1つのシェーダーが使用するディスクリプタセット
     * @param[in]   srcBuffer        -> コピー元
     */
    void UpdateDescriptorSetForCopy(Descriptor& descriptor, const CubismRenderTarget_Vulkan* srcBuffer);

    /**
     * @brief   メッシュ描画を実行する
     *
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index                 ->  描画オブジェクトのインデックス
     * @param[in]   cmdBuffer             ->  フレームバッファ関連のコマンドバッファ
     */
    void ExecuteDrawForDrawable(const CubismModel& model, const csmInt32 index, VkCommandBuffer& cmdBuffer);

    /**
     * @brief   マスク描画を実行する
     *
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index                 ->  描画オブジェクトのインデックス
     * @param[in]   cmdBuffer             ->  フレームバッファ関連のコマンドバッファ
     */
    void ExecuteDrawForMask(const CubismModel& model, const csmInt32 index, VkCommandBuffer& cmdBuffer);

    /**
     * @brief   オフスクリーン描画を実行する
     *
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   offscreen             ->  描画対象
     * @param[in]   cmdBuffer             ->  フレームバッファ関連のコマンドバッファ
     */
    void ExecuteDrawForOffscreen(const CubismModel& model, CubismOffscreenRenderTarget_Vulkan* offscreen, VkCommandBuffer& cmdBuffer);

    /**
     * @brief   オフスクリーンからのコピーを実行する
     *
     * @param[in]   srcBuffer             ->  コピー元
     * @param[in]   cmdBuffer             ->  フレームバッファ関連のコマンドバッファ
     */
    void ExecuteDrawForRenderTarget(const CubismRenderTarget_Vulkan* srcBuffer, VkCommandBuffer& cmdBuffer);

    /**
     * @brief   [オーバーライド]<br>
     *           描画オブジェクト（アートメッシュ）を描画する。<br>
     * @param[in]   model                 ->  描画対象のモデル
     * @param[in]   index           ->  描画メッシュのインデックス
     * @param[in]   commandBuffer    ->  コマンドバッファ
     * @param[in]   updateCommandBuffer ->  更新用コマンドバッファ
     */
    void DrawMeshVulkan(const CubismModel& model, const csmInt32 index,
                        VkCommandBuffer commandBuffer, VkCommandBuffer updateCommandBuffer);

    /**
     * @brief   レンダリング開始
     *
     * @param[in]   drawCommandBuffer       ->  コマンドバッファ
     * @param[in]   isResume                ->  レンダリング再開かのフラグ
     */
    void BeginRendering(VkCommandBuffer drawCommandBuffer, csmBool isResume);

    /**
     * @brief   レンダリング終了
     *
     * @param[in]   drawCommandBuffer       ->  コマンドバッファ
     */
    void EndRendering(VkCommandBuffer drawCommandBuffer);

    /**
     * @brief   MOCバージョンによって異なる対象へのレンダリング開始
     *
     * @param[in]   drawCommandBuffer       ->  コマンドバッファ
     * @param[in]   isResume                ->  レンダリング再開かのフラグ
     */
    void BeginRenderTarget(VkCommandBuffer drawCommandBuffer, csmBool isResume);

    /**
     * @brief   MOCバージョンによって異なる対象へのレンダリング終了
     *
     * @param[in]   drawCommandBuffer       ->  コマンドバッファ
     */
    void EndRenderTarget(VkCommandBuffer drawCommandBuffer);

    /**
     * @brief   モデルを描画する実際の処理
     *
     */
    void DoDrawModel() override;

    /**
     * @brief   描画完了後の追加処理。<br>
     *          マルチバッファリングに必要な処理を実装している。
     */
    void PostDraw();

    /**
     * @brief   モデル描画直前のステートを保持する
     */
    void SaveProfile() override {}

    /**
     * @brief   モデル描画直前のステートを保持する
     */
    void RestoreProfile() override {}

    /**
     * @brief   モデル描画直前のオフスクリーン設定
     */
    void BeforeDrawModelRenderTarget() override;

    /**
     * @brief   モデル描画後のオフスクリーン設定
     */
    void AfterDrawModelRenderTarget() override;

    /**
     * @brief   マスクテクスチャに描画するクリッピングコンテキストをセットする。
     *
     * @param[in]  clip ->  クリッピングコンテキスト
     */
    void SetClippingContextBufferForMask(CubismClippingContext_Vulkan* clip);

    /**
     * @brief   マスクテクスチャに描画するクリッピングコンテキストを取得する。
     *
     * @return  マスクテクスチャに描画するクリッピングコンテキスト
     */
    CubismClippingContext_Vulkan* GetClippingContextBufferForMask() const;

    /**
     * @brief   画面上に描画するクリッピングコンテキストをセットする。
     *
     * @param[in]  clip ->  クリッピングコンテキスト
     */
    void SetClippingContextBufferForDraw(CubismClippingContext_Vulkan* clip);

    /**
     * @brief   画面上に描画するクリッピングコンテキストを取得する。
     *
     * @return  画面上に描画するクリッピングコンテキスト
     */
    CubismClippingContext_Vulkan* GetClippingContextBufferForDrawable() const;

    /**
     * @brief   テクスチャマネージャーで作成したものをバインドする
     *
     * @param[in]   image         ->  テクスチャビューやサンプラーを保持しているインスタンス
     */
    void BindTexture(CubismImageVulkan& image);

    /**
     * @brief  クリッピングマスクバッファのサイズを設定する
     *
     * @param[in]  width  -> クリッピングマスクバッファの横幅
     * @param[in]  height -> クリッピングマスクバッファの立幅
     *
     */
    void SetClippingMaskBufferSize(csmFloat32 width, csmFloat32 height);

    /**
     * @brief  クリッピングマスクバッファのサイズを取得する
    *
     * @return クリッピングマスクバッファのサイズ
     *
     */
    CubismVector2 GetClippingMaskBufferSize() const;

    /**
     * @brief  モデル全体を描画する先のフレームバッファを取得する
     *
     * @return モデル全体を描画する先のフレームバッファへの参照
     *
     */
    CubismRenderTarget_Vulkan* GetModelRenderTarget();

    /**
     * @brief  クリッピングマスクのバッファを取得する
     *
     * @param[in] backbufferNum  -> バックバッファの番号
     * @param[in] offscreenIndex -> オフスクリーンのインデックス
     *
     * @return クリッピングマスクのバッファへのポインタ
     *
     */
    CubismRenderTarget_Vulkan* GetDrawableMaskBuffer(csmUint32 backbufferNum, csmInt32 offscreenIndex);

private:

    /**
     * @brief  色定数バッファを設定する
     *
     * @param[in]   ubo                   ->  ユニフォームバッファ
     * @param[in]   baseColor             ->  ベースカラー
     * @param[in]   multiplyColor         ->  乗算カラー
     * @param[in]   screenColor           ->  スクリーンカラー
     */
    void SetColorUniformBuffer(ModelUBO& ubo, const CubismTextureColor& baseColor,
                               const CubismTextureColor& multiplyColor, const CubismTextureColor& screenColor);

    /**
     * @brief  頂点バッファとインデックスバッファをバインドする
     *
     * @param[in]   index                   ->  描画メッシュのインデックス
     * @param[in]   cmdBuffer               ->  コマンドバッファ
     * @param[in]   drawableObjectType      ->  描画オブジェクトの種類
     */
    void BindVertexAndIndexBuffers(const csmInt32 index, VkCommandBuffer& cmdBuffer, DrawableObjectType drawableObjectType);

    /**
     * @brief  描画に使用するカラーチャンネルを設定
     *
     * @param[in]   ubo              ->  ユニフォームバッファ
     * @param[in]   contextBuffer    ->  描画コンテキスト
     */
    void SetColorChannel(ModelUBO& ubo, CubismClippingContext_Vulkan* contextBuffer);

    /**
     * @brief  コピー用のシェーダーでオフスクリーンのテクスチャを描画する
     *
     * @param[in]   src                -> コピー元
     * @param[in]   drawCommandBuffer  -> コマンドバッファ
     */
    void CopyRenderTarget(const CubismRenderTarget_Vulkan* src, VkCommandBuffer drawCommandBuffer);

    /**
     * @brief  描画オブジェクトのループ処理
     *
     * @param[in]   updateCommandBuffer   -> 更新用コマンドバッファ
     * @param[in]   drawCommandBuffer     -> 描画用コマンドバッファ
     * @param[in]   beginInfo             -> コマンド記録開始時の情報
     */
    void DrawObjectLoop(VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo);

    /**
     * @brief  描画オブジェクトの描画
     *
     * @param[in]   objectIndex           -> オブジェクトのインデックス
     * @param[in]   objectType            -> オブジェクトの種類
     * @param[in]   updateCommandBuffer   -> 更新用コマンドバッファ
     * @param[in]   drawCommandBuffer     -> 描画用コマンドバッファ
     * @param[in]   beginInfo             -> コマンド記録開始時の情報
     */
    void RenderObject(csmInt32 objectIndex, csmInt32 objectType, VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo);

    /**
     * @brief  Drawableの描画
     *
     * @param[in]   drawableIndex         -> Drawableのインデックス
     * @param[in]   updateCommandBuffer   -> 更新用コマンドバッファ
     * @param[in]   drawCommandBuffer     -> 描画用コマンドバッファ
     * @param[in]   beginInfo             -> コマンド記録開始時の情報
     */
    void DrawDrawable(csmInt32 drawableIndex, VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo);

    /**
     * @brief  親オフスクリーンへの描画伝搬
     *
     * @param[in]   objectIndex           -> オブジェクトのインデックス
     * @param[in]   objectType            -> オブジェクトの種類
     * @param[in]   updateCommandBuffer   -> 更新用コマンドバッファ
     * @param[in]   drawCommandBuffer     -> 描画用コマンドバッファ
     * @param[in]   beginInfo             -> コマンド記録開始時の情報
     */
    void SubmitDrawToParentOffscreen(csmInt32 objectIndex, DrawableObjectType objectType,
                                     VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo);

    /**
     * @brief  オフスクリーンの追加
     *
     * @param[in]   offscreenIndex        -> オフスクリーンのインデックス
     * @param[in]   updateCommandBuffer   -> 更新用コマンドバッファ
     * @param[in]   drawCommandBuffer     -> 描画用コマンドバッファ
     * @param[in]   beginInfo             -> コマンド記録開始時の情報
     */
    void AddOffscreen(csmInt32 offscreenIndex, VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo);

    /**
     * @brief  オフスクリーンの描画
     *
     * @param[in]   currentOffscreen      -> 現在のオフスクリーン
     * @param[in]   updateCommandBuffer   -> 更新用コマンドバッファ
     * @param[in]   drawCommandBuffer     -> 描画用コマンドバッファ
     * @param[in]   beginInfo             -> コマンド記録開始時の情報
     */
    void DrawOffscreen(CubismOffscreenRenderTarget_Vulkan* currentOffscreen,
                       VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo);

    /**
     * @brief  オフスクリーンの描画処理（Vulkan版）
     *
     * @param[in]   model                 -> モデル
     * @param[in]   offscreen             -> オフスクリーン
     * @param[in]   clipContext           -> クリッピングコンテキスト
     * @param[in]   updateCommandBuffer   -> 更新用コマンドバッファ
     * @param[in]   drawCommandBuffer     -> 描画用コマンドバッファ
     */
    void DrawOffscreenVulkan(const CubismModel& model, CubismOffscreenRenderTarget_Vulkan* offscreen,
                             CubismClippingContext_Vulkan* clipContext,
                             VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer);

    /**
     * @brief  オフスクリーンの描画用クリッピングコンテキストをセットする
     *
     * @param[in]   clip                  -> クリッピングコンテキスト
     */
    void SetClippingContextBufferForOffscreen(CubismClippingContext_Vulkan* clip);

    /**
     * @brief   オフスクリーン描画用クリッピングコンテキストを取得する。
     *
     * @return  オフスクリーン描画用クリッピングコンテキスト
     */
    CubismClippingContext_Vulkan* GetClippingContextBufferForOffscreen() const;

    /**
     * @brief  オフスクリーン用マスクバッファを取得する
     *
     * @param[in]   backBufferNum         -> バックバッファの番号
     * @param[in]   offscreenIndex        -> オフスクリーンのインデックス
     *
     * @return      オフスクリーン用マスクバッファ
     */
    CubismRenderTarget_Vulkan* GetOffscreenMaskBuffer(csmUint32 backBufferNum, csmInt32 offscreenIndex);

    /**
     * @brief  ブレンドモードで使用するテクスチャにメモリバリアを設定する
     *
     * @param[in]   drawCommandBuffer     -> コマンドバッファ
     */
    void SetBlendTextureBarrier(VkCommandBuffer drawCommandBuffer);

    /**
     * @brief  レンダーターゲット切り替え時に現在の描画を終了する
     *
     * @param[in]   commandBuffer      -> コマンドバッファ
     * @param[in]   nextRenderTarget   -> 切り替え先のレンダーターゲット
     */
    void EnsurePreviousRenderFinished(VkCommandBuffer commandBuffer, const CubismRenderTarget_Vulkan* nextRenderTarget);

    CubismClippingContext_Vulkan* _clippingContextBufferForOffscreen; ///< オフスクリーン用クリッピングコンテキスト
    CubismOffscreenRenderTarget_Vulkan* _currentOffscreen; ///< 現在のオフスクリーン
    CubismRenderTarget_Vulkan* _currentRenderTarget; ///< 現在のレンダーターゲット

    PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT;

    CubismRenderer_Vulkan(const CubismRenderer_Vulkan&);
    CubismRenderer_Vulkan& operator=(const CubismRenderer_Vulkan&);

    CubismClippingManager_Vulkan* _drawableClippingManager; ///< クリッピングマスク管理オブジェクト
    csmVector<csmInt32> _sortedDrawableIndexList; ///< 描画オブジェクトのインデックスを描画順に並べたリスト
    CubismClippingContext_Vulkan* _clippingContextBufferForMask; ///< マスクテクスチャに描画するためのクリッピングコンテキスト
    CubismClippingContext_Vulkan* _clippingContextBufferForDraw; ///< 画面上描画するためのクリッピングコンテキスト

    csmVector<csmInt32> _sortedObjectsIndexList;                ///< 描画オブジェクトのインデックスを描画順に並べたリスト
    csmVector<DrawableObjectType> _sortedObjectsTypeList;       ///< 描画オブジェクトの種別を描画順に並べたリスト

    CubismClippingManager_Vulkan* _offscreenClippingManager; ///< オフスクリーン用クリッピングマスク管理オブジェクト
    csmVector<CubismOffscreenRenderTarget_Vulkan> _offscreenList;     ///< モデルのオフスクリーン

    csmUint32 _commandBufferCurrent; ///< スワップチェーン用に使用中のバッファインデックス

    csmVector<csmVector<CubismRenderTarget_Vulkan>> _drawableMaskBuffers; ///< Drawableのマスク描画用のフレームバッファ
    csmVector<csmVector<CubismRenderTarget_Vulkan>> _offscreenMaskBuffers; ///< オフスクリーン機能マスク描画用のフレームバッファ
    csmVector<CubismRenderTarget_Vulkan> _modelRenderTargets; ///< モデル全体を描画する先のフレームバッファ
    csmVector<csmVector<CubismBufferVulkan>> _vertexBuffers; ///< 頂点バッファ
    csmVector<csmVector<CubismBufferVulkan>> _stagingBuffers; ///< 頂点バッファを更新する際に使うステージングバッファ
    csmVector<csmVector<CubismBufferVulkan>> _indexBuffers; ///< インデックスバッファ

    csmVector<csmVector<CubismBufferVulkan>> _offscreenVertexBuffers; ///< オフスクリーン用頂点バッファ
    csmVector<csmVector<CubismBufferVulkan>> _offscreenStagingBuffers; ///< オフスクリーン用頂点バッファを更新する際に使うステージングバッファ
    csmVector<csmVector<CubismBufferVulkan>> _offscreenIndexBuffers; ///< オフスクリーン用インデックスバッファ

    csmVector<CubismBufferVulkan> _copyVertexBuffer;
    csmVector<CubismBufferVulkan> _copyStagingBuffer;
    csmVector<CubismBufferVulkan> _copyIndexBuffer;

    VkDescriptorPool _descriptorPool; ///< ディスクリプタプール
    VkDescriptorSetLayout _descriptorSetLayout; ///< ディスクリプタセットのレイアウト
    csmVector<csmVector<Descriptor>> _descriptorSets; ///< ディスクリプタ管理オブジェクト

    VkDescriptorPool _offscreenDescriptorPool; ///< オフスクリーン用ディスクリプタプール
    csmVector<csmVector<Descriptor>> _offscreenDescriptorSets; ///< オフスクリーン用ディスクリプタ管理オブジェクト

    VkDescriptorPool _copyDescriptorPool; ///< コピー用ディスクリプタプール
    VkDescriptorSetLayout _copyDescriptorSetLayout; ///< コピー用ディスクリプタセットのレイアウト
    csmVector<Descriptor> _copyDescriptorSets; ///< コピー用ディスクリプタ管理オブジェクト

    csmVector<CubismImageVulkan> _textures; ///< モデルが使うテクスチャ
    CubismImageVulkan _depthImage; ///< オフスクリーンの色情報を保持する深度画像
    VkClearValue _clearColor; ///< クリアカラー

    csmVector<VkSemaphore> _updateFinishedSemaphores; ///< セマフォ
    csmVector<VkCommandBuffer> _updateCommandBuffers; ///< 更新用コマンドバッファ
    csmVector<VkCommandBuffer> _drawCommandBuffers; ///< 描画用コマンドバッファ

    csmBool _isClearedModelRenderTarget; ///< レンダーターゲットをクリアしたか
};
}}}}

//------------ LIVE2D NAMESPACE ------------
