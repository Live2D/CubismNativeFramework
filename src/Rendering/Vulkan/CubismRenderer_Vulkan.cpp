/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismRenderer_Vulkan.hpp"
#include "CubismOffscreenManager_Vulkan.hpp"
#include "Math/CubismMatrix44.hpp"
#include "Type/csmVector.hpp"
#include "Model/CubismModel.hpp"
#include "Rendering/csmBlendMode.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {
// 各種静的変数
namespace {
bool s_useRenderTarget = false;

VkDevice s_device = VK_NULL_HANDLE;
VkPhysicalDevice s_physicalDevice = VK_NULL_HANDLE;
VkCommandPool s_commandPool = VK_NULL_HANDLE;
VkQueue s_queue = VK_NULL_HANDLE;
csmUint32 s_bufferSetNum = 0;

// 描画対象情報
VkExtent2D s_renderExtent;
VkImage s_renderImage;
VkImageView s_imageView;
VkFormat s_imageFormat;
VkFormat s_depthFormat;

const ModelVertex modelRenderTargetVertexArray[] = {
    {{-1.0f, -1.0f}, {0.0f, 0.0f}},
    {{ 1.0f, -1.0f}, {1.0f, 0.0f}},
    {{ 1.0f,  1.0f}, {1.0f, 1.0f}},
    {{-1.0f,  1.0f}, {0.0f, 1.0f}}
};

const csmUint16 modelRenderTargetIndexArray[] = {
    0, 1, 2,
    2, 3, 0
};
}

VkViewport GetViewport(csmFloat32 width, csmFloat32 height, csmFloat32 minDepth, csmFloat32 maxDepth)
{
    VkViewport viewport{};
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    return viewport;
}

VkRect2D GetScissor(csmFloat32 offsetX, csmFloat32 offsetY, csmFloat32 width, csmFloat32 height)
{
    VkRect2D rect{};
    rect.offset.x = offsetX;
    rect.offset.y = offsetY;
    rect.extent.height = height;
    rect.extent.width = width;
    return rect;
}


csmInt32 GetShaderNamesBegin(const csmBlendMode blendMode)
{
#define CSM_GET_SHADER_NAME(COLOR, ALPHA) ShaderNames_ ## COLOR ## ALPHA

#define CSM_SWITCH_ALPHA_BLEND(COLOR) {\
    switch (blendMode.GetAlphaBlendType()) \
    { \
    case Core::csmAlphaBlendType_Over: \
    default: \
        return CSM_GET_SHADER_NAME(COLOR, Over); \
    case Core::csmAlphaBlendType_Atop: \
        return CSM_GET_SHADER_NAME(COLOR, Atop); \
    case Core::csmAlphaBlendType_Out: \
        return CSM_GET_SHADER_NAME(COLOR, Out); \
    case Core::csmAlphaBlendType_ConjointOver: \
        return CSM_GET_SHADER_NAME(COLOR, ConjointOver); \
    case Core::csmAlphaBlendType_DisjointOver: \
        return CSM_GET_SHADER_NAME(COLOR, DisjointOver); \
    } \
}

    switch (blendMode.GetColorBlendType())
    {
    case Core::csmColorBlendType_Normal:
    default:
    {
        // Normal Over　のときは5.2以前の描画方法を利用する
        switch (blendMode.GetAlphaBlendType())
        {
        case Core::csmAlphaBlendType_Over:
        default:
            return ShaderNames_Normal;
        case Core::csmAlphaBlendType_Atop:
            return CSM_GET_SHADER_NAME(Normal, Atop);
        case Core::csmAlphaBlendType_Out:
            return CSM_GET_SHADER_NAME(Normal, Out);
        case Core::csmAlphaBlendType_ConjointOver:
            return CSM_GET_SHADER_NAME(Normal, ConjointOver);
        case Core::csmAlphaBlendType_DisjointOver:
            return CSM_GET_SHADER_NAME(Normal, DisjointOver);
        }
    }
    break;
    case Core::csmColorBlendType_AddCompatible:
        // AddCompatible は5.2以前の描画方法を利用する
        return ShaderNames_Add;
    case Core::csmColorBlendType_MultiplyCompatible:
        // MultCompatible は5.2以前の描画方法を利用する
        return ShaderNames_Mult;
    case Core::csmColorBlendType_Add:
        CSM_SWITCH_ALPHA_BLEND(Add);
        break;
    case Core::csmColorBlendType_AddGlow:
        CSM_SWITCH_ALPHA_BLEND(AddGlow);
        break;
    case Core::csmColorBlendType_Darken:
        CSM_SWITCH_ALPHA_BLEND(Darken);
        break;
    case Core::csmColorBlendType_Multiply:
        CSM_SWITCH_ALPHA_BLEND(Multiply);
        break;
    case Core::csmColorBlendType_ColorBurn:
        CSM_SWITCH_ALPHA_BLEND(ColorBurn);
        break;
    case Core::csmColorBlendType_LinearBurn:
        CSM_SWITCH_ALPHA_BLEND(LinearBurn);
        break;
    case Core::csmColorBlendType_Lighten:
        CSM_SWITCH_ALPHA_BLEND(Lighten);
        break;
    case Core::csmColorBlendType_Screen:
        CSM_SWITCH_ALPHA_BLEND(Screen);
        break;
    case Core::csmColorBlendType_ColorDodge:
        CSM_SWITCH_ALPHA_BLEND(ColorDodge);
        break;
    case Core::csmColorBlendType_Overlay:
        CSM_SWITCH_ALPHA_BLEND(Overlay);
        break;
    case Core::csmColorBlendType_SoftLight:
        CSM_SWITCH_ALPHA_BLEND(SoftLight);
        break;
    case Core::csmColorBlendType_HardLight:
        CSM_SWITCH_ALPHA_BLEND(HardLight);
        break;
    case Core::csmColorBlendType_LinearLight:
        CSM_SWITCH_ALPHA_BLEND(LinearLight);
        break;
    case Core::csmColorBlendType_Hue:
        CSM_SWITCH_ALPHA_BLEND(Hue);
        break;
    case Core::csmColorBlendType_Color:
        CSM_SWITCH_ALPHA_BLEND(Color);
        break;
    }

#undef CSM_SWITCH_ALPHA_BLEND
#undef CSM_GET_SHADER_NAME
}

/*********************************************************************************************************************
*                                      CubismClippingManager_Vulkan
********************************************************************************************************************/

void CubismClippingManager_Vulkan::SetupClippingContext(CubismModel& model, VkCommandBuffer commandBuffer,
                                                        VkCommandBuffer updateCommandBuffer,
                                                        CubismRenderer_Vulkan* renderer, csmInt32 commandBufferCurrent,
                                                        CubismRenderer::DrawableObjectType drawableObjectType)
{
    // 全てのクリッピングを用意する
    // 同じクリップ（複数の場合はまとめて１つのクリップ）を使う場合は１度だけ設定する
    csmInt32 usingClipCount = 0;

    for (csmUint32 clipIndex = 0; clipIndex < _clippingContextListForMask.GetSize(); clipIndex++)
    {
        // １つのクリッピングマスクに関して
        CubismClippingContext_Vulkan* cc = _clippingContextListForMask[clipIndex];

        // このクリップを利用する描画オブジェクト群全体を囲む矩形を計算
        CalcClippedTotalBounds(model, cc, drawableObjectType);

        if (cc->_isUsing)
        {
            usingClipCount++; //使用中としてカウント
        }
    }

    if (usingClipCount <= 0)
    {
        return;
    }

    // マスク作成処理
    // 後の計算のためにインデックスの最初をセット
    switch (drawableObjectType)
    {
    case CubismRenderer::DrawableObjectType_Drawable:
        _currentMaskBuffer = renderer->GetDrawableMaskBuffer(commandBufferCurrent, 0);
        break;
    case CubismRenderer::DrawableObjectType_Offscreen:
        _currentMaskBuffer = renderer->GetOffscreenMaskBuffer(commandBufferCurrent, 0);
        break;
    }

    // 1が無効（描かれない）領域、0が有効（描かれる）領域。（シェーダで Cd*Csで0に近い値をかけてマスクを作る。1をかけると何も起こらない）
    _currentMaskBuffer->BeginDraw(commandBuffer, 1.0f, 1.0f, 1.0f, 1.0f, true);

    // 生成したFrameBufferと同じサイズでビューポートを設定
    const VkViewport viewport = GetViewport(
        static_cast<csmFloat32>(_clippingMaskBufferSize.X),
        static_cast<csmFloat32>(_clippingMaskBufferSize.Y),
        0.0, 1.0
    );
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    const VkRect2D rect = GetScissor(
        0.0, 0.0,
        static_cast<csmFloat32>(_clippingMaskBufferSize.X),
        static_cast<csmFloat32>(_clippingMaskBufferSize.Y)
    );
    vkCmdSetScissor(commandBuffer, 0, 1, &rect);

    // 各マスクのレイアウトを決定していく
    SetupLayoutBounds(usingClipCount);

    // サイズがレンダーテクスチャの枚数と合わない場合は合わせる
    if (_clearedMaskBufferFlags.GetSize() != _renderTextureCount)
    {
        _clearedMaskBufferFlags.Clear();

        for (csmInt32 i = 0; i < _renderTextureCount; ++i)
        {
            _clearedMaskBufferFlags.PushBack(false);
        }
    }
    else
    {
        // マスクのクリアフラグを毎フレーム開始時に初期化
        for (csmInt32 i = 0; i < _renderTextureCount; ++i)
        {
            _clearedMaskBufferFlags[i] = false;
        }
    }

    // 実際にマスクを生成する
    // 全てのマスクをどの様にレイアウトして描くかを決定し、ClipContext , ClippedDrawContext に記憶する
    for (csmUint32 clipIndex = 0; clipIndex < _clippingContextListForMask.GetSize(); clipIndex++)
    {
        // --- 実際に１つのマスクを描く ---
        CubismClippingContext_Vulkan* clipContext = _clippingContextListForMask[clipIndex];
        csmRectF* allClippedDrawRect = clipContext->_allClippedDrawRect; //このマスクを使う、全ての描画オブジェクトの論理座標上の囲み矩形
        csmRectF* layoutBoundsOnTex01 = clipContext->_layoutBounds; //この中にマスクを収める
        const csmFloat32 MARGIN = 0.05f;
        CubismRenderTarget_Vulkan* maskBuffer = NULL;
        // clipContextに設定したオフスクリーンサーフェイスをインデックスで取得
        switch (drawableObjectType)
        {
        case CubismRenderer::DrawableObjectType_Drawable:
            maskBuffer = renderer->GetDrawableMaskBuffer(commandBufferCurrent, clipContext->_bufferIndex);
            break;
        case CubismRenderer::DrawableObjectType_Offscreen:
            maskBuffer = renderer->GetOffscreenMaskBuffer(commandBufferCurrent, clipContext->_bufferIndex);
            break;
        }

        // 現在のレンダーターゲットがclipContextのものと異なる場合
        if (_currentMaskBuffer != maskBuffer)
        {
            _currentMaskBuffer->EndDraw(commandBuffer);
            _currentMaskBuffer = maskBuffer;
            // マスク用RenderTextureをactiveにセット
            _currentMaskBuffer->BeginDraw(commandBuffer, 1.0f, 1.0f, 1.0f, 1.0f, true);
        }

        // モデル座標上の矩形を、適宜マージンを付けて使う
        _tmpBoundsOnModel.SetRect(allClippedDrawRect);
        _tmpBoundsOnModel.Expand(allClippedDrawRect->Width * MARGIN, allClippedDrawRect->Height * MARGIN);
        //########## 本来は割り当てられた領域の全体を使わず必要最低限のサイズがよい
        // シェーダ用の計算式を求める。回転を考慮しない場合は以下のとおり
        // movePeriod' = movePeriod * scaleX + offX     [[ movePeriod' = (movePeriod - tmpBoundsOnModel.movePeriod)*scale + layoutBoundsOnTex01.movePeriod ]]
        csmFloat32 scaleX = layoutBoundsOnTex01->Width / _tmpBoundsOnModel.Width;
        csmFloat32 scaleY = layoutBoundsOnTex01->Height / _tmpBoundsOnModel.Height;

        // マスク生成時に使う行列を求める
        CreateMatrixForMask(false, layoutBoundsOnTex01, scaleX, scaleY);

        clipContext->_matrixForMask.SetMatrix(_tmpMatrixForMask.GetArray());
        clipContext->_matrixForDraw.SetMatrix(_tmpMatrixForDraw.GetArray());

        // 実際の描画を行う
        const csmInt32 clipDrawCount = clipContext->_clippingIdCount;
        for (csmInt32 i = 0; i < clipDrawCount; i++)
        {
            const csmInt32 clipDrawIndex = clipContext->_clippingIdList[i];

            // 頂点情報が更新されておらず、信頼性がない場合は描画をパスする
            if (!model.GetDrawableDynamicFlagVertexPositionsDidChange(clipDrawIndex))
            {
                continue;
            }

            renderer->IsCulling(model.GetDrawableCulling(clipDrawIndex) != 0);

            // レンダーパス開始時にマスクはクリアされているのでクリアする必要なし
            // 今回専用の変換を適用して描く
            // チャンネルも切り替える必要がある(A,R,G,B)
            renderer->SetClippingContextBufferForMask(clipContext);
            renderer->DrawMeshVulkan(model, clipDrawIndex, commandBuffer, updateCommandBuffer);
        }
    }
    // --- 後処理 ---
    _currentMaskBuffer->EndDraw(commandBuffer);
    renderer->SetClippingContextBufferForMask(NULL);
}

/*********************************************************************************************************************
*                                      CubismClippingContext_Vulkan
********************************************************************************************************************/

CubismClippingContext_Vulkan::CubismClippingContext_Vulkan(
    CubismClippingManager<CubismClippingContext_Vulkan, CubismRenderTarget_Vulkan>* manager, CubismModel& model,
    const csmInt32* clippingDrawableIndices, csmInt32 clipCount)
    : CubismClippingContext(clippingDrawableIndices, clipCount)
{
    _isUsing = false;

    _owner = manager;
}

CubismClippingContext_Vulkan::~CubismClippingContext_Vulkan()
{}

CubismClippingManager<CubismClippingContext_Vulkan, CubismRenderTarget_Vulkan>*
CubismClippingContext_Vulkan::GetClippingManager()
{
    return _owner;
}

/*********************************************************************************************************************
*                                      CubismPipeline_Vulkan
********************************************************************************************************************/

namespace {
const csmInt32 ShaderCount = ShaderNames_ShaderCount;
///<シェーダの数 = コピー用 + マスク生成用 + (通常 + 加算 + 乗算 + ブレンドモードの組み合わせ数) * (マスク無 + マスク有 + マスク有反転 + マスク無の乗算済アルファ対応版 + マスク有の乗算済アルファ対応版 + マスク有反転の乗算済アルファ対応版)
CubismPipeline_Vulkan* s_pipelineManager;
}

CubismPipeline_Vulkan::CubismPipeline_Vulkan()
{}

CubismPipeline_Vulkan::~CubismPipeline_Vulkan()
{
    ReleaseShaderProgram();
}

VkShaderModule CubismPipeline_Vulkan::PipelineResource::CreateShaderModule(VkDevice device, csmString filename)
{
    std::ifstream file(filename.GetRawString(), std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        CubismLogError("failed to open file!");
        return NULL;
    }

    csmInt32 fileSize = (csmInt32)file.tellg();
    csmVector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.GetPtr(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = reinterpret_cast<const csmUint32*>(buffer.GetPtr());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        CubismLogError("failed to create shader module!");
        return NULL;
    }

    return shaderModule;
}

csmBool CubismPipeline_Vulkan::PipelineResource::CreateGraphicsPipeline(csmString vertFileName, csmString fragFileName,
                                                                     VkDescriptorSetLayout descriptorSetLayout,
                                                                     csmUint32 shaderName,
                                                                     csmInt32 colorBlendMode, csmInt32 alphaBlendMode)
{
    VkShaderModule vertShaderModule = CreateShaderModule(s_device, vertFileName);
    if (vertShaderModule == NULL)
    {
        return false;
    }

    VkShaderModule fragShaderModule = CreateShaderModule(s_device, fragFileName);
    if (fragShaderModule == NULL)
    {
        return false;
    }

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkVertexInputBindingDescription bindingDescription = ModelVertex::GetBindingDescription();
    VkVertexInputAttributeDescription attributeDescriptions[2];
    ModelVertex::GetAttributeDescriptions(attributeDescriptions);
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = sizeof(attributeDescriptions) / sizeof(attributeDescriptions[0]);
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkDynamicState dynamicState[3] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_CULL_MODE};

    VkPipelineDynamicStateCreateInfo dynamicStateCI{};
    dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCI.dynamicStateCount = sizeof(dynamicState) / sizeof(dynamicState[0]);
    dynamicStateCI.pDynamicStates = dynamicState;

    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};
    pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
    pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
    pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &s_imageFormat;
    renderingInfo.depthAttachmentFormat = s_depthFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.pDynamicState = &dynamicStateCI;
    pipelineInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
    pipelineInfo.pNext = &renderingInfo;

    if (shaderName == ShaderNames_Copy || (colorBlendMode != ColorBlendMode_None && alphaBlendMode != AlphaBlendMode_None))
    {   // 5.3以降
        _pipeline.Resize(1);
        _pipelineLayout.Resize(1);

        if (shaderName == ShaderNames_Copy)
        {
            colorBlendAttachment.blendEnable = VK_TRUE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        }

        if (vkCreatePipelineLayout(s_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout[0]) != VK_SUCCESS)
        {
            CubismLogError("failed to create _pipeline layout!");
        }

        pipelineInfo.layout = _pipelineLayout[0];
        if (vkCreateGraphicsPipelines(s_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline[0]) !=
            VK_SUCCESS)
        {
            CubismLogError("failed to create graphics _pipeline!");
        }
    }
    else
    {   // 5.2以前
        _pipeline.Resize(4);
        _pipelineLayout.Resize(4);
        // 通常
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        if (vkCreatePipelineLayout(s_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout[CompatibleBlend::Blend_Normal]) != VK_SUCCESS)
        {
            CubismLogError("failed to create _pipeline layout!");
        }

        pipelineInfo.layout = _pipelineLayout[CompatibleBlend::Blend_Normal];
        if (vkCreateGraphicsPipelines(s_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline[CompatibleBlend::Blend_Normal]) !=
            VK_SUCCESS)
        {
            CubismLogError("failed to create graphics _pipeline!");
        }

        // 加算
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        if (vkCreatePipelineLayout(s_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout[CompatibleBlend::Blend_Add]) != VK_SUCCESS)
        {
            CubismLogError("failed to create _pipeline layout!");
        }
        pipelineInfo.layout = _pipelineLayout[CompatibleBlend::Blend_Add];
        if (vkCreateGraphicsPipelines(s_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline[CompatibleBlend::Blend_Add]) !=
            VK_SUCCESS)
        {
            CubismLogError("failed to create graphics _pipeline!");
        }

        // 乗算
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        if (vkCreatePipelineLayout(s_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout[CompatibleBlend::Blend_Mult]) != VK_SUCCESS)
        {
            CubismLogError("failed to create _pipeline layout!");
        }
        pipelineInfo.layout = _pipelineLayout[CompatibleBlend::Blend_Mult];
        if (vkCreateGraphicsPipelines(s_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline[CompatibleBlend::Blend_Mult]) !=
            VK_SUCCESS)
        {
            CubismLogError("failed to create graphics _pipeline!");
        }

        // マスク
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        if (vkCreatePipelineLayout(s_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout[CompatibleBlend::Blend_Mask]) != VK_SUCCESS)
        {
            CubismLogError("failed to create _pipeline layout!");
        }
        renderingInfo.pColorAttachmentFormats = &s_imageFormat;
        pipelineInfo.layout = _pipelineLayout[CompatibleBlend::Blend_Mask];
        if (vkCreateGraphicsPipelines(s_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline[CompatibleBlend::Blend_Mask]) !=
            VK_SUCCESS)
        {
            CubismLogError("failed to create graphics _pipeline!");
        }
    }

    vkDestroyShaderModule(s_device, vertShaderModule, nullptr);
    vkDestroyShaderModule(s_device, fragShaderModule, nullptr);

    return true;
}

void CubismPipeline_Vulkan::PipelineResource::Release()
{
    for (csmInt32 i = 0; i < _pipeline.GetSize(); i++)
    {
        if (_pipeline.GetPtr() != NULL)
        {
            vkDestroyPipeline(s_device, _pipeline[i], nullptr);
        }
        if (_pipelineLayout.GetPtr() != NULL)
        {
            vkDestroyPipelineLayout(s_device, _pipelineLayout[i], nullptr);
        }
    }
    _pipeline.Clear();
    _pipelineLayout.Clear();
}

void CubismPipeline_Vulkan::CreatePipelines(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSetLayout copyDescriptorSetLayout)
{
    //パイプラインを作成済みの場合は生成する必要なし
    if (_pipelineResource.GetSize() != 0)
    {
        return;
    }

    _pipelineResource.Resize(ShaderCount);
    for (csmInt32 i = 0; i < ShaderCount; i++)
    {
        _pipelineResource[i] = CSM_NEW PipelineResource();
    }

    CreatePipelineResource(ShaderNames_Copy, "FrameworkShaders/VertShaderSrcCopy.spv", "FrameworkShaders/FragShaderSrcCopy.spv", copyDescriptorSetLayout);
    CreatePipelineResource(ShaderNames_SetupMask, "FrameworkShaders/VertShaderSrcSetupMask.spv", "FrameworkShaders/FragShaderSrcSetupMask.spv", descriptorSetLayout);

    // 通常
    CreatePipelineResource(ShaderNames_Normal, "FrameworkShaders/VertShaderSrc.spv", "FrameworkShaders/FragShaderSrc.spv", descriptorSetLayout);
    CreatePipelineResource(ShaderNames_NormalMasked, "FrameworkShaders/VertShaderSrcMasked.spv", "FrameworkShaders/FragShaderSrcMask.spv", descriptorSetLayout);
    CreatePipelineResource(ShaderNames_NormalMaskedInverted, "FrameworkShaders/VertShaderSrcMasked.spv", "FrameworkShaders/FragShaderSrcMaskInverted.spv", descriptorSetLayout);
    CreatePipelineResource(ShaderNames_NormalPremultipliedAlpha, "FrameworkShaders/VertShaderSrc.spv", "FrameworkShaders/FragShaderSrcPremultipliedAlpha.spv", descriptorSetLayout);
    CreatePipelineResource(ShaderNames_NormalMaskedPremultipliedAlpha, "FrameworkShaders/VertShaderSrcMasked.spv", "FrameworkShaders/FragShaderSrcMaskPremultipliedAlpha.spv", descriptorSetLayout);
    CreatePipelineResource(ShaderNames_NormalMaskedInvertedPremultipliedAlpha, "FrameworkShaders/VertShaderSrcMasked.spv", "FrameworkShaders/FragShaderSrcMaskInvertedPremultipliedAlpha.spv", descriptorSetLayout);

    // 加算
    _pipelineResource[ShaderNames_Add] = _pipelineResource[ShaderNames_Normal];
    _pipelineResource[ShaderNames_AddMasked] = _pipelineResource[ShaderNames_NormalMasked];
    _pipelineResource[ShaderNames_AddMaskedInverted] = _pipelineResource[ShaderNames_NormalMaskedInverted];
    _pipelineResource[ShaderNames_AddPremultipliedAlpha] = _pipelineResource[ShaderNames_NormalPremultipliedAlpha];
    _pipelineResource[ShaderNames_AddMaskedPremultipliedAlpha] = _pipelineResource[ShaderNames_NormalMaskedPremultipliedAlpha];
    _pipelineResource[ShaderNames_AddMaskedInvertedPremultipliedAlpha] = _pipelineResource[ShaderNames_NormalMaskedInvertedPremultipliedAlpha];

    // 乗算
    _pipelineResource[ShaderNames_Mult] = _pipelineResource[ShaderNames_Normal];
    _pipelineResource[ShaderNames_MultMasked] = _pipelineResource[ShaderNames_NormalMasked];
    _pipelineResource[ShaderNames_MultMaskedInverted] = _pipelineResource[ShaderNames_NormalMaskedInverted];
    _pipelineResource[ShaderNames_MultPremultipliedAlpha] = _pipelineResource[ShaderNames_NormalPremultipliedAlpha];
    _pipelineResource[ShaderNames_MultMaskedPremultipliedAlpha] = _pipelineResource[ShaderNames_NormalMaskedPremultipliedAlpha];
    _pipelineResource[ShaderNames_MultMaskedInvertedPremultipliedAlpha] = _pipelineResource[ShaderNames_NormalMaskedInvertedPremultipliedAlpha];

    // ブレンドモードの組み合わせ分作成
    {
        csmUint32 offset = ShaderNames_NormalAtop;
        for (csmInt32 i = 0; i <= Core::csmColorBlendType_Color; ++i)
        {
            if (i == Core::csmColorBlendType_AddCompatible || i == Core::csmColorBlendType_MultiplyCompatible)
            {
                continue;
            }

            // Normal Overはシェーダを作る必要がないため 1 から始める
            const csmInt32 start = (i == 0 ? 1 : 0);
            csmString shaderDirectory = "FrameworkShaders/";
            csmString fragShaderExt = ".spv";
            for (csmInt32 j = start; j <= Core::csmAlphaBlendType_DisjointOver; ++j)
            {
                csmString colorBlendModeName = csmBlendMode::ColorBlendModeToString(i);
                csmString alphaBlendModeName = csmBlendMode::AlphaBlendModeToString(j);

                csmString baseFragShaderFileName = "FragShaderSrcBlend";
                csmString fragShaderFileName = shaderDirectory + baseFragShaderFileName + colorBlendModeName + alphaBlendModeName + fragShaderExt;
                CreatePipelineResource(offset++, "FrameworkShaders/VertShaderSrcBlend.spv", fragShaderFileName, descriptorSetLayout, i, j);

                baseFragShaderFileName = "FragShaderSrcMaskBlend";
                fragShaderFileName = shaderDirectory + baseFragShaderFileName + colorBlendModeName + alphaBlendModeName + fragShaderExt;
                CreatePipelineResource(offset++, "FrameworkShaders/VertShaderSrcMaskedBlend.spv", fragShaderFileName, descriptorSetLayout, i, j);

                baseFragShaderFileName = "FragShaderSrcMaskInvertedBlend";
                fragShaderFileName = shaderDirectory + baseFragShaderFileName + colorBlendModeName + alphaBlendModeName + fragShaderExt;
                CreatePipelineResource(offset++, "FrameworkShaders/VertShaderSrcMaskedBlend.spv", fragShaderFileName, descriptorSetLayout, i, j);

                baseFragShaderFileName = "FragShaderSrcPremultipliedAlphaBlend";
                fragShaderFileName = shaderDirectory + baseFragShaderFileName + colorBlendModeName + alphaBlendModeName + fragShaderExt;
                CreatePipelineResource(offset++, "FrameworkShaders/VertShaderSrcBlend.spv", fragShaderFileName, descriptorSetLayout, i, j);

                baseFragShaderFileName = "FragShaderSrcMaskPremultipliedAlphaBlend";
                fragShaderFileName = shaderDirectory + baseFragShaderFileName + colorBlendModeName + alphaBlendModeName + fragShaderExt;
                CreatePipelineResource(offset++, "FrameworkShaders/VertShaderSrcMaskedBlend.spv", fragShaderFileName, descriptorSetLayout, i, j);

                baseFragShaderFileName = "FragShaderSrcMaskInvertedPremultipliedAlphaBlend";
                fragShaderFileName = shaderDirectory + baseFragShaderFileName + colorBlendModeName + alphaBlendModeName + fragShaderExt;
                CreatePipelineResource(offset++, "FrameworkShaders/VertShaderSrcMaskedBlend.spv", fragShaderFileName, descriptorSetLayout, i, j);
            }
        }
    }
}

void CubismPipeline_Vulkan::CreatePipelineResource(csmUint32 const& shaderName, csmString const& vertShaderFileName, csmString const& fragShaderFileName, VkDescriptorSetLayout const& descriptorSetLayout, csmInt32 const& colorBlendMode, csmInt32 const& alphaBlendMode)
{
    if (!_pipelineResource[shaderName]->CreateGraphicsPipeline(vertShaderFileName, fragShaderFileName, descriptorSetLayout, shaderName, colorBlendMode, alphaBlendMode))
    {
        _pipelineResource[shaderName] = NULL;
        CubismLogError("failed to create shader %d", shaderName);
    }
}

CubismPipeline_Vulkan* CubismPipeline_Vulkan::GetInstance()
{
    if (s_pipelineManager == NULL)
    {
        s_pipelineManager = CSM_NEW CubismPipeline_Vulkan();
    }
    return s_pipelineManager;
}

void CubismPipeline_Vulkan::ReleaseShaderProgram()
{
    for (csmInt32 i = 0; i < _pipelineResource.GetSize(); i++)
    {
        if (i >= ShaderNames_Add && i <= ShaderNames_MultMaskedInvertedPremultipliedAlpha)
        {
            // 加算と乗算は通常と同じリソースを参照しており2重解放になってしまうためスキップ
            continue;
        }

        if (_pipelineResource[i] != NULL)
        {
            _pipelineResource[i]->Release();
            CSM_DELETE(_pipelineResource[i]);
            _pipelineResource[i] = NULL;
        }
    }
}

/*********************************************************************************************************************
*                                       CubismRenderer_Vulkan
********************************************************************************************************************/

CubismRenderer* CubismRenderer::Create(csmUint32 width, csmUint32 height)
{
    return CSM_NEW CubismRenderer_Vulkan(width, height);
}

void CubismRenderer::StaticRelease()
{
    CubismRenderer_Vulkan::DoStaticRelease();
}

CubismRenderer_Vulkan::CubismRenderer_Vulkan(csmUint32 width, csmUint32 height) :
                                               CubismRenderer(width, height)
                                               , vkCmdSetCullModeEXT()
                                               , _drawableClippingManager(NULL)
                                               , _clippingContextBufferForMask(NULL)
                                               , _clippingContextBufferForDraw(NULL)
                                               , _offscreenClippingManager(NULL)
                                               , _clippingContextBufferForOffscreen(NULL)
                                               , _currentOffscreen(NULL)
                                               , _currentRenderTarget(NULL)
                                               , _descriptorPool(VK_NULL_HANDLE)
                                               , _descriptorSetLayout(VK_NULL_HANDLE)
                                               , _clearColor()
                                               , _commandBufferCurrent(0)
{
}

CubismRenderer_Vulkan::~CubismRenderer_Vulkan()
{
    CSM_DELETE_SELF(CubismClippingManager_Vulkan, _drawableClippingManager);
    CSM_DELETE_SELF(CubismClippingManager_Vulkan, _offscreenClippingManager);

    // オフスクリーンを作成していたのなら開放
    for (csmInt32 i = 0; i < _modelRenderTargets.GetSize(); i++)
    {
        if (_modelRenderTargets[i].IsValid())
        {
            _modelRenderTargets[i].DestroyRenderTarget();
        }
    }
    _modelRenderTargets.Clear();

    for (csmUint32 buffer = 0; buffer < _drawableMaskBuffers.GetSize(); buffer++)
    {
        for (csmInt32 i = 0; i < _drawableMaskBuffers[buffer].GetSize(); i++)
        {
            if (_drawableMaskBuffers[buffer][i].IsValid())
            {
                _drawableMaskBuffers[buffer][i].DestroyRenderTarget();
            }
        }
        _drawableMaskBuffers[buffer].Clear();
    }
    _drawableMaskBuffers.Clear();

    // オフスクリーン用マスクバッファを開放
    for (csmUint32 buffer = 0; buffer < _offscreenMaskBuffers.GetSize(); buffer++)
    {
        for (csmInt32 i = 0; i < _offscreenMaskBuffers[buffer].GetSize(); i++)
        {
            if (_offscreenMaskBuffers[buffer][i].IsValid())
            {
                _offscreenMaskBuffers[buffer][i].DestroyRenderTarget();
            }
        }
        _offscreenMaskBuffers[buffer].Clear();
    }
    _offscreenMaskBuffers.Clear();

    _depthImage.Destroy(s_device);

    // セマフォ解放
    for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
    {
        vkDestroySemaphore(s_device, _updateFinishedSemaphores[buffer], nullptr);
    }

    // ディスクリプタ関連解放
    vkDestroyDescriptorPool(s_device, _descriptorPool, nullptr);
    vkDestroyDescriptorPool(s_device, _offscreenDescriptorPool, nullptr);
    vkDestroyDescriptorPool(s_device, _copyDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(s_device, _descriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_device, _copyDescriptorSetLayout, nullptr);

    for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
    {
        // Drawable
        for (csmInt32 i = 0; i < _descriptorSets[buffer].GetSize(); i++)
        {
            _descriptorSets[buffer][i].uniformBuffer.Destroy(s_device);
            _descriptorSets[buffer][i].uniformBufferMask.Destroy(s_device);
        }

        // offscreen
        for (csmInt32 i = 0; i < _offscreenDescriptorSets[buffer].GetSize(); ++i)
        {
            _offscreenDescriptorSets[buffer][i].uniformBuffer.Destroy(s_device);
        }

        _copyDescriptorSets[buffer].uniformBuffer.Destroy(s_device);
    }

    // その他バッファ開放
    for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
    {
        for (csmUint32 drawAssign = 0; drawAssign < _vertexBuffers[buffer].GetSize(); drawAssign++)
        {
            _vertexBuffers[buffer][drawAssign].Destroy(s_device);
            _stagingBuffers[buffer][drawAssign].Destroy(s_device);
            _indexBuffers[buffer][drawAssign].Destroy(s_device);
        }

        for (csmUint32 offscreenAssign = 0; offscreenAssign < _offscreenVertexBuffers[buffer].GetSize(); ++offscreenAssign)
        {
            _offscreenVertexBuffers[buffer][offscreenAssign].Destroy(s_device);
            _offscreenStagingBuffers[buffer][offscreenAssign].Destroy(s_device);
            _offscreenIndexBuffers[buffer][offscreenAssign].Destroy(s_device);
        }

        _copyVertexBuffer[buffer].Destroy(s_device);
        _copyStagingBuffer[buffer].Destroy(s_device);
        _copyIndexBuffer[buffer].Destroy(s_device);
    }
}

void CubismRenderer_Vulkan::DoStaticRelease()
{
    if (s_pipelineManager)
    {
        CSM_DELETE_SELF(CubismPipeline_Vulkan, s_pipelineManager);
        s_pipelineManager = NULL;
    }
}

VkCommandBuffer CubismRenderer_Vulkan::BeginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = s_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(s_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void CubismRenderer_Vulkan::SubmitCommand(VkCommandBuffer commandBuffer, VkSemaphore signalUpdateFinishedSemaphore, VkSemaphore waitUpdateFinishedSemaphore)
{
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    if (waitUpdateFinishedSemaphore != VK_NULL_HANDLE)
    {
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitUpdateFinishedSemaphore;
        submitInfo.pWaitDstStageMask = waitStages;
        vkQueueSubmit(s_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(s_queue);
    }
    else if (signalUpdateFinishedSemaphore != VK_NULL_HANDLE)
    {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalUpdateFinishedSemaphore;
        vkQueueSubmit(s_queue, 1, &submitInfo, VK_NULL_HANDLE);
    }
    else
    {
        vkQueueSubmit(s_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(s_queue);
        vkFreeCommandBuffers(s_device, s_commandPool, 1, &commandBuffer);
    }

}

void CubismRenderer_Vulkan::InitializeConstantSettings(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                                       csmUint32 swapchainImageCount, VkExtent2D extent, VkImageView imageView, VkFormat imageFormat, VkFormat depthFormat)
{
    s_device = device;
    s_physicalDevice = physicalDevice;
    s_commandPool = commandPool;
    s_queue = queue;
    s_bufferSetNum = swapchainImageCount;

    s_renderExtent = extent;
    s_imageView = imageView;
    s_imageFormat = imageFormat;
    s_depthFormat = depthFormat;
}

void CubismRenderer_Vulkan::EnableChangeRenderTarget()
{
    s_useRenderTarget = true;
}

void CubismRenderer_Vulkan::SetRenderTarget(VkImage image, VkImageView view, VkFormat format, VkExtent2D extent)
{
    s_renderImage = image;
    s_imageView = view;
    s_imageFormat = format;
    s_renderExtent = extent;
}

void CubismRenderer_Vulkan::SetImageExtent(VkExtent2D extent)
{
    s_renderExtent = extent;
}

void CubismRenderer_Vulkan::CreateCommandBuffer()
{
    _updateCommandBuffers.Resize(s_bufferSetNum);
    _drawCommandBuffers.Resize(s_bufferSetNum);
    for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = s_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(s_device, &allocInfo, &_updateCommandBuffers[buffer]) != VK_SUCCESS ||
            vkAllocateCommandBuffers(s_device, &allocInfo, &_drawCommandBuffers[buffer]) != VK_SUCCESS)
        {
            CubismLogError("failed to allocate command buffers!");
        }
    }
}

void CubismRenderer_Vulkan::CreateVertexBuffer()
{
    const csmInt32 drawableCount = GetModel()->GetDrawableCount();
    const csmInt32 offscreenCount = GetModel()->GetOffscreenCount();
    _stagingBuffers.Resize(s_bufferSetNum);
    _vertexBuffers.Resize(s_bufferSetNum);

    _offscreenStagingBuffers.Resize(s_bufferSetNum);
    _offscreenVertexBuffers.Resize(s_bufferSetNum);

    _copyStagingBuffer.Resize(s_bufferSetNum);
    _copyVertexBuffer.Resize(s_bufferSetNum);

    for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
    {
        // Drawable用
        _stagingBuffers[buffer].Resize(drawableCount);
        _vertexBuffers[buffer].Resize(drawableCount);
        for (csmInt32 drawAssign = 0; drawAssign < drawableCount; drawAssign++)
        {
            const csmInt32 vcount = GetModel()->GetDrawableVertexCount(drawAssign);
            if (vcount != 0)
            {
                VkDeviceSize bufferSize = sizeof(ModelVertex) * vcount; // 総長 構造体サイズ*個数

                CubismBufferVulkan stagingBuffer;
                stagingBuffer.CreateBuffer(s_device, s_physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                stagingBuffer.Map(s_device, bufferSize);
                _stagingBuffers[buffer][drawAssign] = stagingBuffer;

                CubismBufferVulkan vertexBuffer;
                vertexBuffer.CreateBuffer(s_device, s_physicalDevice, bufferSize,
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                _vertexBuffers[buffer][drawAssign] = vertexBuffer;
            }
        }

        // オフスクリーン用
        _offscreenStagingBuffers[buffer].Resize(offscreenCount);
        _offscreenVertexBuffers[buffer].Resize(offscreenCount);

        for (csmInt32 offscreenAssign = 0; offscreenAssign < offscreenCount; ++offscreenAssign)
        {
            VkDeviceSize bufferSize = sizeof(modelRenderTargetVertexArray);

            CubismBufferVulkan stagingBuffer;
            stagingBuffer.CreateBuffer(s_device, s_physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            stagingBuffer.Map(s_device, bufferSize);
            _offscreenStagingBuffers[buffer][offscreenAssign] = stagingBuffer;

            CubismBufferVulkan vertexBuffer;
            vertexBuffer.CreateBuffer(s_device, s_physicalDevice, bufferSize,
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            _offscreenVertexBuffers[buffer][offscreenAssign] = vertexBuffer;
        }

        // コピー用
        VkDeviceSize copyVertexBufferSize = sizeof(modelRenderTargetVertexArray);
        CubismBufferVulkan copyStagingBuffer;
        copyStagingBuffer.CreateBuffer(s_device, s_physicalDevice, copyVertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        copyStagingBuffer.Map(s_device, copyVertexBufferSize);
        _copyStagingBuffer[buffer] = copyStagingBuffer;

        CubismBufferVulkan copyVertexBuffer;
        copyVertexBuffer.CreateBuffer(s_device, s_physicalDevice, copyVertexBufferSize,
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        _copyVertexBuffer[buffer] = copyVertexBuffer;
    }

}

void CubismRenderer_Vulkan::CreateIndexBuffer()
{
    const csmInt32 drawableCount = GetModel()->GetDrawableCount();
    const csmInt32 offscreenCount = GetModel()->GetOffscreenCount();
    _indexBuffers.Resize(s_bufferSetNum);
    _offscreenIndexBuffers.Resize(s_bufferSetNum);
    _copyIndexBuffer.Resize(s_bufferSetNum);

    for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
    {
        // Drawable用
        _indexBuffers[buffer].Resize(drawableCount);
        for (csmInt32 drawAssign = 0; drawAssign < drawableCount; drawAssign++)
        {
            const csmInt32 icount = GetModel()->GetDrawableVertexIndexCount(drawAssign);
            if (icount != 0)
            {
                VkDeviceSize bufferSize = sizeof(uint16_t) * icount;
                const csmUint16* indices = GetModel()->GetDrawableVertexIndices(drawAssign);

                CubismBufferVulkan stagingBuffer;
                stagingBuffer.CreateBuffer(s_device, s_physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                stagingBuffer.Map(s_device, bufferSize);
                stagingBuffer.MemCpy(indices, bufferSize);
                stagingBuffer.UnMap(s_device);

                CubismBufferVulkan indexBuffer;
                indexBuffer.CreateBuffer(s_device, s_physicalDevice, bufferSize,
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

                VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

                VkBufferCopy copyRegion{};
                copyRegion.size = bufferSize;
                vkCmdCopyBuffer(commandBuffer, stagingBuffer.GetBuffer(), indexBuffer.GetBuffer(), 1, &copyRegion);
                SubmitCommand(commandBuffer);
                _indexBuffers[buffer][drawAssign] = indexBuffer;
                stagingBuffer.Destroy(s_device);
            }
        }

        // オフスクリーン用
        _offscreenIndexBuffers[buffer].Resize(offscreenCount);
        for (csmInt32 offscreenAssign = 0; offscreenAssign < offscreenCount; ++offscreenAssign)
        {
            VkDeviceSize bufferSize = sizeof(modelRenderTargetIndexArray);

            CubismBufferVulkan indexBuffer;
            indexBuffer.CreateBuffer(s_device, s_physicalDevice, bufferSize,
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            CubismBufferVulkan stagingBuffer;
            stagingBuffer.CreateBuffer(s_device, s_physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            stagingBuffer.Map(s_device, bufferSize);
            stagingBuffer.MemCpy(&modelRenderTargetIndexArray, bufferSize);
            stagingBuffer.UnMap(s_device);

            VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

            VkBufferCopy copyRegion{};
            copyRegion.size = bufferSize;
            vkCmdCopyBuffer(commandBuffer, stagingBuffer.GetBuffer(), indexBuffer.GetBuffer(), 1, &copyRegion);
            SubmitCommand(commandBuffer);
            _offscreenIndexBuffers[buffer][offscreenAssign] = indexBuffer;
            stagingBuffer.Destroy(s_device);
        }

        // コピー用
        VkDeviceSize copyIndexBufferSize = sizeof(modelRenderTargetIndexArray);
        CubismBufferVulkan copyIndexBuffer;
        copyIndexBuffer.CreateBuffer(s_device, s_physicalDevice, copyIndexBufferSize,
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        CubismBufferVulkan stagingBuffer;
        stagingBuffer.CreateBuffer(s_device, s_physicalDevice, copyIndexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        stagingBuffer.Map(s_device, copyIndexBufferSize);
        stagingBuffer.MemCpy(&modelRenderTargetIndexArray, copyIndexBufferSize);
        stagingBuffer.UnMap(s_device);

        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.size = copyIndexBufferSize;
        vkCmdCopyBuffer(commandBuffer, stagingBuffer.GetBuffer(), copyIndexBuffer.GetBuffer(), 1, &copyRegion);
        SubmitCommand(commandBuffer);
        _copyIndexBuffer[buffer] = copyIndexBuffer;
        stagingBuffer.Destroy(s_device);
    }

}

void CubismRenderer_Vulkan::CreateDescriptorSets()
{
    // ディスクリプタプールの作成
    csmInt32 textureCount = 3;
    csmInt32 drawModeCount = 3; // 通常の描画、マスクされる描画、マスク生成時の描画
    // Drawable用ディスクリプタセットの数
    const csmInt32 drawableCount = GetModel()->GetDrawableCount();
    csmInt32 descriptorSetCountPerBuffer = drawableCount * drawModeCount;
    csmInt32 descriptorSetCount = s_bufferSetNum * descriptorSetCountPerBuffer;
    // オフスクリーン用ディスクリプタセットの数
    const csmInt32 offscreenCount = GetModel()->GetOffscreenCount();
    csmInt32 offscreenDescriptorSetCountPerBuffer = offscreenCount * drawModeCount;
    csmInt32 offscreenDescriptorSetCount = s_bufferSetNum * offscreenDescriptorSetCountPerBuffer;

    // Drawable用のディスクリプタプール作成
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = descriptorSetCount;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = descriptorSetCount * textureCount; // drawableCount * 描画方法の数 * テクスチャの数

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = descriptorSetCount;

    if (vkCreateDescriptorPool(s_device, &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS)
    {
        CubismLogError("failed to create drawable descriptor pool!");
    }

    // オフスクリーン用ディスクリプタプール作成
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = offscreenDescriptorSetCount;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = offscreenDescriptorSetCount * textureCount; // offscreenCount * 描画方法の数 * テクスチャの数

    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = offscreenDescriptorSetCount;

    if (vkCreateDescriptorPool(s_device, &poolInfo, nullptr, &_offscreenDescriptorPool) != VK_SUCCESS)
    {
        CubismLogError("failed to create offscreen descriptor pool!");
    }

    // コピー用ディスクリプタプール作成
    VkDescriptorPoolSize copyPoolSizes[2]{};
    copyPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    copyPoolSizes[0].descriptorCount = s_bufferSetNum;
    copyPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    copyPoolSizes[1].descriptorCount = s_bufferSetNum;

    VkDescriptorPoolCreateInfo copyPoolInfo{};
    copyPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    copyPoolInfo.poolSizeCount = sizeof(copyPoolSizes) / sizeof(copyPoolSizes[0]);
    copyPoolInfo.pPoolSizes = copyPoolSizes;
    copyPoolInfo.maxSets = s_bufferSetNum;

    if (vkCreateDescriptorPool(s_device, &copyPoolInfo, nullptr, &_copyDescriptorPool) != VK_SUCCESS)
    {
        CubismLogError("failed to create copy descriptor pool!");
    }

    // ディスクリプタセットレイアウトの作成（drawableとoffscreenで共通）
    // ユニフォームバッファ用
    VkDescriptorSetLayoutBinding bindings[4];
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_ALL;

    // キャラクターテクスチャ
    bindings[1].binding = 1;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].pImmutableSamplers = nullptr;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // マスク用オフスクリーンに使用するテクスチャ
    bindings[2].binding = 2;
    bindings[2].descriptorCount = 1;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].pImmutableSamplers = nullptr;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // ブレンド用テクスチャ
    bindings[3].binding = 3;
    bindings[3].descriptorCount = 1;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].pImmutableSamplers = nullptr;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = sizeof(bindings) / sizeof(bindings[0]);
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(s_device, &layoutInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS)
    {
        CubismLogError("failed to create descriptor set layout!");
    }

    // コピー用ディスクリプタセットレイアウトの作成
    VkDescriptorSetLayoutBinding copyBindings[2];
    copyBindings[0].binding = 0;
    copyBindings[0].descriptorCount = 1;
    copyBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    copyBindings[0].pImmutableSamplers = nullptr;
    copyBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    copyBindings[1].binding = 1;
    copyBindings[1].descriptorCount = 1;
    copyBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    copyBindings[1].pImmutableSamplers = nullptr;
    copyBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo copyLayoutInfo{};
    copyLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    copyLayoutInfo.bindingCount = sizeof(copyBindings) / sizeof(copyBindings[0]);
    copyLayoutInfo.pBindings = copyBindings;

    if (vkCreateDescriptorSetLayout(s_device, &copyLayoutInfo, nullptr, &_copyDescriptorSetLayout) != VK_SUCCESS)
    {
        CubismLogError("failed to create copy descriptor set layout!");
    }

    // ディスクリプタセットの作成
    _descriptorSets.Resize(s_bufferSetNum);
    _offscreenDescriptorSets.Resize(s_bufferSetNum);
    _copyDescriptorSets.Resize(s_bufferSetNum);

    // Drawable用
    for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
    {
        _descriptorSets[buffer].Resize(drawableCount);
        for (csmInt32 drawAssign = 0; drawAssign < drawableCount; drawAssign++)
        {
            // 通常描画用
            _descriptorSets[buffer][drawAssign].uniformBuffer.CreateBuffer(s_device, s_physicalDevice, sizeof(ModelUBO),
                                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            _descriptorSets[buffer][drawAssign].uniformBuffer.Map(s_device, VK_WHOLE_SIZE);

            // マスク生成時用
            _descriptorSets[buffer][drawAssign].uniformBufferMask.CreateBuffer(s_device, s_physicalDevice, sizeof(ModelUBO),
                                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            _descriptorSets[buffer][drawAssign].uniformBufferMask.Map(s_device, VK_WHOLE_SIZE);
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _descriptorPool;
        allocInfo.descriptorSetCount = descriptorSetCountPerBuffer;
        csmVector<VkDescriptorSetLayout> layouts;
        for (csmInt32 i = 0; i < descriptorSetCountPerBuffer; i++)
        {
            layouts.PushBack(_descriptorSetLayout);
        }
        allocInfo.pSetLayouts = layouts.GetPtr();

        csmVector<VkDescriptorSet> descriptorSets;
        descriptorSets.Resize(descriptorSetCountPerBuffer);
        if (vkAllocateDescriptorSets(s_device, &allocInfo, descriptorSets.GetPtr()) != VK_SUCCESS)
        {
            CubismLogError("failed to allocate descriptor sets!");
        }

        for (csmInt32 drawAssign = 0; drawAssign < drawableCount; drawAssign++)
        {
            _descriptorSets[buffer][drawAssign].descriptorSet = descriptorSets[drawAssign * 3];
            _descriptorSets[buffer][drawAssign].descriptorSetMasked = descriptorSets[drawAssign * 3 + 1];
            _descriptorSets[buffer][drawAssign].descriptorSetForMask = descriptorSets[drawAssign * 3 + 2];
        }
    }

    // オフスクリーン用
    for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
    {
        _offscreenDescriptorSets[buffer].Resize(offscreenCount);
        for (csmInt32 offscreenAssign = 0; offscreenAssign < offscreenCount; offscreenAssign++)
        {
            _offscreenDescriptorSets[buffer][offscreenAssign].uniformBuffer.CreateBuffer(s_device, s_physicalDevice, sizeof(ModelUBO),
                                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            _offscreenDescriptorSets[buffer][offscreenAssign].uniformBuffer.Map(s_device, VK_WHOLE_SIZE);
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _offscreenDescriptorPool;
        allocInfo.descriptorSetCount = offscreenDescriptorSetCountPerBuffer;
        csmVector<VkDescriptorSetLayout> layouts;
        for (csmInt32 i = 0; i < offscreenDescriptorSetCountPerBuffer; i++)
        {
            layouts.PushBack(_descriptorSetLayout);
        }
        allocInfo.pSetLayouts = layouts.GetPtr();

        csmVector<VkDescriptorSet> descriptorSets;
        descriptorSets.Resize(offscreenDescriptorSetCountPerBuffer);
        if (vkAllocateDescriptorSets(s_device, &allocInfo, descriptorSets.GetPtr()) != VK_SUCCESS)
        {
            CubismLogError("failed to allocate descriptor sets!");
        }

        for (csmInt32 offscreenAssign = 0; offscreenAssign < offscreenCount; offscreenAssign++)
        {
            _offscreenDescriptorSets[buffer][offscreenAssign].descriptorSet = descriptorSets[offscreenAssign * 2];
            _offscreenDescriptorSets[buffer][offscreenAssign].descriptorSetMasked = descriptorSets[offscreenAssign * 2 + 1];
        }
    }

    // コピー用
    for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
    {
        _copyDescriptorSets[buffer].uniformBuffer.CreateBuffer(s_device, s_physicalDevice,
                                                                sizeof(csmFloat32) * 4, // BaseColor
                                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        _copyDescriptorSets[buffer].uniformBuffer.Map(s_device, VK_WHOLE_SIZE);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _copyDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &_copyDescriptorSetLayout;

        VkDescriptorSet descriptorSet;
        if (vkAllocateDescriptorSets(s_device, &allocInfo, &descriptorSet) != VK_SUCCESS)
        {
            CubismLogError("failed to allocate descriptor sets!");
        }

        _copyDescriptorSets[buffer].descriptorSet = descriptorSet;
    }
}

void CubismRenderer_Vulkan::CreateDepthBuffer()
{
    _depthImage.CreateImage(s_device, s_physicalDevice, s_renderExtent.width, s_renderExtent.height,
        1, s_depthFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    _depthImage.CreateView(s_device, s_depthFormat,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 1);
}

void CubismRenderer_Vulkan::InitializeRenderer()
{
    vkCmdSetCullModeEXT = reinterpret_cast<PFN_vkCmdSetCullMode>(vkGetDeviceProcAddr(s_device, "vkCmdSetCullModeEXT"));

    _updateFinishedSemaphores.Resize(s_bufferSetNum);
    for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkCreateSemaphore(s_device, &semaphoreInfo, nullptr, &_updateFinishedSemaphores[buffer]);
    }

    CreateCommandBuffer();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateDescriptorSets();
    CreateDepthBuffer();
    CubismPipeline_Vulkan::GetInstance()->CreatePipelines(_descriptorSetLayout, _copyDescriptorSetLayout);
}

void CubismRenderer_Vulkan::Initialize(CubismModel* model)
{
    Initialize(model, 1);
}

void CubismRenderer_Vulkan::Initialize(CubismModel* model, csmInt32 maskBufferCount)
{
    if (s_device == nullptr)
    {
        CubismLogError("Device has not been set.");
        CSM_ASSERT(0);
        return;
    }

    _sortedDrawableIndexList.Resize(model->GetDrawableCount(), 0);

    CubismRenderer::Initialize(model, maskBufferCount); // 親クラスの処理を呼ぶ

    // 1未満は1に補正する
    if (maskBufferCount < 1)
    {
        maskBufferCount = 1;
        CubismLogWarning(
            "The number of render textures must be an integer greater than or equal to 1. Set the number of render textures to 1.");
    }

    // オフスクリーン作成
    _modelRenderTargets.Clear();
    csmInt32 createSize = 1;
    for (csmInt32 i = 0; i < createSize; i++)
    {
        CubismRenderTarget_Vulkan modelRenderTarget;
        modelRenderTarget.CreateRenderTarget(s_device, s_physicalDevice,_modelRenderTargetWidth, _modelRenderTargetHeight, s_imageFormat, s_depthFormat);
        _modelRenderTargets.PushBack(modelRenderTarget);
    }

    if (model->IsUsingMasking())
    {
        //モデルがマスクを使用している時のみにする
        _drawableClippingManager = CSM_NEW CubismClippingManager_Vulkan();
        _drawableClippingManager->Initialize(
            *model,
            maskBufferCount,
            DrawableObjectType_Drawable
        );

        const csmInt32 bufferWidth = static_cast<csmInt32>(_drawableClippingManager->GetClippingMaskBufferSize().X);
        const csmInt32 bufferHeight = static_cast<csmInt32>(_drawableClippingManager->GetClippingMaskBufferSize().Y);

        _drawableMaskBuffers.Clear();

        _drawableMaskBuffers.Resize(s_bufferSetNum);
        for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
        {
            _drawableMaskBuffers[buffer].Resize(maskBufferCount);
            // バックバッファ分確保
            for (csmInt32 i = 0; i < maskBufferCount; i++)
            {
                _drawableMaskBuffers[buffer][i].CreateRenderTarget(s_device, s_physicalDevice, bufferWidth, bufferHeight,
                    s_imageFormat, s_depthFormat);
            }
        }
    }

    if (model->IsUsingMaskingForOffscreen())
    {
        _offscreenClippingManager = CSM_NEW CubismClippingManager_Vulkan();
        _offscreenClippingManager->Initialize(
            *model,
            maskBufferCount,
            DrawableObjectType_Offscreen
        );

        const csmInt32 bufferWidth = static_cast<csmInt32>(_offscreenClippingManager->GetClippingMaskBufferSize().X);
        const csmInt32 bufferHeight = static_cast<csmInt32>(_offscreenClippingManager->GetClippingMaskBufferSize().Y);

        _offscreenMaskBuffers.Clear();
        _offscreenMaskBuffers.Resize(s_bufferSetNum);
        for (csmUint32 buffer = 0; buffer < s_bufferSetNum; buffer++)
        {
            _offscreenMaskBuffers[buffer].Resize(maskBufferCount);
            // バックバッファ分確保
            for (csmInt32 i = 0; i < maskBufferCount; ++i)
            {
                _offscreenMaskBuffers[buffer][i].CreateRenderTarget(s_device, s_physicalDevice, bufferWidth, bufferHeight, s_imageFormat, s_depthFormat);
            }
        }
    }

    csmInt32 drawableCount = model->GetDrawableCount();
    csmInt32 offscreenCount = model->GetOffscreenCount();
    _sortedObjectsIndexList.Resize(drawableCount + offscreenCount, 0);
    _sortedObjectsTypeList.Resize(drawableCount + offscreenCount, DrawableObjectType_Drawable);

    // オフスクリーン数が0の場合は何もしない
    if (offscreenCount > 0)
    {
        _offscreenList = csmVector<CubismOffscreenRenderTarget_Vulkan>(offscreenCount);
        for (csmInt32 offscreenIndex = 0; offscreenIndex < offscreenCount; ++offscreenIndex)
        {
            CubismOffscreenRenderTarget_Vulkan renderTarget;
            renderTarget.SetOffscreenIndex(offscreenIndex);
            _offscreenList.PushBack(renderTarget);
        }

        // 全てのオフスクリーンを登録し終わってから行う
        SetupParentOffscreens(model, offscreenCount);
    }

    _commandBufferCurrent = 0;

    InitializeRenderer();
}

void CubismRenderer_Vulkan::SetupParentOffscreens(const CubismModel* model, csmInt32 offscreenCount)
{
    CubismOffscreenRenderTarget_Vulkan* parentOffscreen;
    for (csmInt32 offscreenIndex = 0; offscreenIndex < offscreenCount; ++offscreenIndex)
    {
        parentOffscreen = NULL;
        const csmInt32 ownerIndex = model->GetOffscreenOwnerIndices()[offscreenIndex];
        csmInt32 parentIndex = model->GetPartParentPartIndex(ownerIndex);

        // 親のオフスクリーンを探す
        while (parentIndex != CubismModel::CubismNoIndex_Parent)
        {
            for (csmInt32 i = 0; i < offscreenCount; ++i)
            {
                if (model->GetOffscreenOwnerIndices()[_offscreenList.At(i).GetOffscreenIndex()] != parentIndex)
                {
                    continue; // オフスクリーンのインデックスが親と一致しなければスキップ
                }

                parentOffscreen = &_offscreenList.At(i);
                break;
            }

            if (parentOffscreen != NULL)
            {
                break;
            }

            parentIndex = model->GetPartParentPartIndex(parentIndex);
        }

        // 親のオフスクリーンを設定
        _offscreenList.At(offscreenIndex).SetParentPartOffscreen(parentOffscreen);
    }
}

void CubismRenderer_Vulkan::CopyToBuffer(csmInt32 drawAssign, const csmInt32 vcount, const csmFloat32* varray,
                                         const csmFloat32* uvarray, VkCommandBuffer commandBuffer)
{
    csmVector<ModelVertex> vertices;

    for (csmInt32 ct = 0; ct < vcount * 2; ct += 2)
    {
        ModelVertex vertex;
        // モデルデータからのコピー
        vertex.pos.X = varray[ct + 0];
        vertex.pos.Y = varray[ct + 1];
        vertex.texCoord.X = uvarray[ct + 0];
        vertex.texCoord.Y = uvarray[ct + 1];
        vertices.PushBack(vertex);
    }
    csmUint32 bufferSize = sizeof(ModelVertex) * vertices.GetSize();
    _stagingBuffers[_commandBufferCurrent][drawAssign].MemCpy(vertices.GetPtr(), (size_t)bufferSize);

    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, _stagingBuffers[_commandBufferCurrent][drawAssign].GetBuffer(), _vertexBuffers[_commandBufferCurrent][drawAssign].GetBuffer(), 1,
                    &copyRegion);
}

void CubismRenderer_Vulkan::UpdateMatrix(csmFloat32 vkMat16[16], CubismMatrix44 cubismMat)
{
    for (int i = 0; i < 16; i++)
    {
        vkMat16[i] = cubismMat.GetArray()[i];
    }
}

void CubismRenderer_Vulkan::UpdateColor(csmFloat32 vkVec4[4], csmFloat32 r, csmFloat32 g, csmFloat32 b, csmFloat32 a)
{
    vkVec4[0] = r;
    vkVec4[1] = g;
    vkVec4[2] = b;
    vkVec4[3] = a;
}

void CubismRenderer_Vulkan::UpdateDescriptorSet(Descriptor& descriptor, csmUint32 textureIndex, csmBool isMasked)
{
    VkBuffer uniformBuffer = descriptor.uniformBuffer.GetBuffer();
    // descriptorが更新されていない最初の1回のみ行う
    VkDescriptorSet descriptorSet;
    if (!((isMasked && !descriptor.isDescriptorSetMaskedUpdated) || !descriptor.isDescriptorSetUpdated))
    {
        return;
    }

    if (isMasked)
    {
        descriptorSet = descriptor.descriptorSetMasked;
        descriptor.isDescriptorSetMaskedUpdated = true;
    }
    else
    {
        descriptorSet = descriptor.descriptorSet;
        descriptor.isDescriptorSetUpdated = true;
    }

    csmVector<VkWriteDescriptorSet> descriptorWrites{};

    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = uniformBuffer;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet ubo{};
    ubo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo.dstSet = descriptorSet;
    ubo.dstBinding = 0;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;
    ubo.pBufferInfo = &uniformBufferInfo;
    descriptorWrites.PushBack(ubo);

    //テクスチャ1はキャラクターのテクスチャ、テクスチャ2はマスク用のオフスクリーンに使用するテクスチャ、テクスチャ3はブレンド用のオフスクリーンに使用するテクスチャ
    VkDescriptorImageInfo imageInfo1{};
    VkDescriptorImageInfo imageInfo2{};
    VkDescriptorImageInfo imageInfo3{};

    imageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo1.imageView = _textures[textureIndex].GetView();
    imageInfo1.sampler = _textures[textureIndex].GetSampler();
    VkWriteDescriptorSet image1{};
    image1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    image1.dstSet = descriptorSet;
    image1.dstBinding = 1;
    image1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    image1.descriptorCount = 1;
    image1.pImageInfo = &imageInfo1;
    descriptorWrites.PushBack(image1);

    if (isMasked)
    {
        imageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo2.imageView = _drawableMaskBuffers[_commandBufferCurrent][GetClippingContextBufferForDrawable()->
            _bufferIndex].GetTextureView();
        imageInfo2.sampler = _drawableMaskBuffers[_commandBufferCurrent][GetClippingContextBufferForDrawable()->
            _bufferIndex].GetTextureSampler();
        VkWriteDescriptorSet image2{};
        image2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        image2.dstSet = descriptorSet;
        image2.dstBinding = 2;
        image2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        image2.descriptorCount = 1;
        image2.pImageInfo = &imageInfo2;
        descriptorWrites.PushBack(image2);
    }

    if (GetModel()->IsBlendModeEnabled())
    {
        CubismRenderTarget_Vulkan* blendRenderTarget = (_currentRenderTarget != NULL)
                                                          ? _currentRenderTarget
                                                          : GetModelRenderTarget();

        imageInfo3.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo3.imageView = blendRenderTarget->GetTextureView();
        imageInfo3.sampler = blendRenderTarget->GetTextureSampler();
        VkWriteDescriptorSet image3{};
        image3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        image3.dstSet = descriptorSet;
        image3.dstBinding = 3;
        image3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        image3.descriptorCount = 1;
        image3.pImageInfo = &imageInfo3;
        descriptorWrites.PushBack(image3);
    }

    vkUpdateDescriptorSets(s_device, descriptorWrites.GetSize(), descriptorWrites.GetPtr(), 0, NULL);
}

void CubismRenderer_Vulkan::UpdateDescriptorSetForMask(Descriptor& descriptor, csmUint32 textureIndex)
{
    VkBuffer uniformBuffer = descriptor.uniformBufferMask.GetBuffer();

    // descriptorが更新されていない最初の1回のみ行う
    if (descriptor.isDescriptorSetForMaskUpdated)
    {
        return;
    }

    descriptor.isDescriptorSetForMaskUpdated = true;

    VkDescriptorSet descriptorSet = descriptor.descriptorSetForMask;
    csmVector<VkWriteDescriptorSet> descriptorWrites{};

    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = uniformBuffer;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet ubo{};
    ubo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo.dstSet = descriptorSet;
    ubo.dstBinding = 0;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;
    ubo.pBufferInfo = &uniformBufferInfo;
    descriptorWrites.PushBack(ubo);

    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = _textures[textureIndex].GetView();
    imageInfo.sampler = _textures[textureIndex].GetSampler();
    VkWriteDescriptorSet image{};
    image.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    image.dstSet = descriptorSet;
    image.dstBinding = 1;
    image.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    image.descriptorCount = 1;
    image.pImageInfo = &imageInfo;
    descriptorWrites.PushBack(image);

    vkUpdateDescriptorSets(s_device, descriptorWrites.GetSize(), descriptorWrites.GetPtr(), 0, NULL);
}

void CubismRenderer_Vulkan::UpdateDescriptorSetForOffscreen(Descriptor& descriptor, CubismOffscreenRenderTarget_Vulkan* offscreen, csmBool isMasked)
{
    VkBuffer uniformBuffer = descriptor.uniformBuffer.GetBuffer();

    // descriptorが更新されていない最初の1回のみ行う
    VkDescriptorSet descriptorSet;
    if (!((isMasked && !descriptor.isDescriptorSetMaskedUpdated) || !descriptor.isDescriptorSetUpdated))
    {
        return;
    }

    if (isMasked)
    {
        descriptorSet = descriptor.descriptorSetMasked;
        descriptor.isDescriptorSetMaskedUpdated = true;
    }
    else
    {
        descriptorSet = descriptor.descriptorSet;
        descriptor.isDescriptorSetUpdated = true;
    }

    csmVector<VkWriteDescriptorSet> descriptorWrites{};

    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = uniformBuffer;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet ubo{};
    ubo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo.dstSet = descriptorSet;
    ubo.dstBinding = 0;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;
    ubo.pBufferInfo = &uniformBufferInfo;
    descriptorWrites.PushBack(ubo);

    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = offscreen->GetRenderTarget()->GetTextureView();
    imageInfo.sampler = offscreen->GetRenderTarget()->GetTextureSampler();
    VkWriteDescriptorSet image{};
    image.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    image.dstSet = descriptorSet;
    image.dstBinding = 1;
    image.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    image.descriptorCount = 1;
    image.pImageInfo = &imageInfo;
    descriptorWrites.PushBack(image);

    // マスク用
    VkDescriptorImageInfo maskImageInfo{};
    if (isMasked)
    {
        maskImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        maskImageInfo.imageView = _offscreenMaskBuffers[_commandBufferCurrent][GetClippingContextBufferForOffscreen()->_bufferIndex].GetTextureView();
        maskImageInfo.sampler = _offscreenMaskBuffers[_commandBufferCurrent][GetClippingContextBufferForOffscreen()->_bufferIndex].GetTextureSampler();
        VkWriteDescriptorSet maskImage{};
        maskImage.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        maskImage.dstSet = descriptorSet;
        maskImage.dstBinding = 2;
        maskImage.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        maskImage.descriptorCount = 1;
        maskImage.pImageInfo = &maskImageInfo;
        descriptorWrites.PushBack(maskImage);
    }

    // ブレンド用
    VkDescriptorImageInfo blendImageInfo{};
    if (GetModel()->IsBlendModeEnabled())
    {
        CubismRenderTarget_Vulkan* blendRenderTarget = (_currentRenderTarget != NULL)
                                                          ? _currentRenderTarget
                                                          : GetModelRenderTarget();

        blendImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        blendImageInfo.imageView = blendRenderTarget->GetTextureView();
        blendImageInfo.sampler = blendRenderTarget->GetTextureSampler();
        VkWriteDescriptorSet blendImage{};
        blendImage.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        blendImage.dstSet = descriptorSet;
        blendImage.dstBinding = 3;
        blendImage.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        blendImage.descriptorCount = 1;
        blendImage.pImageInfo = &blendImageInfo;
        descriptorWrites.PushBack(blendImage);
    }

    vkUpdateDescriptorSets(s_device, descriptorWrites.GetSize(), descriptorWrites.GetPtr(), 0, NULL);

}

void CubismRenderer_Vulkan::UpdateDescriptorSetForCopy(Descriptor& descriptor, const CubismRenderTarget_Vulkan* srcBuffer)
{
    // descriptorが更新されていない最初の1回のみ行う
    if (descriptor.isDescriptorSetUpdated)
    {
        return;
    }

    descriptor.isDescriptorSetUpdated = true;

    VkBuffer uniformBuffer = descriptor.uniformBuffer.GetBuffer();

    VkDescriptorSet descriptorSet = descriptor.descriptorSet;
    csmVector<VkWriteDescriptorSet> descriptorWrites{};

    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = uniformBuffer;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet ubo{};
    ubo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo.dstSet = descriptorSet;
    ubo.dstBinding = 0;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;
    ubo.pBufferInfo = &uniformBufferInfo;
    descriptorWrites.PushBack(ubo);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = srcBuffer->GetTextureView();
    imageInfo.sampler = srcBuffer->GetTextureSampler();
    VkWriteDescriptorSet image{};
    image.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    image.dstSet = descriptorSet;
    image.dstBinding = 1;
    image.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    image.descriptorCount = 1;
    image.pImageInfo = &imageInfo;
    descriptorWrites.PushBack(image);

    vkUpdateDescriptorSets(s_device, descriptorWrites.GetSize(), descriptorWrites.GetPtr(), 0, NULL);

    descriptor.isDescriptorSetUpdated = true;
}


void CubismRenderer_Vulkan::ExecuteDrawForDrawable(const CubismModel& model, const csmInt32 index, VkCommandBuffer& cmdBuffer)
{
    // パイプラインレイアウト設定用のインデックスを取得
    csmUint32 blendIndex = 0;
    csmUint32 shaderIndex = 0;
    const csmBool masked = GetClippingContextBufferForDrawable() != NULL; // この描画オブジェクトはマスク対象か
    const csmBool invertedMask = model.GetDrawableInvertedMask(index);
    const csmBool isPremultipliedAlpha = IsPremultipliedAlpha();
    const csmInt32 offset = (masked ? (invertedMask ? 2 : 1) : 0) + (isPremultipliedAlpha ? 3 : 0);
    csmBool isBlendMode = false;
    const csmInt32 shaderNameBegin = GetShaderNamesBegin(model.GetDrawableBlendModeType(index));

    switch (shaderNameBegin)
    {
    default:
        // 5.3以降
        shaderIndex = shaderNameBegin + offset;
        blendIndex = 0;
        isBlendMode = true;
        break;
     case ShaderNames_Normal:
         shaderIndex = ShaderNames_Normal + offset;
         blendIndex = CompatibleBlend::Blend_Normal;
         break;
    case ShaderNames_Add:
        shaderIndex = ShaderNames_Add + offset;
        blendIndex = CompatibleBlend::Blend_Add;
        break;
    case ShaderNames_Mult:
        shaderIndex = ShaderNames_Mult + offset;
        blendIndex = CompatibleBlend::Blend_Mult;
        break;
    }

    VkPipelineLayout pipelineLayout = CubismPipeline_Vulkan::GetInstance()->GetPipelineLayout(shaderIndex, blendIndex);
    if (pipelineLayout == NULL)
    {
        return;
    }

    VkPipeline pipeline = CubismPipeline_Vulkan::GetInstance()->GetPipeline(shaderIndex, blendIndex);
    if (pipeline == NULL)
    {
        return;
    }


    Descriptor &descriptor = _descriptorSets[_commandBufferCurrent][index];
    ModelUBO ubo;
    if (masked)
    {
        // クリッピング用行列の設定
        UpdateMatrix(ubo.clipMatrix, GetClippingContextBufferForDrawable()->_matrixForDraw); // テクスチャ座標の変換に使用するのでy軸の向きは反転しない

        // カラーチャンネルの設定
        SetColorChannel(ubo, GetClippingContextBufferForDrawable());
    }

    // MVP行列の設定
    UpdateMatrix(ubo.projectionMatrix, GetMvpMatrix());

    // 色定数バッファの設定
    CubismTextureColor baseColor;
    if (model.IsBlendModeEnabled())
    {
        // ブレンドモードではモデルカラーは最後に処理するため不透明度のみ対応させる
        csmFloat32 drawableOpacity = model.GetDrawableOpacity(index);
        baseColor.A = drawableOpacity;
        if (isPremultipliedAlpha)
        {
            baseColor.R = drawableOpacity;
            baseColor.G = drawableOpacity;
            baseColor.B = drawableOpacity;
        }
    }
    else
    {
        // ブレンドモード使用しない場合はDrawable単位でモデルカラーを処理する
        baseColor = GetModelColorWithOpacity(model.GetDrawableOpacity(index));
    }
    CubismTextureColor multiplyColor = model.GetMultiplyColor(index);
    CubismTextureColor screenColor = model.GetScreenColor(index);
    SetColorUniformBuffer(ubo, baseColor, multiplyColor, screenColor);

    // ディスクリプタにユニフォームバッファをコピー
    descriptor.uniformBuffer.MemCpy(&ubo, sizeof(ModelUBO));

    // 頂点バッファの設定
    BindVertexAndIndexBuffers(index, cmdBuffer, DrawableObjectType_Drawable);

    // テクスチャインデックス取得
    csmInt32 textureIndex = model.GetDrawableTextureIndex(index);

    // ディスクリプタセットのバインド
    UpdateDescriptorSet(descriptor, textureIndex, masked);
    VkDescriptorSet* descriptorSet = (masked ? &_descriptorSets[_commandBufferCurrent][index].descriptorSetMasked
                                             : &_descriptorSets[_commandBufferCurrent][index].descriptorSet);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 0, 1,
                                descriptorSet, 0, nullptr);

    // パイプラインのバインド
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);

    if (isBlendMode)
    {
        SetBlendTextureBarrier(cmdBuffer);
    }

    // 描画
    vkCmdDrawIndexed(cmdBuffer, model.GetDrawableVertexIndexCount(index), 1, 0, 0, 0);
}

void CubismRenderer_Vulkan::ExecuteDrawForMask(const CubismModel& model, const csmInt32 index, VkCommandBuffer& cmdBuffer)
{
    csmUint32 shaderIndex = ShaderNames_SetupMask;
    csmUint32 blendIndex = Blend_Mask;

    VkPipelineLayout pipelineLayout = CubismPipeline_Vulkan::GetInstance()->GetPipelineLayout(shaderIndex, blendIndex);
    if (pipelineLayout == NULL)
    {
        return;
    }

    VkPipeline pipeline = CubismPipeline_Vulkan::GetInstance()->GetPipeline(shaderIndex, blendIndex);
    if (pipeline == NULL)
    {
        return;
    }

    Descriptor &descriptor = _descriptorSets[_commandBufferCurrent][index];
    ModelUBO ubo;

    // クリッピング用行列の設定
    UpdateMatrix(ubo.clipMatrix, GetClippingContextBufferForMask()->_matrixForMask);

    // カラーチャンネルの設定
    SetColorChannel(ubo, GetClippingContextBufferForMask());

    // MVP行列の設定
    UpdateMatrix(ubo.projectionMatrix, GetMvpMatrix());

    // 色定数バッファの設定
    csmRectF *rect = GetClippingContextBufferForMask()->_layoutBounds;
    CubismTextureColor baseColor = {rect->X * 2.0f - 1.0f, rect->Y * 2.0f - 1.0f, rect->GetRight() * 2.0f - 1.0f, rect->GetBottom() * 2.0f - 1.0f};
    CubismTextureColor multiplyColor = model.GetMultiplyColor(index);
    CubismTextureColor screenColor = model.GetScreenColor(index);
    SetColorUniformBuffer(ubo, baseColor, multiplyColor, screenColor);

    // マスク生成時用ユニフォームバッファにコピー
    descriptor.uniformBufferMask.MemCpy(&ubo, sizeof(ModelUBO));

    // 頂点バッファの設定
    BindVertexAndIndexBuffers(index, cmdBuffer, DrawableObjectType_Drawable);

    // テクスチャインデックス取得
    csmInt32 textureIndex = model.GetDrawableTextureIndex(index);

    // マスク生成時用ディスクリプタセットの更新とバインド
    UpdateDescriptorSetForMask(descriptor, textureIndex);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1,
                            &_descriptorSets[_commandBufferCurrent][index].descriptorSetForMask, 0, nullptr);

    // パイプラインのバインド
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);

    // 描画
    vkCmdDrawIndexed(cmdBuffer, model.GetDrawableVertexIndexCount(index), 1, 0, 0, 0);
}

void CubismRenderer_Vulkan::ExecuteDrawForOffscreen(const CubismModel& model, CubismOffscreenRenderTarget_Vulkan* offscreen, VkCommandBuffer& cmdBuffer)
{
    // パイプラインレイアウト設定用のインデックスを取得
    csmUint32 blendIndex = 0;
    csmUint32 shaderIndex = 0;
    csmInt32 offscreenIndex = offscreen->GetOffscreenIndex();

    const csmBool masked = GetClippingContextBufferForOffscreen() != NULL; // この描画オブジェクトはマスク対象か
    const csmBool invertedMask = model.GetOffscreenInvertedMask(offscreenIndex);
    const csmInt32 usePremultipliedAlpha = 3; // オフスクリーンはPremultipliedAlphaを使用
    const csmInt32 offset = (masked ? (invertedMask ? 2 : 1) : 0) + usePremultipliedAlpha;
    csmBool isBlendMode = false;
    const csmInt32 shaderNameBegin = GetShaderNamesBegin(model.GetOffscreenBlendModeType(offscreenIndex));

    switch (shaderNameBegin)
    {
    default:
        // 5.3以降
        shaderIndex = shaderNameBegin + offset;
        blendIndex = 0;
        isBlendMode = true;
        break;
     case ShaderNames_Normal:
         shaderIndex = ShaderNames_Normal + offset;
         blendIndex = CompatibleBlend::Blend_Normal;
         break;
    case ShaderNames_Add:
        shaderIndex = ShaderNames_Add + offset;
        blendIndex = CompatibleBlend::Blend_Add;
        break;
    case ShaderNames_Mult:
        shaderIndex = ShaderNames_Mult + offset;
        blendIndex = CompatibleBlend::Blend_Mult;
        break;
    }

    VkPipelineLayout pipelineLayout = CubismPipeline_Vulkan::GetInstance()->GetPipelineLayout(shaderIndex, blendIndex);
    if (pipelineLayout == NULL)
    {
        return;
    }

    VkPipeline pipeline = CubismPipeline_Vulkan::GetInstance()->GetPipeline(shaderIndex, blendIndex);
    if (pipeline == NULL)
    {
        return;
    }

    Descriptor &descriptor = _offscreenDescriptorSets[_commandBufferCurrent][offscreenIndex];
    ModelUBO ubo;
    if (masked)
    {
        // クリッピング用行列の設定
        UpdateMatrix(ubo.clipMatrix, GetClippingContextBufferForOffscreen()->_matrixForDraw); // テクスチャ座標の変換に使用するのでy軸の向きは反転しない

        // カラーチャンネルの設定
        SetColorChannel(ubo, GetClippingContextBufferForOffscreen());
    }

    // MVP行列の設定
    CubismMatrix44 mvpMatrix;
    mvpMatrix.LoadIdentity();
    UpdateMatrix(ubo.projectionMatrix, mvpMatrix);

    // 色定数バッファの設定
    // オフスクリーンはPreMultipliedAlpha
    csmFloat32 offscreenOpacity = model.GetOffscreenOpacity(offscreenIndex);
    CubismTextureColor baseColor = CubismTextureColor(offscreenOpacity, offscreenOpacity, offscreenOpacity, offscreenOpacity);
    CubismTextureColor multiplyColor = model.GetMultiplyColorOffscreen(offscreenIndex);
    CubismTextureColor screenColor = model.GetScreenColorOffscreen(offscreenIndex);
    SetColorUniformBuffer(ubo, baseColor, multiplyColor, screenColor);

    // ディスクリプタにユニフォームバッファをコピー
    descriptor.uniformBuffer.MemCpy(&ubo, sizeof(ModelUBO));

    // 頂点バッファとインデックスバッファのバインド
    VkBuffer vertexBuffers[] = {_offscreenVertexBuffers[_commandBufferCurrent][offscreenIndex].GetBuffer()};
    VkDeviceSize offsets[] = {0};
    csmUint32 bufferSize = sizeof(modelRenderTargetVertexArray);
    _offscreenStagingBuffers[_commandBufferCurrent][offscreenIndex].MemCpy(modelRenderTargetVertexArray, (size_t)bufferSize);
    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(_updateCommandBuffers[_commandBufferCurrent], _offscreenStagingBuffers[_commandBufferCurrent][offscreenIndex].GetBuffer(), _offscreenVertexBuffers[_commandBufferCurrent][offscreenIndex].GetBuffer(), 1,
                    &copyRegion);
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmdBuffer, _offscreenIndexBuffers[_commandBufferCurrent][offscreenIndex].GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

    // ディスクリプタセットのバインド
    UpdateDescriptorSetForOffscreen(descriptor, offscreen, masked);
    VkDescriptorSet* descriptorSet = (masked ? &_offscreenDescriptorSets[_commandBufferCurrent][offscreenIndex].descriptorSetMasked
                                             : &_offscreenDescriptorSets[_commandBufferCurrent][offscreenIndex].descriptorSet);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 0, 1,
                                descriptorSet, 0, nullptr);

    // パイプラインのバインド
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);

    if (isBlendMode)
    {
        SetBlendTextureBarrier(cmdBuffer);
    }

    // 描画
    vkCmdDrawIndexed(cmdBuffer, sizeof(modelRenderTargetIndexArray) / sizeof(csmUint16), 1, 0, 0, 0);
}

void CubismRenderer_Vulkan::ExecuteDrawForRenderTarget(const CubismRenderTarget_Vulkan* srcBuffer, VkCommandBuffer& cmdBuffer)
{
    Descriptor &descriptor = _copyDescriptorSets[_commandBufferCurrent];

    csmFloat32 uboBaseColor[4];
    CubismTextureColor baseColor = GetModelColor();
    baseColor.R *= baseColor.A;
    baseColor.G *= baseColor.A;
    baseColor.B *= baseColor.A;
    UpdateColor(uboBaseColor, baseColor.R, baseColor.G, baseColor.B, baseColor.A);
    descriptor.uniformBuffer.MemCpy(&uboBaseColor, sizeof(csmFloat32) * 4);

    // ディスクリプタセット更新
    UpdateDescriptorSetForCopy(descriptor, srcBuffer);

    // ディスクリプタセットバインド
    VkDescriptorSet* descriptorSet = &_copyDescriptorSets[_commandBufferCurrent].descriptorSet;
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            CubismPipeline_Vulkan::GetInstance()->GetPipelineLayout(ShaderNames_Copy, 0),
                            0, 1, descriptorSet, 0, nullptr);

    // パイプラインバインド
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        CubismPipeline_Vulkan::GetInstance()->GetPipeline(ShaderNames_Copy, 0));

    // 裏面描画の有効
    vkCmdSetCullModeEXT(cmdBuffer, VK_CULL_MODE_NONE);

    // 頂点バッファとインデックスバッファのバインド
    VkBuffer vertexBuffers[] = {_copyVertexBuffer[_commandBufferCurrent].GetBuffer()};
    VkDeviceSize offsets[] = {0};
    csmUint32 bufferSize = sizeof(modelRenderTargetVertexArray);
    _copyStagingBuffer[_commandBufferCurrent].MemCpy(modelRenderTargetVertexArray, (size_t)bufferSize);
    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(_updateCommandBuffers[_commandBufferCurrent], _copyStagingBuffer[_commandBufferCurrent].GetBuffer(), _copyVertexBuffer[_commandBufferCurrent].GetBuffer(), 1,
                    &copyRegion);
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmdBuffer, _copyIndexBuffer[_commandBufferCurrent].GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

    // ビューポート
    VkViewport viewport = GetViewport(static_cast<csmFloat32>(s_renderExtent.width), static_cast<csmFloat32>(s_renderExtent.height), 0.0, 1.0);
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    // シザー
    VkRect2D rect = GetScissor(0.0, 0.0, static_cast<csmFloat32>(s_renderExtent.width), static_cast<csmFloat32>(s_renderExtent.height));
    vkCmdSetScissor(cmdBuffer, 0, 1, &rect);

    // 描画
    vkCmdDrawIndexed(cmdBuffer, sizeof(modelRenderTargetIndexArray) / sizeof(csmUint16), 1, 0, 0, 0);
}

void CubismRenderer_Vulkan::DrawMeshVulkan(const CubismModel& model, const csmInt32 index,
                                           VkCommandBuffer commandBuffer, VkCommandBuffer updateCommandBuffer)
{
    if (s_device == VK_NULL_HANDLE)
    {
        // デバイス未設定
        return;
    }
    if (model.GetDrawableVertexIndexCount(index) == 0)
    {
        // 描画物無し
        return;
    }
    if (model.GetDrawableOpacity(index) <= 0.0f && GetClippingContextBufferForMask() == NULL)
    {
        // 描画不要なら描画処理をスキップする
        return;
    }

    csmInt32 textureIndex = model.GetDrawableTextureIndices(index);
    if (_textures[textureIndex].GetSampler() == VK_NULL_HANDLE || _textures[textureIndex].GetView() == VK_NULL_HANDLE)
    {
        return;
    }

    // 裏面描画の有効・無効
    if (IsCulling())
    {
        vkCmdSetCullModeEXT(commandBuffer, VK_CULL_MODE_BACK_BIT);
    }
    else
    {
        vkCmdSetCullModeEXT(commandBuffer, VK_CULL_MODE_NONE);
    }

    // 頂点バッファにコピー
    CopyToBuffer(index, model.GetDrawableVertexCount(index),
                 const_cast<csmFloat32*>(model.GetDrawableVertices(index)),
                 reinterpret_cast<csmFloat32*>(const_cast<Core::csmVector2*>(model.GetDrawableVertexUvs(index))),
                 _updateCommandBuffers[_commandBufferCurrent]);

    if (GetClippingContextBufferForMask() != NULL) // マスク生成時
    {
        ExecuteDrawForMask(model, index, commandBuffer);
    }
    else
    {
        ExecuteDrawForDrawable(model, index, commandBuffer);
    }

    SetClippingContextBufferForDraw(NULL);
    SetClippingContextBufferForMask(NULL);
}

void CubismRenderer_Vulkan::BeginRendering(VkCommandBuffer drawCommandBuffer, csmBool isResume)
{
    VkClearValue clearValue[2];
    clearValue[0].color = _clearColor.color;
    clearValue[1].depthStencil = {1.0, 0};

    VkImageMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_NONE;
    memoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    memoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    memoryBarrier.image = s_renderImage;
    memoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(drawCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

    VkRenderingAttachmentInfoKHR colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttachment.imageView = s_imageView;
    colorAttachment.loadOp = ((s_useRenderTarget && !isResume)? VK_ATTACHMENT_LOAD_OP_CLEAR
                                                              : VK_ATTACHMENT_LOAD_OP_LOAD);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = *clearValue;

    VkRenderingAttachmentInfoKHR depthStencilAttachment{};
    depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    depthStencilAttachment.imageView = _depthImage.GetView();
    depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthStencilAttachment.clearValue = {1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, {s_renderExtent}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthStencilAttachment;

    vkCmdBeginRendering(drawCommandBuffer, &renderingInfo);
}

void CubismRenderer_Vulkan::EndRendering(VkCommandBuffer drawCommandBuffer)
{
    vkCmdEndRendering(drawCommandBuffer);

    // レイアウト変更
    VkImageMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    memoryBarrier.dstAccessMask = (s_useRenderTarget ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_NONE);
    memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    memoryBarrier.newLayout = (s_useRenderTarget ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL);
    memoryBarrier.image = s_renderImage;
    memoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(drawCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            (s_useRenderTarget ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT), 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
}

void CubismRenderer_Vulkan::DoDrawModel()
{
    VkCommandBuffer updateCommandBuffer = _updateCommandBuffers[_commandBufferCurrent];
    VkCommandBuffer drawCommandBuffer = _drawCommandBuffers[_commandBufferCurrent];

    _isClearedModelRenderTarget  = false;

    //------------ クリッピングマスク・バッファ前処理方式の場合 ------------
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(updateCommandBuffer, &beginInfo);
    vkBeginCommandBuffer(drawCommandBuffer, &beginInfo);

    if (_drawableClippingManager != NULL)
    {
        // サイズが違う場合はここで作成しなおし
        for (csmInt32 i = 0; i < _drawableClippingManager->GetRenderTextureCount(); ++i)
        {
            if (_drawableMaskBuffers[_commandBufferCurrent][i].GetBufferWidth() != static_cast<csmUint32>(
                _drawableClippingManager->
                GetClippingMaskBufferSize().X) ||
                _drawableMaskBuffers[_commandBufferCurrent][i].GetBufferHeight() != static_cast<csmUint32>(
                    _drawableClippingManager->
                    GetClippingMaskBufferSize().Y))
            {
                _drawableMaskBuffers[_commandBufferCurrent][i].CreateRenderTarget(
                    s_device, s_physicalDevice,
                    static_cast<csmUint32>(_drawableClippingManager->GetClippingMaskBufferSize().X),
                    static_cast<csmUint32>(_drawableClippingManager->GetClippingMaskBufferSize().Y),
                    s_imageFormat, s_depthFormat
                );
            }
        }
        if (IsUsingHighPrecisionMask())
        {
            _drawableClippingManager->SetupMatrixForHighPrecision(*GetModel(), false, DrawableObjectType_Drawable);
        }
        else
        {
            _drawableClippingManager->SetupClippingContext(*GetModel(), drawCommandBuffer, updateCommandBuffer, this, _commandBufferCurrent, DrawableObjectType_Drawable);
        }
    }

    // オフスクリーン用クリッピングマネージャーの処理
    if (_offscreenClippingManager != NULL)
    {
        // サイズが違う場合はここで作成しなおし
        for (csmInt32 i = 0; i < _offscreenClippingManager->GetRenderTextureCount(); ++i)
        {
            if (_offscreenMaskBuffers[_commandBufferCurrent][i].GetBufferWidth() != static_cast<csmUint32>(
                _offscreenClippingManager->GetClippingMaskBufferSize().X) ||
                _offscreenMaskBuffers[_commandBufferCurrent][i].GetBufferHeight() != static_cast<csmUint32>(
                    _offscreenClippingManager->GetClippingMaskBufferSize().Y))
            {
                _offscreenMaskBuffers[_commandBufferCurrent][i].CreateRenderTarget(
                    s_device, s_physicalDevice,
                    static_cast<csmUint32>(_offscreenClippingManager->GetClippingMaskBufferSize().X),
                    static_cast<csmUint32>(_offscreenClippingManager->GetClippingMaskBufferSize().Y),
                    s_imageFormat, s_depthFormat
                );
            }
        }
        if (IsUsingHighPrecisionMask())
        {
            _offscreenClippingManager->SetupMatrixForHighPrecision(*GetModel(), false, DrawableObjectType_Offscreen, GetMvpMatrix());
        }
        else
        {
            _offscreenClippingManager->SetupClippingContext(*GetModel(), drawCommandBuffer, updateCommandBuffer, this, _commandBufferCurrent, DrawableObjectType_Offscreen);
        }
    }

    SubmitCommand(updateCommandBuffer, _updateFinishedSemaphores[_commandBufferCurrent]);
    SubmitCommand(drawCommandBuffer, VK_NULL_HANDLE, _updateFinishedSemaphores[_commandBufferCurrent]);

    DrawObjectLoop(updateCommandBuffer, drawCommandBuffer, beginInfo);
}

void CubismRenderer_Vulkan::PostDraw()
{
    _commandBufferCurrent++;
    if (s_bufferSetNum <= _commandBufferCurrent)
    {
        _commandBufferCurrent = 0;
    }
}

void CubismRenderer_Vulkan::BeforeDrawModelRenderTarget()
{
    if (!GetModel()->IsBlendModeEnabled())
    {
        return;
    }

    VkCommandBuffer drawCommandBuffer = _drawCommandBuffers[_commandBufferCurrent];

    // オフスクリーンのバッファのサイズが違う場合は作り直し
    csmBool renderTargetRecreated = false;
    for (csmInt32 i = 0; i < _modelRenderTargets.GetSize(); ++i)
    {
        if (_modelRenderTargets[i].GetBufferWidth() != _modelRenderTargetWidth || _modelRenderTargets[i].GetBufferHeight() != _modelRenderTargetHeight)
        {
            _modelRenderTargets[i].DestroyRenderTarget();
            _modelRenderTargets[i].CreateRenderTarget(s_device, s_physicalDevice,_modelRenderTargetWidth, _modelRenderTargetHeight, s_imageFormat, s_depthFormat);
            renderTargetRecreated = true;
        }
    }

    // オフスクリーンが作成し直しの場合ブレンドテクスチャを参照する全てのディスクリプタセットの更新フラグをリセット
    if (renderTargetRecreated)
    {
        for (csmUint32 buffer = 0; buffer < s_bufferSetNum; ++buffer)
        {
            for (csmInt32 i = 0; i < _descriptorSets[buffer].GetSize(); ++i)
            {
                // ブレンドモードが有効な場合のみリセット(ブレンドテクスチャを参照するディスクリプタのみ)
                if (GetModel()->IsBlendModeEnabled())
                {
                    _descriptorSets[buffer][i].isDescriptorSetUpdated = false;
                    _descriptorSets[buffer][i].isDescriptorSetMaskedUpdated = false;
                    _descriptorSets[buffer][i].isDescriptorSetForMaskUpdated = false;
                }
            }

            for (csmInt32 i = 0; i < _offscreenDescriptorSets[buffer].GetSize(); ++i)
            {
                if (GetModel()->IsBlendModeEnabled())
                {
                    _offscreenDescriptorSets[buffer][i].isDescriptorSetUpdated = false;
                    _offscreenDescriptorSets[buffer][i].isDescriptorSetMaskedUpdated = false;
                }
            }

            _copyDescriptorSets[buffer].isDescriptorSetUpdated = false;
        }
    }

    // 別バッファに描画開始
    _modelRenderTargets[0].BeginDraw(drawCommandBuffer, 0.0f, 0.0f, 0.0f, 0.0f, !_isClearedModelRenderTarget);
    _currentRenderTarget = &_modelRenderTargets[0];
    _isClearedModelRenderTarget = true;
}

void CubismRenderer_Vulkan::AfterDrawModelRenderTarget()
{
    if (!GetModel()->IsBlendModeEnabled())
    {
        return;
    }

    VkCommandBuffer drawCommandBuffer = _drawCommandBuffers[_commandBufferCurrent];

    // 元のバッファに描画する
    _modelRenderTargets[0].EndDraw(drawCommandBuffer);

    if (_currentOffscreen != NULL)
    {
        CopyRenderTarget(_currentOffscreen->GetRenderTarget(), drawCommandBuffer);
    }
    else
    {
        CopyRenderTarget(&_modelRenderTargets[0], drawCommandBuffer);
    }

}

void CubismRenderer_Vulkan::BeginRenderTarget(VkCommandBuffer drawCommandBuffer, csmBool isResume)
{
    if (GetModel()->IsBlendModeEnabled())
    {
        BeforeDrawModelRenderTarget();
    }
    else
    {
        BeginRendering(drawCommandBuffer, isResume);
    }
}

void CubismRenderer_Vulkan::EndRenderTarget(VkCommandBuffer drawCommandBuffer)
{
    if (GetModel()->IsBlendModeEnabled())
    {
        AfterDrawModelRenderTarget();
    }
    else
    {
        EndRendering(drawCommandBuffer);
    }
}

void CubismRenderer_Vulkan::CopyRenderTarget(const CubismRenderTarget_Vulkan* src, VkCommandBuffer drawCommandBuffer)
{
    // 描画開始
    BeginRendering(drawCommandBuffer, false);

    ExecuteDrawForRenderTarget(src, drawCommandBuffer);

    // 描画終了
    _modelRenderTargets[0].EndDraw(drawCommandBuffer);
}

void CubismRenderer_Vulkan::SetClippingContextBufferForMask(CubismClippingContext_Vulkan* clip)
{
    _clippingContextBufferForMask = clip;
}

CubismClippingContext_Vulkan* CubismRenderer_Vulkan::GetClippingContextBufferForMask() const
{
    return _clippingContextBufferForMask;
}

void CubismRenderer_Vulkan::SetClippingContextBufferForDraw(CubismClippingContext_Vulkan* clip)
{
    _clippingContextBufferForDraw = clip;
}

CubismClippingContext_Vulkan* CubismRenderer_Vulkan::GetClippingContextBufferForDrawable() const
{
    return _clippingContextBufferForDraw;
}

void CubismRenderer_Vulkan::BindTexture(CubismImageVulkan& image)
{
    _textures.PushBack(image);
}

void CubismRenderer_Vulkan::SetClippingMaskBufferSize(csmFloat32 width, csmFloat32 height)
{
    // インスタンス破棄前にレンダーテクスチャの数を保存
    const csmInt32 renderTextureCount = _drawableClippingManager->GetRenderTextureCount();

    //FrameBufferのサイズを変更するためにインスタンスを破棄・再作成する
    CSM_DELETE_SELF(CubismClippingManager_Vulkan, _drawableClippingManager);

    _drawableClippingManager = CSM_NEW CubismClippingManager_Vulkan();

    _drawableClippingManager->SetClippingMaskBufferSize(width, height);

    _drawableClippingManager->Initialize(
        *GetModel(),
        renderTextureCount,
        DrawableObjectType_Drawable
    );
}

CubismVector2 CubismRenderer_Vulkan::GetClippingMaskBufferSize() const
{
    return _drawableClippingManager->GetClippingMaskBufferSize();
}

CubismRenderTarget_Vulkan* CubismRenderer_Vulkan::GetModelRenderTarget()
{
    return &_modelRenderTargets[0];
}

CubismRenderTarget_Vulkan* CubismRenderer_Vulkan::GetDrawableMaskBuffer(csmUint32 backbufferNum, csmInt32 offscreenIndex)
{
    return &_drawableMaskBuffers[backbufferNum][offscreenIndex];
}

void CubismRenderer_Vulkan::SetColorUniformBuffer(ModelUBO& ubo, const CubismTextureColor& baseColor,
                                                  const CubismTextureColor& multiplyColor, const CubismTextureColor& screenColor)
{
    UpdateColor(ubo.baseColor, baseColor.R, baseColor.G, baseColor.B, baseColor.A);
    UpdateColor(ubo.multiplyColor, multiplyColor.R, multiplyColor.G, multiplyColor.B, multiplyColor.A);
    UpdateColor(ubo.screenColor, screenColor.R, screenColor.G, screenColor.B, screenColor.A);
}

void CubismRenderer_Vulkan::BindVertexAndIndexBuffers(const csmInt32 index, VkCommandBuffer& cmdBuffer, DrawableObjectType drawableObjectType)
{
    csmVector<csmVector<CubismBufferVulkan>> *targetVertexBuffers;
    csmVector<csmVector<CubismBufferVulkan>> *targetIndexBuffers;
    switch (drawableObjectType)
    {
    case CubismRenderer::DrawableObjectType_Drawable:
        targetVertexBuffers = &_vertexBuffers;
        targetIndexBuffers = &_indexBuffers;
        break;
    case CubismRenderer::DrawableObjectType_Offscreen:
        targetVertexBuffers = &_offscreenVertexBuffers;
        targetIndexBuffers = &_offscreenIndexBuffers;
        break;
    default:
        return;
    }

    VkBuffer vertexBuffers[] = {(*targetVertexBuffers)[_commandBufferCurrent][index].GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmdBuffer, (*targetIndexBuffers)[_commandBufferCurrent][index].GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
}

void CubismRenderer_Vulkan::SetColorChannel(ModelUBO& ubo, CubismClippingContext_Vulkan* contextBuffer)
{
    const csmInt32 channelIndex = contextBuffer->_layoutChannelIndex;
    CubismTextureColor *colorChannel = contextBuffer->GetClippingManager()->GetChannelFlagAsColor(channelIndex);
    UpdateColor(ubo.channelFlag, colorChannel->R, colorChannel->G, colorChannel->B, colorChannel->A);
}

void CubismRenderer_Vulkan::SetClippingContextBufferForOffscreen(CubismClippingContext_Vulkan* clip)
{
    _clippingContextBufferForOffscreen = clip;
}

CubismClippingContext_Vulkan* CubismRenderer_Vulkan::GetClippingContextBufferForOffscreen() const
{
    return _clippingContextBufferForOffscreen;
}

CubismRenderTarget_Vulkan* CubismRenderer_Vulkan::GetOffscreenMaskBuffer(csmUint32 backBufferNum, csmInt32 offscreenIndex)
{
    return &_offscreenMaskBuffers[backBufferNum][offscreenIndex];
}

void CubismRenderer_Vulkan::DrawObjectLoop(VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo)
{

    // スワップチェーンを再作成した際に深度バッファのサイズを更新する
    if (_depthImage.GetWidth() != s_renderExtent.width || _depthImage.GetHeight() != s_renderExtent.height)
    {
        _depthImage.Destroy(s_device);
        CreateDepthBuffer();
    }

    const csmInt32 drawableCount = GetModel()->GetDrawableCount();
    const csmInt32 offscreenCount = GetModel()->GetOffscreenCount();
    const csmInt32 totalCount = drawableCount + offscreenCount;
    const csmInt32* renderOrder = GetModel()->GetRenderOrders();

    _currentOffscreen = NULL;
    _currentRenderTarget = NULL;

    // インデックスを描画順でソート
    for (csmInt32 i = 0; i < totalCount; ++i)
    {
        const csmInt32 order = renderOrder[i];

        if (i < drawableCount)
        {
            _sortedObjectsIndexList[order] = i;
            _sortedObjectsTypeList[order] = DrawableObjectType_Drawable;
        }
        else if (i < totalCount)
        {
            _sortedObjectsIndexList[order] = i - drawableCount;
            _sortedObjectsTypeList[order] = DrawableObjectType_Offscreen;
        }
    }

    //描画
    vkBeginCommandBuffer(updateCommandBuffer, &beginInfo);
    vkBeginCommandBuffer(drawCommandBuffer, &beginInfo);
    BeginRenderTarget(drawCommandBuffer, false);

    for (csmInt32 i = 0; i < totalCount; ++i)
    {
        const csmInt32 objectIndex = _sortedObjectsIndexList[i];
        const csmInt32 objectType = _sortedObjectsTypeList[i];

        RenderObject(objectIndex, objectType, updateCommandBuffer, drawCommandBuffer, beginInfo);
    }

    while (_currentOffscreen != NULL)
    {
        // オフスクリーンが残っている場合は親オフスクリーンへの伝搬を行う
        SubmitDrawToParentOffscreen(_currentOffscreen->GetOffscreenIndex(), DrawableObjectType_Offscreen,
                                    updateCommandBuffer, drawCommandBuffer, beginInfo);
    }

    EndRenderTarget(drawCommandBuffer);

    SubmitCommand(updateCommandBuffer, _updateFinishedSemaphores[_commandBufferCurrent]);
    SubmitCommand(drawCommandBuffer, VK_NULL_HANDLE, _updateFinishedSemaphores[_commandBufferCurrent]);

    PostDraw();
}

void CubismRenderer_Vulkan::RenderObject(csmInt32 objectIndex, csmInt32 objectType,
                                         VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo)
{
    switch (objectType)
    {
    case DrawableObjectType_Drawable:
        // Drawable
        DrawDrawable(objectIndex, updateCommandBuffer, drawCommandBuffer, beginInfo);
        break;
    case DrawableObjectType_Offscreen:
        // Offscreen
        AddOffscreen(objectIndex, updateCommandBuffer, drawCommandBuffer, beginInfo);
        break;
    default:
        // 不明なタイプはエラーログを出す
        CubismLogError("Unknown drawable type: %d", objectType);
        break;
    }
}

void CubismRenderer_Vulkan::DrawDrawable(csmInt32 drawableIndex,
                                         VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo)
{
    // Drawableが表示状態でなければ処理をパスする
    if (!GetModel()->GetDrawableDynamicFlagIsVisible(drawableIndex))
    {
        return;
    }

    SubmitDrawToParentOffscreen(drawableIndex, DrawableObjectType_Drawable, updateCommandBuffer, drawCommandBuffer, beginInfo);

    // クリッピングマスク
    CubismClippingContext_Vulkan* clipContext = (_drawableClippingManager != NULL)
        ? (*_drawableClippingManager->GetClippingContextListForDraw())[drawableIndex]
        : NULL;

    if (clipContext != NULL && IsUsingHighPrecisionMask()) // マスクを書く必要がある
    {
        if (clipContext->_isUsing) // 書くことになっていた
        {
            CubismRenderTarget_Vulkan* currentHighPrecisionMaskColorBuffer = &_drawableMaskBuffers[_commandBufferCurrent][clipContext->_bufferIndex];

            EnsurePreviousRenderFinished(drawCommandBuffer, currentHighPrecisionMaskColorBuffer);

            // 描画順を考慮して今までに積んだコマンドを実行する
            SubmitCommand(updateCommandBuffer, _updateFinishedSemaphores[_commandBufferCurrent]);
            SubmitCommand(drawCommandBuffer, VK_NULL_HANDLE, _updateFinishedSemaphores[_commandBufferCurrent]);
            vkBeginCommandBuffer(updateCommandBuffer, &beginInfo);
            vkBeginCommandBuffer(drawCommandBuffer, &beginInfo);

            currentHighPrecisionMaskColorBuffer->BeginDraw(drawCommandBuffer, 1.0f, 1.0f, 1.0f, 1.0f, true);

            // 生成したRenderTargetと同じサイズでビューポートを設定
            const VkViewport viewport = GetViewport(
                static_cast<csmFloat32>(_drawableClippingManager->GetClippingMaskBufferSize().X),
                static_cast<csmFloat32>(_drawableClippingManager->GetClippingMaskBufferSize().Y),
                0.0, 1.0
            );
            vkCmdSetViewport(drawCommandBuffer, 0, 1, &viewport);

            const VkRect2D rect = GetScissor(
                0.0, 0.0,
                static_cast<csmFloat32>(_drawableClippingManager->GetClippingMaskBufferSize().X),
                static_cast<csmFloat32>(_drawableClippingManager->GetClippingMaskBufferSize().Y)
            );
            vkCmdSetScissor(drawCommandBuffer, 0, 1, &rect);

            const csmInt32 clipDrawCount = clipContext->_clippingIdCount;
            for (csmInt32 ctx = 0; ctx < clipDrawCount; ctx++)
            {
                const csmInt32 clipDrawIndex = clipContext->_clippingIdList[ctx];

                // 頂点情報が更新されておらず、信頼性がない場合は描画をパスする
                if (!GetModel()->GetDrawableDynamicFlagVertexPositionsDidChange(clipDrawIndex))
                {
                    continue;
                }

                IsCulling(GetModel()->GetDrawableCulling(clipDrawIndex) != 0);

                SetClippingContextBufferForMask(clipContext);
                DrawMeshVulkan(*GetModel(), clipDrawIndex, drawCommandBuffer, updateCommandBuffer);
            }
            // --- 後処理 ---
            currentHighPrecisionMaskColorBuffer->EndDraw(drawCommandBuffer);
            SetClippingContextBufferForMask(NULL);
            SubmitCommand(updateCommandBuffer, _updateFinishedSemaphores[_commandBufferCurrent]);
            SubmitCommand(drawCommandBuffer, VK_NULL_HANDLE, _updateFinishedSemaphores[_commandBufferCurrent]);
            vkBeginCommandBuffer(updateCommandBuffer, &beginInfo);
            vkBeginCommandBuffer(drawCommandBuffer, &beginInfo);

            // 元のレンダーターゲットを再開
            if (_currentRenderTarget == NULL)
            {
                BeginRendering(drawCommandBuffer, true);
            }
            else if (!_currentRenderTarget->IsRendering())
            {
                _currentRenderTarget->BeginDraw(drawCommandBuffer, 0.0f, 0.0f, 0.0f, 0.0f, false);
            }
        }
    }

    // ビューポートを設定する
    const VkViewport viewport = GetViewport(
        static_cast<csmFloat32>(s_renderExtent.width),
        static_cast<csmFloat32>(s_renderExtent.height),
        0.0, 1.0
    );
    vkCmdSetViewport(drawCommandBuffer, 0, 1, &viewport);

    const VkRect2D rect = GetScissor(
        0.0, 0.0,
        static_cast<csmFloat32>(s_renderExtent.width),
        static_cast<csmFloat32>(s_renderExtent.height)
    );
    vkCmdSetScissor(drawCommandBuffer, 0, 1, &rect);

    // クリッピングマスクをセットする
    SetClippingContextBufferForDraw(clipContext);
    IsCulling(GetModel()->GetDrawableCulling(drawableIndex) != 0);
    DrawMeshVulkan(*GetModel(), drawableIndex, drawCommandBuffer, updateCommandBuffer);
}

void CubismRenderer_Vulkan::SubmitDrawToParentOffscreen(csmInt32 objectIndex, DrawableObjectType objectType,
                                                        VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo)
{
    if (_currentOffscreen == NULL ||
        objectIndex == CubismModel::CubismNoIndex_Offscreen)
    {
        return;
    }

    csmInt32 currentOwnerIndex = GetModel()->GetOffscreenOwnerIndices()[_currentOffscreen->GetOffscreenIndex()];

    // オーナーが不明な場合は処理を終了
    if (currentOwnerIndex == CubismModel::CubismNoIndex_Offscreen)
    {
        return;
    }

    csmInt32 targetParentIndex = CubismModel::CubismNoIndex_Parent;
    // 描画オブジェクトのタイプ別に親パーツのインデックスを取得
    switch (objectType)
    {
    case DrawableObjectType_Drawable:
        targetParentIndex = GetModel()->GetDrawableParentPartIndex(objectIndex);
        break;
    case DrawableObjectType_Offscreen:
        targetParentIndex = GetModel()->GetPartParentPartIndex(GetModel()->GetOffscreenOwnerIndices()[objectIndex]);
        break;
    default:
        // 不明なタイプだった場合は処理を終了
        return;
    }

    // 階層を辿って現在のオフスクリーンのオーナーのパーツがいたら処理を終了する。
    while (targetParentIndex != CubismModel::CubismNoIndex_Parent)
    {
        // オブジェクトの親が現在のオーナーと同じ場合は処理を終了
        if (targetParentIndex == currentOwnerIndex)
        {
            return;
        }

        targetParentIndex = GetModel()->GetPartParentPartIndex(targetParentIndex);
    }

    /**
     * 呼び出し元の描画オブジェクトは現オフスクリーンの描画対象でない。
     * つまり描画順グループの仕様により、現オフスクリーンの描画対象は全て描画完了しているので
     * 現オフスクリーンを描画する。
     */
    DrawOffscreen(_currentOffscreen, updateCommandBuffer, drawCommandBuffer, beginInfo);

    // さらに親のオフスクリーンに伝搬可能なら伝搬する。
    SubmitDrawToParentOffscreen(objectIndex, objectType, updateCommandBuffer, drawCommandBuffer, beginInfo);
}

void CubismRenderer_Vulkan::AddOffscreen(csmInt32 offscreenIndex,
                                         VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo)
{
    // 以前のオフスクリーンレンダリングターゲットを親に伝搬する処理を追加する
    if (_currentOffscreen != NULL && _currentOffscreen->GetOffscreenIndex() != offscreenIndex)
    {
        csmBool isParent = false;
        csmInt32 ownerIndex = GetModel()->GetOffscreenOwnerIndices()[offscreenIndex];
        csmInt32 parentIndex = GetModel()->GetPartParentPartIndex(ownerIndex);

        csmInt32 currentOffscreenIndex = _currentOffscreen->GetOffscreenIndex();
        csmInt32 currentOffscreenOwnerIndex = GetModel()->GetOffscreenOwnerIndices()[currentOffscreenIndex];

        while (parentIndex != CubismModel::CubismNoIndex_Parent)
        {
            if (parentIndex == currentOffscreenOwnerIndex)
            {
                isParent = true;
                break;
            }
            parentIndex = GetModel()->GetPartParentPartIndex(parentIndex);
        }

        if (!isParent)
        {
            // 現在のオフスクリーンレンダリングターゲットがあるなら、親に伝搬する
            SubmitDrawToParentOffscreen(offscreenIndex, DrawableObjectType_Offscreen,
                                        updateCommandBuffer, drawCommandBuffer, beginInfo);
        }
    }

    CubismOffscreenRenderTarget_Vulkan* offscreen = &_offscreenList.At(offscreenIndex);
    offscreen->SetOffscreenRenderTarget(s_device, s_physicalDevice,
        _modelRenderTargetWidth, _modelRenderTargetHeight, s_imageFormat, s_depthFormat);

    // 以前のオフスクリーンレンダリングターゲットを取得
    CubismOffscreenRenderTarget_Vulkan* oldOffscreen = offscreen->GetParentPartOffscreen();
    offscreen->SetOldOffscreen(oldOffscreen);

    // 別バッファに描画を開始
    EnsurePreviousRenderFinished(drawCommandBuffer, offscreen->GetRenderTarget());
    offscreen->GetRenderTarget()->BeginDraw(drawCommandBuffer, 0.0f, 0.0f, 0.0f, 0.0f, true);

    const VkViewport viewport = GetViewport(
        static_cast<csmFloat32>(_modelRenderTargetWidth),
        static_cast<csmFloat32>(_modelRenderTargetHeight),
        0.0, 1.0
    );
    vkCmdSetViewport(drawCommandBuffer, 0, 1, &viewport);

    const VkRect2D rect = GetScissor(
        0.0, 0.0,
        static_cast<csmFloat32>(_modelRenderTargetWidth),
        static_cast<csmFloat32>(_modelRenderTargetHeight)
    );
    vkCmdSetScissor(drawCommandBuffer, 0, 1, &rect);

    // 現在のオフスクリーンレンダリングターゲットを設定
    _currentOffscreen = offscreen;
    _currentRenderTarget = offscreen->GetRenderTarget();
}

void CubismRenderer_Vulkan::DrawOffscreen(CubismOffscreenRenderTarget_Vulkan* currentOffscreen,
                                          VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer, const VkCommandBufferBeginInfo& beginInfo)
{
    if (currentOffscreen == NULL)
    {
        return;
    }

    CubismRenderTarget_Vulkan* renderTarget = currentOffscreen->GetRenderTarget();
    if (renderTarget == NULL || !renderTarget->IsValid())
    {
        return;
    }
    renderTarget->EndDraw(drawCommandBuffer);

    // 親のオフスクリーン、またはモデル描画用ターゲットに描画する
    CubismOffscreenRenderTarget_Vulkan* parentOffscreen = currentOffscreen->GetOldOffscreen();
    _currentOffscreen = parentOffscreen;

    csmInt32 offscreenIndex = currentOffscreen->GetOffscreenIndex();

    // クリッピングマスク
    CubismClippingContext_Vulkan* clipContext = (_offscreenClippingManager != NULL)
        ? (*_offscreenClippingManager->GetClippingContextListForOffscreen())[offscreenIndex]
        : NULL;

    if (clipContext != NULL && IsUsingHighPrecisionMask())
    {
        if (clipContext->_isUsing)
        {
            CubismRenderTarget_Vulkan* currentHighPrecisionMaskColorBuffer = &_offscreenMaskBuffers[_commandBufferCurrent][clipContext->_bufferIndex];
            EnsurePreviousRenderFinished(drawCommandBuffer, currentHighPrecisionMaskColorBuffer);

            // 描画順を考慮して今までに積んだコマンドを実行する
            SubmitCommand(updateCommandBuffer, _updateFinishedSemaphores[_commandBufferCurrent]);
            SubmitCommand(drawCommandBuffer, VK_NULL_HANDLE, _updateFinishedSemaphores[_commandBufferCurrent]);
            vkBeginCommandBuffer(updateCommandBuffer, &beginInfo);
            vkBeginCommandBuffer(drawCommandBuffer, &beginInfo);

            currentHighPrecisionMaskColorBuffer->BeginDraw(drawCommandBuffer, 1.0f, 1.0f, 1.0f, 1.0f, true);

            // 生成したRenderTargetと同じサイズでビューポートを設定
            const VkViewport viewport = GetViewport(
                static_cast<csmFloat32>(_offscreenClippingManager->GetClippingMaskBufferSize().X),
                static_cast<csmFloat32>(_offscreenClippingManager->GetClippingMaskBufferSize().Y),
                0.0, 1.0
            );
            vkCmdSetViewport(drawCommandBuffer, 0, 1, &viewport);

            const VkRect2D rect = GetScissor(
                0.0, 0.0,
                static_cast<csmFloat32>(_offscreenClippingManager->GetClippingMaskBufferSize().X),
                static_cast<csmFloat32>(_offscreenClippingManager->GetClippingMaskBufferSize().Y)
            );
            vkCmdSetScissor(drawCommandBuffer, 0, 1, &rect);

            const csmInt32 clipDrawCount = clipContext->_clippingIdCount;
            for (csmInt32 index = 0; index < clipDrawCount; index++)
            {
                const csmInt32 clipDrawIndex = clipContext->_clippingIdList[index];

                // 頂点情報が更新されておらず、信頼性がない場合は描画をパスする
                if (!GetModel()->GetDrawableDynamicFlagVertexPositionsDidChange(clipDrawIndex))
                {
                    continue;
                }

                IsCulling(GetModel()->GetDrawableCulling(clipDrawIndex) != 0);

                SetClippingContextBufferForMask(clipContext);
                DrawMeshVulkan(*GetModel(), clipDrawIndex, drawCommandBuffer, updateCommandBuffer);
            }

            // --- 後処理 ---
            currentHighPrecisionMaskColorBuffer->EndDraw(drawCommandBuffer);
            SetClippingContextBufferForMask(NULL);
            SubmitCommand(updateCommandBuffer, _updateFinishedSemaphores[_commandBufferCurrent]);
            SubmitCommand(drawCommandBuffer, VK_NULL_HANDLE, _updateFinishedSemaphores[_commandBufferCurrent]);
            vkBeginCommandBuffer(updateCommandBuffer, &beginInfo);
            vkBeginCommandBuffer(drawCommandBuffer, &beginInfo);

            // ビューポートを戻す
            const VkViewport viewportRestore = GetViewport(
                static_cast<csmFloat32>(_modelRenderTargetWidth),
                static_cast<csmFloat32>(_modelRenderTargetHeight),
                0.0, 1.0
            );
            vkCmdSetViewport(drawCommandBuffer, 0, 1, &viewportRestore);

            const VkRect2D rectRestore = GetScissor(
                0.0, 0.0,
                static_cast<csmFloat32>(_modelRenderTargetWidth),
                static_cast<csmFloat32>(_modelRenderTargetHeight)
            );
            vkCmdSetScissor(drawCommandBuffer, 0, 1, &rectRestore);
        }
    }

    // クリッピングマスクをセットする
    SetClippingContextBufferForOffscreen(clipContext);

    IsCulling(GetModel()->GetOffscreenCulling(offscreenIndex) != 0);

    // オフスクリーンテクスチャを親に描画
    DrawOffscreenVulkan(*GetModel(), currentOffscreen, clipContext, updateCommandBuffer, drawCommandBuffer);
}

void CubismRenderer_Vulkan::DrawOffscreenVulkan(const CubismModel& model, CubismOffscreenRenderTarget_Vulkan* offscreen,
                                                CubismClippingContext_Vulkan* clipContext,
                                                VkCommandBuffer updateCommandBuffer, VkCommandBuffer drawCommandBuffer)
{
    if (offscreen == NULL)
    {
        return;
    }

    CubismRenderTarget_Vulkan* renderTarget = offscreen->GetRenderTarget();
    if (renderTarget == NULL || !renderTarget->IsValid())
    {
        return;
    }

    csmInt32 offscreenIndex = offscreen->GetOffscreenIndex();

    // 裏面描画の有効・無効
    if (IsCulling())
    {
        vkCmdSetCullModeEXT(drawCommandBuffer, VK_CULL_MODE_BACK_BIT);
    }
    else
    {
        vkCmdSetCullModeEXT(drawCommandBuffer, VK_CULL_MODE_NONE);
    }

    if (_currentOffscreen != NULL)
    {
        _currentRenderTarget = _currentOffscreen->GetRenderTarget();
    }
    else
    {
        _currentRenderTarget = NULL;
    }


    // 親のレンダーターゲットに描画を開始
    if (_currentRenderTarget != NULL)
    {
        // 親のオフスクリーンに描画
        _currentRenderTarget->BeginDraw(drawCommandBuffer, 0.0f, 0.0f, 0.0f, 0.0f, false);
    }
    else
    {
        // メインのレンダーターゲットに描画
        _modelRenderTargets[0].BeginDraw(drawCommandBuffer, 0.0f, 0.0f, 0.0f, 0.0f, false);
        _currentRenderTarget = &_modelRenderTargets[0];
    }

    ExecuteDrawForOffscreen(model, offscreen, drawCommandBuffer);

    // 後処理
    offscreen->StopUsingRenderTexture();
    SetClippingContextBufferForOffscreen(NULL);
    SetClippingContextBufferForMask(NULL);
}

void CubismRenderer_Vulkan::SetBlendTextureBarrier(VkCommandBuffer drawCommandBuffer)
{
    CubismRenderTarget_Vulkan* blendRenderTarget = (_currentRenderTarget != NULL)
                                                          ? _currentRenderTarget
                                                          : GetModelRenderTarget();

        VkImageMemoryBarrier2 imageBarrier{};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageBarrier.image = blendRenderTarget->GetTextureImage();
        imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        depInfo.memoryBarrierCount = 0;
        depInfo.pMemoryBarriers = nullptr;
        depInfo.bufferMemoryBarrierCount = 0;
        depInfo.pBufferMemoryBarriers = nullptr;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(drawCommandBuffer, &depInfo);
}

void CubismRenderer_Vulkan::EnsurePreviousRenderFinished(VkCommandBuffer commandBuffer, const CubismRenderTarget_Vulkan* nextRenderTarget)
{
    // 描画終了
    if (_currentRenderTarget == NULL)
    {
        EndRendering(commandBuffer);
    }
    else if (_currentRenderTarget != nextRenderTarget && _currentRenderTarget->IsRendering())
    {
        // 描画中の場合終了する
        _currentRenderTarget->EndDraw(commandBuffer);
    }
}

}}}}

//------------ LIVE2D NAMESPACE ------------
