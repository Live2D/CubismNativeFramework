/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismRenderer_Metal.hpp"
#include "CubismCommandBuffer_Metal.hpp"
#include "CubismRenderTarget_Metal.hpp"
#include "Math/CubismMatrix44.hpp"
#include "Model/CubismModel.hpp"
#include "CubismShader_Metal.hpp"
#include "Shaders/MetalShaderTypes.h"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

namespace
{
    csmUint32 s_maskBufferCount = 0;         ///< マスクバッファの数。モデルロード前に毎回設定されている必要あり。
    id<MTLDevice> s_device = nil;         ///< 使用デバイス。モデルロード前に毎回設定されている必要あり。
    id<MTLDevice> s_InitializeClippingDevice = nil;  ///< CubismClippingManagerのinitializeForXXXを呼び出す前に設定しておくデバイス。

    const csmFloat32 modelRenderTargetVertexArray[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    const csmFloat32 modelRenderTargetUvArray[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
    };
    const csmFloat32 modelRenderTargetReverseUvArray[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    };
    const csmInt16 modelRenderTargetIndexArray[] = {
        0, 1, 2,
        2, 1, 3,
    };
}

/*********************************************************************************************************************
*                                      CubismClippingManager_Metal
********************************************************************************************************************/
void CubismClippingManager_Metal::SetupClippingContext(CubismModel& model, CubismRenderer_Metal* renderer, CubismRenderTarget_Metal* lastColorBuffer, csmRectF lastViewport, CubismRenderer::DrawableObjectType drawableObjectType)
{
    // 全てのクリッピングを用意する
    // 同じクリップ（複数の場合はまとめて１つのクリップ）を使う場合は１度だけ設定する
    csmInt32 usingClipCount = 0;
    for (csmUint32 clipIndex = 0; clipIndex < _clippingContextListForMask.GetSize(); clipIndex++)
    {
        // １つのクリッピングマスクに関して
        CubismClippingContext_Metal* cc = _clippingContextListForMask[clipIndex];

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
    id <MTLRenderCommandEncoder> renderEncoder = nil;
    MTLViewport clipVp = {0, 0, GetClippingMaskBufferSize().X, GetClippingMaskBufferSize().Y, 0.0, 1.0};

    // 後の計算のためにインデックスの最初をセットする。
    switch (drawableObjectType)
    {
    case CubismRenderer::DrawableObjectType_Drawable:
    default:
        _currentMaskBuffer = renderer->GetDrawableMaskBuffer(0);
        break;
    case CubismRenderer::DrawableObjectType_Offscreen:
        _currentMaskBuffer = renderer->GetOffscreenMaskBuffer(0);
        break;
    }
    renderer->EndRenderTarget();
    _currentMaskBuffer->Clear(1.0f, 1.0f, 1.0f, 1.0f);
    _currentMaskBuffer->BeginDraw(renderer->_mtlCommandBuffer);
    renderEncoder = _currentMaskBuffer->GetCommandEncoder();

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
        CubismClippingContext_Metal* clipContext = _clippingContextListForMask[clipIndex];
        csmRectF* allClippedDrawRect = clipContext->_allClippedDrawRect; //このマスクを使う、全ての描画オブジェクトの論理座標上の囲み矩形
        csmRectF* layoutBoundsOnTex01 = clipContext->_layoutBounds; //この中にマスクを収める
        const csmFloat32 MARGIN = 0.05f;
        const csmBool isRightHanded = false;

        // clipContextに設定したレンダーテクスチャをインデックスで取得
        CubismRenderTarget_Metal* maskBuffer = nil;
        switch (drawableObjectType)
        {
        case CubismRenderer::DrawableObjectType_Drawable:
        default:
            maskBuffer = renderer->GetDrawableMaskBuffer(clipContext->_bufferIndex);
            break;
        case CubismRenderer::DrawableObjectType_Offscreen:
            maskBuffer = renderer->GetOffscreenMaskBuffer(clipContext->_bufferIndex);
            break;
        }

        // 現在のレンダーテクスチャがclipContextのものと異なる場合
        if (_currentMaskBuffer != maskBuffer)
        {
            _currentMaskBuffer->EndDraw();
            _currentMaskBuffer = maskBuffer;
            // マスク用RenderTextureをactiveにセット
            _currentMaskBuffer->Clear(1.0f, 1.0f, 1.0f, 1.0f);
            _currentMaskBuffer->BeginDraw(renderer->_mtlCommandBuffer);
            renderEncoder = _currentMaskBuffer->GetCommandEncoder();
            // マスク用RenderTextureをactiveにセット
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
        CreateMatrixForMask(isRightHanded, layoutBoundsOnTex01, scaleX, scaleY);

        clipContext->_matrixForMask.SetMatrix(_tmpMatrixForMask.GetArray());
        clipContext->_matrixForDraw.SetMatrix(_tmpMatrixForDraw.GetArray());

        if(drawableObjectType == CubismRenderer::DrawableObjectType_Offscreen)
        {
            // clipContext * mvp^-1
            CubismMatrix44 invertMvp = renderer->GetMvpMatrix().GetInvert();
            clipContext->_matrixForDraw.MultiplyByMatrix(&invertMvp);
        }

        // 実際のマスク描画を行う
        const csmInt32 clipDrawCount = clipContext->_clippingIdCount;
        for (csmInt32 i = 0; i < clipDrawCount; i++)
        {
            const csmInt32 clipDrawIndex = clipContext->_clippingIdList[i];
            CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBufferData = clipContext->_clippingCommandBufferList->At(i);// [i];

            // 頂点情報が更新されておらず、信頼性がない場合は描画をパスする
            if (!model.GetDrawableDynamicFlagVertexPositionsDidChange(clipDrawIndex))
            {
                continue;
            }

            // Update Vertex / Index buffer.
            {
                csmFloat32* vertices = const_cast<csmFloat32*>(model.GetDrawableVertices(clipDrawIndex));
                Core::csmVector2* uvs = const_cast<Core::csmVector2*>(model.GetDrawableVertexUvs(clipDrawIndex));
                csmUint16* vertexIndices = const_cast<csmUint16*>(model.GetDrawableVertexIndices(clipDrawIndex));
                const csmUint32 vertexCount = model.GetDrawableVertexCount(clipDrawIndex);
                const csmUint32 vertexIndexCount = model.GetDrawableVertexIndexCount(clipDrawIndex);

                drawCommandBufferData->UpdateVertexBuffer(vertices, uvs, vertexCount);
                if (vertexIndexCount > 0)
                {
                    drawCommandBufferData->UpdateIndexBuffer(vertexIndices, vertexIndexCount);
                }

                if (vertexCount <= 0)
                {
                    continue;
                }
            }

            renderer->IsCulling(model.GetDrawableCulling(clipDrawIndex) != 0);

            // マスクがクリアされていないなら処理する
            if (!_clearedMaskBufferFlags[clipContext->_bufferIndex])
            {
                // 生成したOffscreenSurfaceと同じサイズでビューポートを設定
                [renderEncoder setViewport:clipVp];
                _clearedMaskBufferFlags[clipContext->_bufferIndex] = true;
            }

            // 今回専用の変換を適用して描く
            // チャンネルも切り替える必要がある(A,R,G,B)
            renderer->SetClippingContextBufferForMask(clipContext);

            renderer->DrawMeshMetal(drawCommandBufferData, renderEncoder, model, clipDrawIndex, NULL);
        }
    }

    // --- 後処理 ---
    _currentMaskBuffer->EndDraw();
    renderer->BeginRenderTarget();
    renderer->SetClippingContextBufferForMask(NULL);
}

/*********************************************************************************************************************
*                                      CubismClippingContext_Metal
********************************************************************************************************************/
CubismClippingContext_Metal::CubismClippingContext_Metal(CubismClippingManager<CubismClippingContext_Metal, CubismRenderTarget_Metal>* manager, CubismModel& model, const csmInt32* clippingDrawableIndices, csmInt32 clipCount)
    : CubismClippingContext(clippingDrawableIndices, clipCount)
{
    _owner = manager;

    _clippingCommandBufferList = CSM_NEW csmVector<CubismCommandBuffer_Metal::DrawCommandBuffer*>;
    for (csmUint32 i = 0; i < _clippingIdCount; ++i)
    {
        const csmInt32 clippingId = _clippingIdList[i];
        CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer = NULL;
        const csmInt32 drawableVertexCount = model.GetDrawableVertexCount(clippingId);
        const csmInt32 drawableVertexIndexCount = model.GetDrawableVertexIndexCount(clippingId);
        const csmSizeInt vertexSize = sizeof(csmFloat32) * 2;


        drawCommandBuffer = CSM_NEW CubismCommandBuffer_Metal::DrawCommandBuffer();
        drawCommandBuffer->CreateVertexBuffer(s_InitializeClippingDevice, vertexSize, drawableVertexCount * 2);      // Vertices + UVs
        drawCommandBuffer->CreateIndexBuffer(s_InitializeClippingDevice, drawableVertexIndexCount);


        _clippingCommandBufferList->PushBack(drawCommandBuffer);
    }
}

CubismClippingContext_Metal::~CubismClippingContext_Metal()
{
    if (_clippingCommandBufferList != NULL)
    {
        for (csmUint32 i = 0; i < _clippingCommandBufferList->GetSize(); ++i)
        {
            if (_clippingCommandBufferList->At(i) != NULL)
            {
                CSM_DELETE(_clippingCommandBufferList->At(i));
                _clippingCommandBufferList->At(i) = NULL;
            }
        }

        CSM_DELETE(_clippingCommandBufferList);
        _clippingCommandBufferList = NULL;
    }
}

CubismClippingManager<CubismClippingContext_Metal, CubismRenderTarget_Metal>* CubismClippingContext_Metal::GetClippingManager()
{
    return _owner;
}

/*********************************************************************************************************************
 *                                      CubismRenderer_Metal
 ********************************************************************************************************************/
CubismRenderer* CubismRenderer::Create(csmUint32 width, csmUint32 height)
{
    return CSM_NEW CubismRenderer_Metal(width, height);
}

void CubismRenderer::StaticRelease()
{
}

void CubismRenderer_Metal::SetConstantSettings(id<MTLDevice> device, csmUint32 maskBufferCount)
{
    if (device == nil)
    {
        return;
    }

    s_maskBufferCount = maskBufferCount;
    s_device = device;
}

CubismRenderer_Metal::CubismRenderer_Metal(csmUint32 width, csmUint32 height)
    : CubismRenderer(width, height)
    , _device(NULL)
    , _mtlCommandBuffer(NULL)
    , _mtlCommandEncoder(NULL)
    , _renderPassDescriptor(NULL)
    , _deviceInfo(NULL)
    , _drawableClippingManager(NULL)
    , _offscreenClippingManager(NULL)
    , _clippingContextBufferForMask(NULL)
    , _clippingContextBufferForDrawable(NULL)
    , _clippingContextBufferForOffscreen(NULL)
    , _copyCommandBuffer(NULL)
    , _offscreenDrawCommandBuffer(NULL)
{
    // テクスチャ対応マップの容量を確保しておく.
    _textures.PrepareCapacity(32, true);
}

CubismRenderer_Metal::~CubismRenderer_Metal()
{
    CSM_DELETE_SELF(CubismClippingManager_Metal, _drawableClippingManager);
    CSM_DELETE_SELF(CubismClippingManager_Metal, _offscreenClippingManager);

    if (_drawableDrawCommandBuffer.GetSize() > 0)
    {
        for (csmInt32 i = 0; i < _drawableDrawCommandBuffer.GetSize(); ++i)
        {
            if (_drawableDrawCommandBuffer[i] != NULL)
            {
                CSM_DELETE(_drawableDrawCommandBuffer[i]);
            }
        }

        _drawableDrawCommandBuffer.Clear();
    }

   if(_copyCommandBuffer != NULL)
   {
      CSM_DELETE(_copyCommandBuffer);
   }

   if(_offscreenDrawCommandBuffer != NULL)
   {
     CSM_DELETE(_offscreenDrawCommandBuffer);
   }

    if (_textures.GetSize() > 0)
    {
        _textures.Clear();
    }

    for (csmUint32 i = 0; i < _drawableMasks.GetSize(); ++i)
    {
        _drawableMasks[i].DestroyRenderTarget();
    }
    _drawableMasks.Clear();

    for(csmUint32 i = 0; i < _offscreenMasks.GetSize(); ++i)
    {
        _offscreenMasks[i].DestroyRenderTarget();
    }
    _offscreenMasks.Clear();

    for(csmUint32 i  = 0; i < _modelRenderTargets.GetSize(); ++i)
    {
        _modelRenderTargets[i].DestroyRenderTarget();
    }
    _modelRenderTargets.Clear();
}

void CubismRenderer_Metal::Initialize(CubismModel* model)
{
    Initialize(model, 1);
}

void CubismRenderer_Metal::Initialize(CubismModel* model, csmInt32 maskBufferCount)
{
    // デバイスが設定されていない場合は設定する
    if (_device == NULL)
    {
        if (!OnDeviceChanged())
        {
            return;
        }
    }

    // 1未満は1に補正する
    if (maskBufferCount < 1)
    {
        maskBufferCount = 1;
        CubismLogWarning("The number of render textures must be an integer greater than or equal to 1. Set the number of render textures to 1.");
    }

    if (model->IsUsingMasking())
    {
        // ubismClippingManager_Metal内にdeviceを渡す用
        s_InitializeClippingDevice = _device;
        _drawableClippingManager = CSM_NEW CubismClippingManager_Metal();  //クリッピングマスク・バッファ前処理方式を初期化
        _drawableClippingManager->Initialize(
            *model,
            maskBufferCount,
            DrawableObjectType_Drawable
        );

        _drawableMasks.Clear();

        for (csmInt32 i = 0; i < maskBufferCount; ++i)
        {
            CubismRenderTarget_Metal masks;
            masks.SetMTLPixelFormat(MTLPixelFormatBGRA8Unorm);
            masks.CreateRenderTarget(_device, _drawableClippingManager->GetClippingMaskBufferSize().X, _drawableClippingManager->GetClippingMaskBufferSize().Y);
            _drawableMasks.PushBack(masks);
        }
    }

    if(model->IsUsingMaskingForOffscreen())
    {
        _offscreenClippingManager = CSM_NEW CubismClippingManager_Metal();  //クリッピングマスク・バッファ前処理方式を初期化
        _offscreenClippingManager->Initialize(
            *model,
            maskBufferCount,
            DrawableObjectType_Offscreen
        );

        _offscreenMasks.Clear();

        for(csmInt32 i = 0; i < maskBufferCount; ++i)
        {
            CubismRenderTarget_Metal offscreenMask;
            offscreenMask.SetMTLPixelFormat(MTLPixelFormatBGRA8Unorm);
            offscreenMask.CreateRenderTarget(_device, _offscreenClippingManager->GetClippingMaskBufferSize().X, _offscreenClippingManager->GetClippingMaskBufferSize().Y);
            _offscreenMasks.PushBack(offscreenMask);
        }
    }

    if(IsBlendMode(model))
    {
        // オフスクリーンの作成
        // 添え字 0 は描画先となる
        // 添え字 1 はTextureBarrierの代替用
        csmInt32 createSize = 2;
        for(csmInt32 i = 0; i < createSize; i++)
        {
            CubismRenderTarget_Metal modelRenderTarget;
            modelRenderTarget.SetMTLPixelFormat(MTLPixelFormatBGRA8Unorm);
            modelRenderTarget.CreateRenderTarget(_device, _modelRenderTargetWidth, _modelRenderTargetHeight);
            _modelRenderTargets.PushBack(modelRenderTarget);
        }
    }

    _sortedObjectsIndexList.Resize(model->GetDrawableCount() + model->GetOffscreenCount(), 0);
    _sortedObjectsTypeList.Resize(model->GetDrawableCount() + model->GetOffscreenCount(), DrawableObjectType_Drawable);

    _drawableDrawCommandBuffer.Resize(model->GetDrawableCount());

    for (csmInt32 i = 0; i < _drawableDrawCommandBuffer.GetSize(); ++i)
    {
        const csmInt32 drawableVertexCount = model->GetDrawableVertexCount(i);
        const csmInt32 drawableVertexIndexCount = model->GetDrawableVertexIndexCount(i);
        const csmSizeInt vertexSize = sizeof(csmFloat32) * 2;

        _drawableDrawCommandBuffer[i] = CSM_NEW CubismCommandBuffer_Metal::DrawCommandBuffer();

        // ここで頂点情報のメモリを確保する
        _drawableDrawCommandBuffer[i]->CreateVertexBuffer(_device, vertexSize, drawableVertexCount);

        if (drawableVertexIndexCount > 0)
        {
            _drawableDrawCommandBuffer[i]->CreateIndexBuffer(_device, drawableVertexIndexCount);
        }
    }

    const csmInt32 offscreenCount = model->GetOffscreenCount();
    // オフスクリーンの数が0の場合は何もしない
    if(offscreenCount > 0)
    {
        _offscreenList = csmVector<CubismOffscreenRenderTarget_Metal>(offscreenCount);
        for (csmInt32 offscreenIndex = 0; offscreenIndex < offscreenCount; ++offscreenIndex)
        {
            CubismOffscreenRenderTarget_Metal renderTarget;
            renderTarget.SetOffscreenIndex(offscreenIndex);
            _offscreenList.PushBack(renderTarget);
        }

        // 全てのオフスクリーンを登録し終わってから行う
        SetupParentOffscreens(model, offscreenCount);

        const csmSizeInt vertexSize = sizeof(csmFloat32) * 2;
        _offscreenDrawCommandBuffer = CSM_NEW CubismCommandBuffer_Metal::DrawCommandBuffer();
        _offscreenDrawCommandBuffer->CreateVertexBuffer(_device, vertexSize, 4);
        _offscreenDrawCommandBuffer->UpdateVertexBuffer(const_cast<csmFloat32*>(modelRenderTargetVertexArray), const_cast<csmFloat32*>(modelRenderTargetReverseUvArray), 4);
        _offscreenDrawCommandBuffer->CreateIndexBuffer(_device, 6);
        _offscreenDrawCommandBuffer->UpdateIndexBuffer(const_cast<csmInt16*>(modelRenderTargetIndexArray), 6);
    }

    if(IsBlendMode(model))
    {
        const csmSizeInt vertexSize = sizeof(csmFloat32) * 2;
        _copyCommandBuffer = CSM_NEW CubismCommandBuffer_Metal::DrawCommandBuffer();
        _copyCommandBuffer->CreateVertexBuffer(_device, vertexSize, 4);
        _copyCommandBuffer->UpdateVertexBuffer(const_cast<csmFloat32*>(modelRenderTargetVertexArray), const_cast<csmFloat32*>(modelRenderTargetUvArray), 4);
        _copyCommandBuffer->CreateIndexBuffer(_device, 6);
        _copyCommandBuffer->UpdateIndexBuffer(const_cast<csmInt16*>(modelRenderTargetIndexArray), 6);
    }

    CubismRenderer::Initialize(model, maskBufferCount);  //親クラスの処理を呼ぶ
}

void CubismRenderer_Metal::SetupParentOffscreens(const CubismModel* model, csmInt32 offscreenCount)
{
    CubismOffscreenRenderTarget_Metal* parentOffscreen;
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
                    continue;  //オフスクリーンのインデックスが親と一致しなければスキップ
                }

                parentOffscreen = &_offscreenList.At(i);
                break;
            }

            if (parentOffscreen != NULL)
            {
                break;  // 親のオフスクリーンが見つかった場合はループを抜ける
            }

            parentIndex = model->GetPartParentPartIndex(parentIndex);
        }

        // 親のオフスクリーンを設定
        _offscreenList.At(offscreenIndex).SetParentPartOffscreen(parentOffscreen);
    }
}

csmBool CubismRenderer_Metal::OnDeviceChanged()
{
    // 1未満は1に補正する
    if (s_maskBufferCount < 1)
    {
        s_maskBufferCount = 1;
        CubismLogWarning("The number of render textures must be an integer greater than or equal to 1. Set the number of render textures to 1.");
    }

    if (s_device == NULL)
    {
        CubismLogError("Device has not been set.");
        CSM_ASSERT(0);
        return false;
    }

    const csmBool isInitialized = _device != NULL && _device != s_device;

    _device = s_device;
    _deviceInfo = CubismDeviceInfo_Metal::GetDeviceInfo(s_device);
    if (isInitialized)
    {
        Initialize(GetModel(), s_maskBufferCount);
    }

    return true;
}

void CubismRenderer_Metal::StartFrame(id<MTLCommandBuffer> commandBuffer, MTLRenderPassDescriptor* renderPassDescriptor)
{
    _mtlCommandBuffer = commandBuffer;
    _renderPassDescriptor = renderPassDescriptor;
}

void CubismRenderer_Metal::PreDraw()
{
    if (_mtlCommandEncoder == nil)
    {
        _mtlCommandEncoder = [_mtlCommandBuffer renderCommandEncoderWithDescriptor:_renderPassDescriptor];
    }
}

void CubismRenderer_Metal::PostDraw()
{
    if (_mtlCommandEncoder != nil)
    {
        [_mtlCommandEncoder endEncoding];
        _mtlCommandEncoder = nil;
    }
}

void CubismRenderer_Metal::BeginRenderTarget(csmBool isClearEnabled)
{
    if (_mtlCommandEncoder != nil)
    {
        return;
    }

    if(IsBlendMode())
    {
        CubismRenderTarget_Metal* target = NULL;
        if (_currentOffscreen != NULL)
        {
            target = _currentOffscreen->GetRenderTarget();
        }
        if (target == NULL)
        {
            target = &_modelRenderTargets[0];
        }

        if (isClearEnabled)
        {
            target->Clear(0.0f, 0.0f, 0.0f, 0.0f);
        }
        target->BeginDraw(_mtlCommandBuffer);
        _mtlCommandEncoder = target->GetCommandEncoder();
    }
    else
    {
        PreDraw();
    }
}

void CubismRenderer_Metal::EndRenderTarget()
{
    if(IsBlendMode())
    {
        if (_currentOffscreen == NULL)
        {
            _modelRenderTargets[0].EndDraw();
        }
        else
        {
            _currentOffscreen->GetRenderTarget()->EndDraw();
        }
        _mtlCommandEncoder = nil;
    }
    else
    {
        PostDraw();
    }
}

void CubismRenderer_Metal::BeforeDrawModelRenderTarget()
{
    if(_modelRenderTargets.GetSize() == 0)
    {
        PreDraw();
        return;
    }

    // レンダーターゲットのクリア状況のリセット
    _modelRenderTargets[0].Clear(0.0f, 0.0f, 0.0f, 0.0f);
    _modelRenderTargets[0].BeginDraw(_mtlCommandBuffer);
    _mtlCommandEncoder = _modelRenderTargets[0].GetCommandEncoder();
}

void CubismRenderer_Metal::AfterDrawModelRenderTarget()
{
    if(_modelRenderTargets.GetSize() == 0)
    {
      PostDraw();
        return;
    }

    _modelRenderTargets[0].EndDraw();
    _mtlCommandEncoder = nil;
    PreDraw();
    _copyCommandBuffer->SetCommandBuffer(_mtlCommandBuffer);
    _deviceInfo->GetShader()->SetupShaderProgramForRenderTarget(_copyCommandBuffer, _mtlCommandEncoder, this);
    id <MTLRenderPipelineState> pipelineState = _copyCommandBuffer->GetCommandDraw()->GetRenderPipelineState();
    [_mtlCommandEncoder setRenderPipelineState:pipelineState];
    [_mtlCommandEncoder setVertexBuffer:_copyCommandBuffer->GetVertexBuffer() offset:0 atIndex:MetalVertexInputIndexVertices];
    [_mtlCommandEncoder setVertexBuffer:_copyCommandBuffer->GetUvBuffer() offset:0 atIndex:MetalVertexInputUVs];
    [_mtlCommandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:6
                        indexType:MTLIndexTypeUInt16
                        indexBuffer:_copyCommandBuffer->GetIndexBuffer()
                        indexBufferOffset:0];
    PostDraw();
}

void CubismRenderer_Metal::DoDrawModel()
{
    BeforeDrawModelRenderTarget();

    //------------ クリッピングマスク・バッファ前処理方式の場合 ------------
    if (_drawableClippingManager != NULL)
    {
        // サイズが違う場合はここで作成しなおし
        for (csmInt32 i = 0; i < _drawableClippingManager->GetRenderTextureCount(); ++i)
        {
            if (_drawableMasks[i].GetBufferWidth() != _drawableClippingManager->GetClippingMaskBufferSize().X ||
                _drawableMasks[i].GetBufferHeight() != _drawableClippingManager->GetClippingMaskBufferSize().Y)
            {
                _drawableMasks[i].CreateRenderTarget(
                    _device,
                    _drawableClippingManager->GetClippingMaskBufferSize().X,
                    _drawableClippingManager->GetClippingMaskBufferSize().Y
                );
            }
        }

        if (IsUsingHighPrecisionMask())
        {
            _drawableClippingManager->SetupMatrixForHighPrecision(*GetModel(), false, DrawableObjectType_Drawable);
        }
        else
        {
            _drawableClippingManager->SetupClippingContext(*GetModel(), this, _rendererProfile._lastColorBuffer, _rendererProfile._lastViewport, CubismRenderer::DrawableObjectType_Drawable);
        }
    }

    if(_offscreenClippingManager != NULL)
    {
        // サイズが違う場合はここで作成しなおし
        for (csmInt32 i = 0; i < _offscreenClippingManager->GetRenderTextureCount(); ++i)
        {
            if (_offscreenMasks[i].GetBufferWidth() != _offscreenClippingManager->GetClippingMaskBufferSize().X ||
                _offscreenMasks[i].GetBufferHeight() != _offscreenClippingManager->GetClippingMaskBufferSize().Y)
            {
                _offscreenMasks[i].CreateRenderTarget(
                    _device,
                    _offscreenClippingManager->GetClippingMaskBufferSize().X,
                    _offscreenClippingManager->GetClippingMaskBufferSize().Y
                );
            }
        }

        if (IsUsingHighPrecisionMask())
        {
            _offscreenClippingManager->SetupMatrixForHighPrecision(*GetModel(), false, DrawableObjectType_Offscreen, GetMvpMatrix());
        }
        else
        {
            _offscreenClippingManager->SetupClippingContext(*GetModel(), this, _rendererProfile._lastColorBuffer, _rendererProfile._lastViewport, CubismRenderer::DrawableObjectType_Offscreen);
        }
    }

    // モデルの描画順に従って描画する
    DrawObjectLoop();

    AfterDrawModelRenderTarget();
}

void CubismRenderer_Metal::DrawObjectLoop()
{
    const csmInt32 drawableCount = GetModel()->GetDrawableCount();
    const csmInt32 offscreenCount = GetModel()->GetOffscreenCount();
    const csmInt32 totalCount = drawableCount + offscreenCount;
    const csmInt32* renderOrder = GetModel()->GetRenderOrders();

    _currentOffscreen = NULL;

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

    // Update Vertex / Index buffer.
    for (csmInt32 i = 0; i < drawableCount; ++i)
    {
        csmFloat32* vertices = const_cast<csmFloat32*>(GetModel()->GetDrawableVertices(i));
        Core::csmVector2* uvs = const_cast<Core::csmVector2*>(GetModel()->GetDrawableVertexUvs(i));
        csmUint16* vertexIndices = const_cast<csmUint16*>(GetModel()->GetDrawableVertexIndices(i));
        const csmUint32 vertexCount = GetModel()->GetDrawableVertexCount(i);
        const csmUint32 vertexIndexCount = GetModel()->GetDrawableVertexIndexCount(i);

        _drawableDrawCommandBuffer[i]->UpdateVertexBuffer(vertices, uvs, vertexCount);
        if (vertexIndexCount > 0)
        {
            _drawableDrawCommandBuffer[i]->UpdateIndexBuffer(vertexIndices, vertexIndexCount);
        }
    }

    // 描画
    for (csmInt32 i = 0; i < totalCount; ++i)
    {
        const csmInt32 objectIndex = _sortedObjectsIndexList[i];
        const csmInt32 objectType = _sortedObjectsTypeList[i];
        RenderObject(objectIndex, objectType);
    }

    while (_currentOffscreen != NULL)
    {
        // オフスクリーンが残っている場合は親オフスクリーンへの伝搬を行う
        SubmitDrawToParentOffscreen(_currentOffscreen->GetOffscreenIndex(), DrawableObjectType_Offscreen);
    }
}

void CubismRenderer_Metal::RenderObject(csmInt32 objectIndex, csmInt32 objectType)
{
    switch (objectType)
    {
    case DrawableObjectType_Drawable:
        // Drawable
        DrawDrawable(objectIndex);
        break;
    case DrawableObjectType_Offscreen:
        // Offscreen
        AddOffscreen(objectIndex);
        break;
    default:
        // 不明なタイプはエラーログを出す
        CubismLogError("Unknown drawable type: %d", objectType);
        break;
    }
}

csmBool CubismRenderer_Metal::IsBlendMode(CubismModel* model)
{
    if(!model)
    {
        return GetModel()->IsBlendModeEnabled();
    }
    return model->IsBlendModeEnabled();
}

csmBool CubismRenderer_Metal::IsBlendModeIndex(csmInt32 index, DrawableObjectType drawableObjectType)
{
    csmBlendMode blendMode;
    switch (drawableObjectType)
    {
    case DrawableObjectType_Drawable:
        blendMode = GetModel()->GetDrawableBlendModeType(index);
        break;
    case DrawableObjectType_Offscreen:
        blendMode = GetModel()->GetOffscreenBlendModeType(index);
        break;
    default:
        return false;
    }

    // ブレンドモードなのかチェックする
    const csmInt32 colorBlendType = blendMode.GetColorBlendType();
    const csmInt32 alphaBlendType = blendMode.GetAlphaBlendType();
    if ((colorBlendType == Core::csmColorBlendType_Normal && alphaBlendType == Core::csmAlphaBlendType_Over) ||
        colorBlendType == Core::csmColorBlendType_AddCompatible ||
        colorBlendType == Core::csmColorBlendType_MultiplyCompatible)
    {
        // ブレンドモードじゃない
        return false;
    }

    return true;
}

void CubismRenderer_Metal::DrawDrawable(csmInt32 drawableIndex)
{
    // Drawableが表示状態でなければ処理をパスする
    if (!GetModel()->GetDrawableDynamicFlagIsVisible(drawableIndex) ||
        GetModel()->GetDrawableVertexIndexCount(drawableIndex) <= 0)
    {
        return;
    }

    SubmitDrawToParentOffscreen(drawableIndex, DrawableObjectType_Drawable);

    id<MTLTexture> blendTexture = nil;

    // クリッピングマスク
    CubismClippingContext_Metal* clipContext = (_drawableClippingManager != NULL)
        ? (*_drawableClippingManager->GetClippingContextListForDraw())[drawableIndex]
        : NULL;

    if (clipContext != NULL && IsUsingHighPrecisionMask()) // マスクを書く必要がある
    {
        // 生成したRenderTargetと同じサイズでビューポートを設定
        MTLViewport clipVp = {0, 0, _drawableClippingManager->GetClippingMaskBufferSize().X, _drawableClippingManager->GetClippingMaskBufferSize().Y, 0.0, 1.0};
        if(clipContext->_isUsing) // 書くことになっていた
        {
            EndRenderTarget();
            _drawableMasks[clipContext->_bufferIndex].Clear(1.0f, 1.0f, 1.0f, 1.0f);
            _drawableMasks[clipContext->_bufferIndex].BeginDraw(_mtlCommandBuffer);
            _mtlCommandEncoder = _drawableMasks[clipContext->_bufferIndex].GetCommandEncoder();
            [_mtlCommandEncoder setViewport:clipVp];
        }

        {
            const csmInt32 clipDrawCount = clipContext->_clippingIdCount;
            for (csmInt32 index = 0; index < clipDrawCount; index++)
            {
                const csmInt32 clipDrawIndex = clipContext->_clippingIdList[index];
                CubismCommandBuffer_Metal::DrawCommandBuffer::DrawCommand* drawCommandMask = clipContext->_clippingCommandBufferList->At(index)->GetCommandDraw();

                // 頂点情報が更新されておらず、信頼性がない場合は描画をパスする
                if (!GetModel()->GetDrawableDynamicFlagVertexPositionsDidChange(clipDrawIndex))
                {
                    continue;
                }

                IsCulling(GetModel()->GetDrawableCulling(clipDrawIndex) != 0);

                // Update Vertex / Index buffer.
                {
                    csmFloat32* vertices = const_cast<csmFloat32*>(GetModel()->GetDrawableVertices(clipDrawIndex));
                    Core::csmVector2* uvs = const_cast<Core::csmVector2*>(GetModel()->GetDrawableVertexUvs(clipDrawIndex));
                    csmUint16* vertexIndices = const_cast<csmUint16*>(GetModel()->GetDrawableVertexIndices(clipDrawIndex));
                    const csmUint32 vertexCount = GetModel()->GetDrawableVertexCount(clipDrawIndex);
                    const csmUint32 vertexIndexCount = GetModel()->GetDrawableVertexIndexCount(clipDrawIndex);

                    CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBufferMask = clipContext->_clippingCommandBufferList->At(index);
                    drawCommandBufferMask->UpdateVertexBuffer(vertices, uvs, vertexCount);
                    if (vertexIndexCount > 0)
                    {
                        drawCommandBufferMask->UpdateIndexBuffer(vertexIndices, vertexIndexCount);
                    }

                    if (vertexCount <= 0)
                    {
                        continue;
                    }
                }

                // 今回専用の変換を適用して描く
                // チャンネルも切り替える必要がある(A,R,G,B)
                SetClippingContextBufferForMask(clipContext);

                DrawMeshMetal(clipContext->_clippingCommandBufferList->At(index), _mtlCommandEncoder, *GetModel(), clipDrawIndex, NULL);
            }
        }

        {
            // --- 後処理 ---
            _drawableMasks[clipContext->_bufferIndex].EndDraw();
            _mtlCommandEncoder = nil;
            // レンダーターゲットの切り替えを減らしたいためマスク処理がある場合はついでにここでブレンドモードの判定を行なう
            if (IsBlendModeIndex(drawableIndex, DrawableObjectType_Drawable))
            {
                blendTexture = _currentOffscreen != NULL ?
                    CopyRenderTarget(*_currentOffscreen->GetRenderTarget())->GetColorBuffer() :
                    CopyRenderTarget(_modelRenderTargets[0])->GetColorBuffer();
            }
            BeginRenderTarget();
        }
    }
    else if (IsBlendModeIndex(drawableIndex, DrawableObjectType_Drawable))
    {
        // レンダーターゲットの切り替えを減らしたいためマスク処理がない場合はここでブレンドモードの判定を行なう
        EndRenderTarget();
        blendTexture = _currentOffscreen != NULL ?
            CopyRenderTarget(*_currentOffscreen->GetRenderTarget())->GetColorBuffer() :
            CopyRenderTarget(_modelRenderTargets[0])->GetColorBuffer();
        BeginRenderTarget();
    }

    CubismCommandBuffer_Metal::DrawCommandBuffer::DrawCommand* drawCommandDraw = _drawableDrawCommandBuffer[drawableIndex]->GetCommandDraw();
    _drawableDrawCommandBuffer[drawableIndex]->SetCommandBuffer(_mtlCommandBuffer);

    // クリッピングマスクをセットする
    SetClippingContextBufferForDrawable(clipContext);

    IsCulling(GetModel()->GetDrawableCulling(drawableIndex) != 0);

    DrawMeshMetal(_drawableDrawCommandBuffer[drawableIndex], _mtlCommandEncoder, *GetModel(), drawableIndex, blendTexture);
}

void CubismRenderer_Metal::SubmitDrawToParentOffscreen(csmInt32 objectIndex, DrawableObjectType objectType)
{
    if(_currentOffscreen == NULL || objectIndex == CubismModel::CubismNoIndex_Offscreen)
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
    DrawOffscreen(_currentOffscreen);

    // さらに親のオフスクリーンに伝搬可能なら伝搬する。
   SubmitDrawToParentOffscreen(objectIndex, objectType);
}

void CubismRenderer_Metal::AddOffscreen(csmInt32 offscreenIndex)
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
            SubmitDrawToParentOffscreen(offscreenIndex, DrawableObjectType_Offscreen);
        }
    }

    CubismOffscreenRenderTarget_Metal* offscreen = &_offscreenList.At(offscreenIndex);

    offscreen->SetOffscreenRenderTarget(_device, _deviceInfo->GetOffscreenManager(), _modelRenderTargetWidth, _modelRenderTargetHeight);

    // 現在のオフスクリーンレンダリングターゲットを設定
    CubismOffscreenRenderTarget_Metal* oldOffscreen = offscreen->GetParentPartOffscreen();
    offscreen->SetOldOffscreen(oldOffscreen);
    EndRenderTarget();
    _currentOffscreen = offscreen;
    BeginRenderTarget(true);
}

void CubismRenderer_Metal::DrawOffscreen(CubismOffscreenRenderTarget_Metal* currentOffscreen)
{
    // 親のオフスクリーン、またはモデル描画用ターゲットに描画する
    CubismOffscreenRenderTarget_Metal* parentOffscreen = currentOffscreen->GetOldOffscreen();
    _currentOffscreen = parentOffscreen; // 描画先を親に切り替え

    const csmInt32 offscreenIndex = currentOffscreen->GetOffscreenIndex();

    id<MTLTexture> blendTexture = nil;

    // 確実に切り替える必要があるため閉じる
    currentOffscreen->GetRenderTarget()->EndDraw();
    _mtlCommandEncoder = nil;

    // クリッピングマスク
    CubismClippingContext_Metal* clipContext = (_offscreenClippingManager != NULL) ?
        (*_offscreenClippingManager->GetClippingContextListForOffscreen())[offscreenIndex] :
        NULL;

    if (clipContext != NULL && IsUsingHighPrecisionMask()) // マスクを書く必要がある
    {
        // 生成したRenderTargetと同じサイズでビューポートを設定
        MTLViewport clipVp = {0, 0, _offscreenClippingManager->GetClippingMaskBufferSize().X, _offscreenClippingManager->GetClippingMaskBufferSize().Y, 0.0, 1.0};
        if (clipContext->_isUsing) // 書くことになっていた
        {
            _offscreenMasks[clipContext->_bufferIndex].Clear(1.0f, 1.0f, 1.0f, 1.0f);
            _offscreenMasks[clipContext->_bufferIndex].BeginDraw(_mtlCommandBuffer);
            _mtlCommandEncoder = _offscreenMasks[clipContext->_bufferIndex].GetCommandEncoder();
            [_mtlCommandEncoder setViewport:clipVp];
        }

        {
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

                // Update Vertex / Index buffer.
                {
                    csmFloat32* vertices = const_cast<csmFloat32*>(GetModel()->GetDrawableVertices(clipDrawIndex));
                    Core::csmVector2* uvs = const_cast<Core::csmVector2*>(GetModel()->GetDrawableVertexUvs(clipDrawIndex));
                    csmUint16* vertexIndices = const_cast<csmUint16*>(GetModel()->GetDrawableVertexIndices(clipDrawIndex));
                    const csmUint32 vertexCount = GetModel()->GetDrawableVertexCount(clipDrawIndex);
                    const csmUint32 vertexIndexCount = GetModel()->GetDrawableVertexIndexCount(clipDrawIndex);

                    CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBufferMask = clipContext->_clippingCommandBufferList->At(index);
                    drawCommandBufferMask->UpdateVertexBuffer(vertices, uvs, vertexCount);
                    if (vertexIndexCount > 0)
                    {
                        drawCommandBufferMask->UpdateIndexBuffer(vertexIndices, vertexIndexCount);
                    }

                    if (vertexCount <= 0)
                    {
                        continue;
                    }
                }

                // 今回専用の変換を適用して描く
                // チャンネルも切り替える必要がある(A,R,G,B)
                SetClippingContextBufferForMask(clipContext);

                DrawMeshMetal(clipContext->_clippingCommandBufferList->At(index), _mtlCommandEncoder, *GetModel(), clipDrawIndex, NULL);
            }
        }

        {
            // --- 後処理 ---
            _offscreenMasks[clipContext->_bufferIndex].EndDraw();
            _mtlCommandEncoder = nil;
            // レンダーターゲットの切り替えを減らしたいためマスク処理がある場合はついでにここでブレンドモードの判定を行なう
            if (IsBlendModeIndex(offscreenIndex, DrawableObjectType_Offscreen))
            {
                blendTexture = parentOffscreen != NULL ?
                    CopyRenderTarget(*parentOffscreen->GetRenderTarget())->GetColorBuffer() :
                    CopyRenderTarget(_modelRenderTargets[0])->GetColorBuffer();
            }
            BeginRenderTarget();
        }
    }
    else
    {
        // オフスクリーンは絶対にレンダーターゲットを切り替える必要がある
        if (IsBlendModeIndex(offscreenIndex, DrawableObjectType_Offscreen))
        {
            // レンダーターゲットの切り替えを減らしたいためマスク処理がない場合はここでブレンドモードの判定を行なう
            blendTexture = parentOffscreen != NULL ?
                CopyRenderTarget(*parentOffscreen->GetRenderTarget())->GetColorBuffer() :
                CopyRenderTarget(_modelRenderTargets[0])->GetColorBuffer();
        }
        BeginRenderTarget();
    }

    // クリッピングマスクをセットする
    SetClippingContextBufferForOffscreen(clipContext);
    IsCulling(GetModel()->GetOffscreenCulling(offscreenIndex) != 0);

    DrawOffscreenMetal(_mtlCommandEncoder, *GetModel(), currentOffscreen, blendTexture);

    // 後処理
    currentOffscreen->StopUsingRenderTexture(_deviceInfo->GetOffscreenManager());
    SetClippingContextBufferForOffscreen(NULL);
    SetClippingContextBufferForMask(NULL);
}

void CubismRenderer_Metal::DrawMeshMetal(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer
                                    , id <MTLRenderCommandEncoder> renderEncoder
                                    , const CubismModel& model, const csmInt32 index, id<MTLTexture> blendTexture)
{
#ifndef CSM_DEBUG
    if (_textures[model.GetDrawableTextureIndex(index)] == 0)
    {
        return;    // モデルが参照するテクスチャがバインドされていない場合は描画をスキップする
    }
#endif

    // 裏面描画の有効・無効
    if (IsCulling())
    {
        [renderEncoder setCullMode:MTLCullModeBack];
    }
    else
    {
        [renderEncoder setCullMode:MTLCullModeNone];
    }

    // プリミティブの宣言の頂点の周り順を設定
    [renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];

    // シェーダーセット
    if (IsGeneratingMask())
    {
        _deviceInfo->GetShader()->SetupShaderProgramForMask(drawCommandBuffer, renderEncoder, this, model, index);
    }
    else
    {
        _deviceInfo->GetShader()->SetupShaderProgramForDrawable(drawCommandBuffer, renderEncoder, this, model, index, blendTexture);
    }

    // パイプライン状態オブジェクトを設定する
    id <MTLRenderPipelineState> pipelineState = drawCommandBuffer->GetCommandDraw()->GetRenderPipelineState();
    [renderEncoder setRenderPipelineState:pipelineState];

    // 頂点描画
    {
        csmInt32 indexCount = model.GetDrawableVertexIndexCount(index);
        id <MTLBuffer> indexBuffer = drawCommandBuffer->GetIndexBuffer();
        [renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:indexCount indexType:MTLIndexTypeUInt16
                             indexBuffer:indexBuffer indexBufferOffset:0];
    }

    // 後処理
    SetClippingContextBufferForDrawable(NULL);
    SetClippingContextBufferForMask(NULL);
}

void CubismRenderer_Metal::DrawOffscreenMetal(id <MTLRenderCommandEncoder> renderEncoder, const CubismModel& model, CubismOffscreenRenderTarget_Metal* offscreen, id<MTLTexture> blendTexture)
{
    // 裏面描画の有効・無効
    if (IsCulling())
    {
        [renderEncoder setCullMode:MTLCullModeBack];
    }
    else
    {
        [renderEncoder setCullMode:MTLCullModeNone];
    }

    // プリミティブの宣言の頂点の周り順を設定
    [renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];

    // シェーダーセット
    _deviceInfo->GetShader()->SetupShaderProgramForOffscreen(_offscreenDrawCommandBuffer, renderEncoder, this, model, offscreen, blendTexture);

    id <MTLRenderPipelineState> pipelineState = _offscreenDrawCommandBuffer->GetCommandDraw()->GetRenderPipelineState();
    [renderEncoder setRenderPipelineState:pipelineState];

    [renderEncoder setVertexBuffer:_offscreenDrawCommandBuffer->GetVertexBuffer() offset:0 atIndex:MetalVertexInputIndexVertices];
    [renderEncoder setVertexBuffer:_offscreenDrawCommandBuffer->GetUvBuffer() offset:0 atIndex:MetalVertexInputUVs];


    [renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:6
                        indexType:MTLIndexTypeUInt16
                        indexBuffer:_offscreenDrawCommandBuffer->GetIndexBuffer()
                        indexBufferOffset:0];
}

CubismCommandBuffer_Metal::DrawCommandBuffer* CubismRenderer_Metal::GetDrawCommandBufferData(csmInt32 drawableIndex)
{
    return _drawableDrawCommandBuffer[drawableIndex];
}

id<MTLDevice> CubismRenderer_Metal::GetDevice()
{
    return _device;
}

void CubismRenderer_Metal::SaveProfile()
{
}

void CubismRenderer_Metal::RestoreProfile()
{
}

void CubismRenderer_Metal::BindTexture(csmUint32 modelTextureIndex, id <MTLTexture> texture)
{
    _textures[modelTextureIndex] = texture;
}

const csmMap< csmInt32, id <MTLTexture> >& CubismRenderer_Metal::GetBindedTextures() const
{
    return _textures;
}

id<MTLTexture> CubismRenderer_Metal::GetBindedTextureId(csmInt32 textureId)
{
    return _textures[textureId];
}

void CubismRenderer_Metal::SetDrawbleClippingMaskBufferSize(csmFloat32 width, csmFloat32 height)
{
    if (_drawableClippingManager == NULL)
    {
        return;
    }

    // インスタンス破棄前にレンダーテクスチャの数を保存
    const csmInt32 renderTextureCount = _drawableClippingManager->GetRenderTextureCount();

    // RenderTargetのサイズを変更するためにインスタンスを破棄・再作成する
    CSM_DELETE_SELF(CubismClippingManager_Metal, _drawableClippingManager);

    // ubismClippingManager_Metal内にdeviceを渡す用
    s_InitializeClippingDevice = _device;

    _drawableClippingManager = CSM_NEW CubismClippingManager_Metal();

    _drawableClippingManager->SetClippingMaskBufferSize(width, height);

    _drawableClippingManager->Initialize(
        *GetModel(),
        renderTextureCount,
        DrawableObjectType_Drawable
    );
}
void CubismRenderer_Metal::SetOffscreenClippingMaskBufferSize(csmFloat32 width, csmFloat32 height)
{
    if (_offscreenClippingManager == NULL)
    {
        return;
    }

    // インスタンス破棄前にレンダーテクスチャの数を保存
    const csmInt32 renderTextureCount = _offscreenClippingManager->GetRenderTextureCount();

    // RenderTargetのサイズを変更するためにインスタンスを破棄・再作成する
    CSM_DELETE_SELF(CubismClippingManager_Metal, _offscreenClippingManager);

    _offscreenClippingManager = CSM_NEW CubismClippingManager_Metal();

    _offscreenClippingManager->SetClippingMaskBufferSize(width, height);

    _offscreenClippingManager->Initialize(
        *GetModel(),
        renderTextureCount,
        DrawableObjectType_Offscreen
    );
}

csmInt32 CubismRenderer_Metal::GetDrawableRenderTextureCount() const
{
    return _drawableClippingManager->GetRenderTextureCount();
}

csmInt32 CubismRenderer_Metal::GetOffscreenRenderTextureCount() const
{
    return _offscreenClippingManager->GetRenderTextureCount();
}

CubismVector2 CubismRenderer_Metal::GetDrawableClippingMaskBufferSize() const
{
    return _drawableClippingManager->GetClippingMaskBufferSize();
}

CubismVector2 CubismRenderer_Metal::GetOffscreenClippingMaskBufferSize() const
{
    return _offscreenClippingManager->GetClippingMaskBufferSize();
}

CubismRenderTarget_Metal* CubismRenderer_Metal::GetModelRenderTarget(csmInt32 index)
{
    return &_modelRenderTargets[index];
}

CubismRenderTarget_Metal* CubismRenderer_Metal::GetDrawableMaskBuffer(csmInt32 index)
{
    return &_drawableMasks[index];
}

CubismRenderTarget_Metal* CubismRenderer_Metal::GetOffscreenMaskBuffer(csmInt32 index)
{
    return &_offscreenMasks[index];
}

CubismOffscreenRenderTarget_Metal* CubismRenderer_Metal::GetCurrentOffscreen() const
{
    return _currentOffscreen;
}

void CubismRenderer_Metal::SetClippingContextBufferForMask(CubismClippingContext_Metal* clip)
{
    _clippingContextBufferForMask = clip;
}

CubismClippingContext_Metal* CubismRenderer_Metal::GetClippingContextBufferForMask() const
{
    return _clippingContextBufferForMask;
}

void CubismRenderer_Metal::SetClippingContextBufferForDrawable(CubismClippingContext_Metal* clip)
{
    _clippingContextBufferForDrawable = clip;
}

CubismClippingContext_Metal* CubismRenderer_Metal::GetClippingContextBufferForDrawable() const
{
    return _clippingContextBufferForDrawable;
}

void CubismRenderer_Metal::SetClippingContextBufferForOffscreen(CubismClippingContext_Metal* clip)
{
    _clippingContextBufferForOffscreen = clip;
}

CubismClippingContext_Metal* CubismRenderer_Metal::GetClippingContextBufferForOffscreen() const
{
    return _clippingContextBufferForOffscreen;
}

CubismRenderTarget_Metal* CubismRenderer_Metal::CopyRenderTarget(CubismRenderTarget_Metal& srcBuffer)
{
    CubismRenderTarget_Metal::CopyBuffer(srcBuffer, _modelRenderTargets[1], _mtlCommandBuffer );
    return &_modelRenderTargets[1];
}

const inline csmBool CubismRenderer_Metal::IsGeneratingMask() const
{
    return (GetClippingContextBufferForMask() != NULL);
}

}}}}

//------------ LIVE2D NAMESPACE ------------
