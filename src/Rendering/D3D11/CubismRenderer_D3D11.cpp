/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismRenderer_D3D11.hpp"

#include <cfloat> // FLT_MAX,MIN

#include "Math/CubismMatrix44.hpp"
#include "Type/csmVector.hpp"
#include "Type/csmVectorSort.hpp"
#include "Model/CubismModel.hpp"
#include "CubismShader_D3D11.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

namespace
{
    csmUint32 s_bufferSetNum = 0;           ///< 作成コンテキストの数。モデルロード前に設定されている必要あり。
    ID3D11Device* s_device = NULL;          ///< 使用デバイス。モデルロード前に設定されている必要あり。

    const csmFloat32 ModelRenderTargetVertices[] = {
        //  x,     y,     u,     v
        -1.0f,  1.0f,  0.0f,  0.0f,
         1.0f,  1.0f,  1.0f,  0.0f,
        -1.0f, -1.0f,  0.0f,  1.0f,
         1.0f, -1.0f,  1.0f,  1.0f,
    };

    const csmFloat32 ModelRenderTargetReverseVertices[] = {
        //  x,     y,     u,     v
        -1.0f,  1.0f,  0.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  0.0f,  0.0f,
         1.0f, -1.0f,  1.0f,  0.0f,
    };

    const WORD ModelRenderTargetIndexArray[] = {
        0, 1, 2,
        1, 3, 2,
    };

    csmInt32 GetShaderNames(const csmBlendMode blendMode)
    {
    #define CSM_GET_SHADER_NAME(COLOR, ALPHA) ShaderNames_ ## COLOR ## ALPHA

    #define CSM_SWITCH_ALPHA_BLEND(COLOR) {\
        switch (blendMode.GetAlphaBlendType()) \
        { \
        case Core::csmAlphaBlendType_Over: \
        default: \
            shaderNames = CSM_GET_SHADER_NAME(COLOR, Over); \
            break; \
        case Core::csmAlphaBlendType_Atop: \
            shaderNames = CSM_GET_SHADER_NAME(COLOR, Atop); \
            break; \
        case Core::csmAlphaBlendType_Out: \
            shaderNames = CSM_GET_SHADER_NAME(COLOR, Out); \
            break; \
        case Core::csmAlphaBlendType_ConjointOver: \
            shaderNames = CSM_GET_SHADER_NAME(COLOR, ConjointOver); \
            break; \
        case Core::csmAlphaBlendType_DisjointOver: \
            shaderNames = CSM_GET_SHADER_NAME(COLOR, DisjointOver); \
            break; \
        } \
    }

        csmInt32 shaderNames;

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
                shaderNames = ShaderNames_Normal;
                break;
            case Core::csmAlphaBlendType_Atop:
                shaderNames = CSM_GET_SHADER_NAME(Normal, Atop);
                break;
            case Core::csmAlphaBlendType_Out:
                shaderNames = CSM_GET_SHADER_NAME(Normal, Out);
                break;
            case Core::csmAlphaBlendType_ConjointOver:
                shaderNames = CSM_GET_SHADER_NAME(Normal, ConjointOver);
                break;
            case Core::csmAlphaBlendType_DisjointOver:
                shaderNames = CSM_GET_SHADER_NAME(Normal, DisjointOver);
                break;
            }
        }
        break;
        case Core::csmColorBlendType_AddCompatible:
            // AddCompatible は5.2以前の描画方法を利用する
            shaderNames = ShaderNames_Add;
            break;
        case Core::csmColorBlendType_MultiplyCompatible:
            // MultCompatible は5.2以前の描画方法を利用する
            shaderNames = ShaderNames_Mult;
            break;
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

        return shaderNames;

    #undef CSM_SWITCH_ALPHA_BLEND
    #undef CSM_GET_SHADER_NAME
    }
}

DirectX::XMMATRIX ConvertToD3DX(CubismMatrix44& mtx)
{
    DirectX::XMMATRIX retMtx;
    retMtx.r[0].m128_f32[0] = mtx.GetArray()[ 0];
    retMtx.r[0].m128_f32[1] = mtx.GetArray()[ 1];
    retMtx.r[0].m128_f32[2] = mtx.GetArray()[ 2];
    retMtx.r[0].m128_f32[3] = mtx.GetArray()[ 3];

    retMtx.r[1].m128_f32[0] = mtx.GetArray()[ 4];
    retMtx.r[1].m128_f32[1] = mtx.GetArray()[ 5];
    retMtx.r[1].m128_f32[2] = mtx.GetArray()[ 6];
    retMtx.r[1].m128_f32[3] = mtx.GetArray()[ 7];

    retMtx.r[2].m128_f32[0] = mtx.GetArray()[ 8];
    retMtx.r[2].m128_f32[1] = mtx.GetArray()[ 9];
    retMtx.r[2].m128_f32[2] = mtx.GetArray()[10];
    retMtx.r[2].m128_f32[3] = mtx.GetArray()[11];

    retMtx.r[3].m128_f32[0] = mtx.GetArray()[12];
    retMtx.r[3].m128_f32[1] = mtx.GetArray()[13];
    retMtx.r[3].m128_f32[2] = mtx.GetArray()[14];
    retMtx.r[3].m128_f32[3] = mtx.GetArray()[15];

    return retMtx;
}

/*********************************************************************************************************************
*                                      CubismClippingManager_D3D11
********************************************************************************************************************/
void CubismClippingManager_D3D11::SetupClippingContext(ID3D11Device* device, ID3D11DeviceContext* context, CubismRenderState_D3D11* renderState, CubismModel& model, CubismRenderer_D3D11* renderer, csmInt32 currentRenderTarget, CubismRenderer::DrawableObjectType drawableObjectType)
{
    // 全てのクリッピングを用意する
    // 同じクリップ（複数の場合はまとめて１つのクリップ）を使う場合は１度だけ設定する
    csmInt32 usingClipCount = 0;
    for (csmUint32 clipIndex = 0; clipIndex < _clippingContextListForMask.GetSize(); ++clipIndex)
    {
        // １つのクリッピングマスクに関して
        CubismClippingContext_D3D11* cc = _clippingContextListForMask[clipIndex];

        // このクリップを利用する描画オブジェクト群全体を囲む矩形を計算
        CalcClippedTotalBounds(model, cc, drawableObjectType);

        if (cc->_isUsing)
        {
            ++usingClipCount; //使用中としてカウント
        }
    }

    if (usingClipCount <= 0)
    {
        return;
    }

    // マスク作成処理
    // ビューポートは退避済み
    // 生成したRenderTargetと同じサイズでビューポートを設定
    renderState->SetViewport(context,
            0,
            0,
            static_cast<FLOAT>(_clippingMaskBufferSize.X),
            static_cast<FLOAT>(_clippingMaskBufferSize.Y),
            0.0f, 1.0f);

    // 後の計算のためにインデックスの最初をセット
    switch (drawableObjectType)
    {
    case CubismRenderer::DrawableObjectType_Drawable:
    default:
        _currentMaskBuffer = renderer->GetDrawableMaskBuffer(currentRenderTarget, 0);
        break;
    case CubismRenderer::DrawableObjectType_Offscreen:
        _currentMaskBuffer = renderer->GetOffscreenMaskBuffer(currentRenderTarget, 0);
        break;
    }

    // ----- マスク描画処理 -----
    // マスク用RenderTextureをactiveにセット
    _currentMaskBuffer->BeginDraw(context);

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
    for (csmUint32 clipIndex = 0; clipIndex < _clippingContextListForMask.GetSize(); ++clipIndex)
    {
        // --- 実際に１つのマスクを描く ---
        CubismClippingContext_D3D11* clipContext = _clippingContextListForMask[clipIndex];
        csmRectF* allClippedDrawRect = clipContext->_allClippedDrawRect; //このマスクを使う、全ての描画オブジェクトの論理座標上の囲み矩形
        csmRectF* layoutBoundsOnTex01 = clipContext->_layoutBounds; //この中にマスクを収める
        const csmFloat32 MARGIN = 0.05f;
        const csmBool isRightHanded = true;

        // clipContextに設定したレンダーターゲットをインデックスで取得
        CubismRenderTarget_D3D11* maskBuffer = NULL;
        switch (drawableObjectType)
        {
        case CubismRenderer::DrawableObjectType_Drawable:
        default:
            maskBuffer = renderer->GetDrawableMaskBuffer(currentRenderTarget, clipContext->_bufferIndex);
            break;
        case CubismRenderer::DrawableObjectType_Offscreen:
            maskBuffer = renderer->GetOffscreenMaskBuffer(currentRenderTarget, clipContext->_bufferIndex);
            break;
        }

        // 現在のレンダーテクスチャがclipContextのものと異なる場合
        if (_currentMaskBuffer != maskBuffer)
        {
            _currentMaskBuffer->EndDraw(context);
            _currentMaskBuffer = maskBuffer;

            // マスク用RenderTextureをactiveにセット
            _currentMaskBuffer->BeginDraw(context);
        }

        // モデル座標上の矩形を、適宜マージンを付けて使う
        _tmpBoundsOnModel.SetRect(allClippedDrawRect);
        _tmpBoundsOnModel.Expand(allClippedDrawRect->Width * MARGIN, allClippedDrawRect->Height * MARGIN);
        //########## 本来は割り当てられた領域の全体を使わず必要最低限のサイズがよい
        // シェーダ用の計算式を求める。回転を考慮しない場合は以下のとおり
        // movePeriod' = movePeriod * scaleX + offX [[ movePeriod' = (movePeriod - tmpBoundsOnModel.movePeriod)*scale + layoutBoundsOnTex01.movePeriod ]]
        csmFloat32 scaleX = layoutBoundsOnTex01->Width / _tmpBoundsOnModel.Width;
        csmFloat32 scaleY = layoutBoundsOnTex01->Height / _tmpBoundsOnModel.Height;

        // マスク生成時に使う行列を求める
        CreateMatrixForMask(isRightHanded, layoutBoundsOnTex01, scaleX, scaleY);

        clipContext->_matrixForMask.SetMatrix(_tmpMatrixForMask.GetArray());
        clipContext->_matrixForDraw.SetMatrix(_tmpMatrixForDraw.GetArray());

        if (drawableObjectType == CubismRenderer::DrawableObjectType_Offscreen)
        {
            // clipContext * mvp^-1
            CubismMatrix44 invertMvp = renderer->GetMvpMatrix().GetInvert();
            clipContext->_matrixForDraw.MultiplyByMatrix(&invertMvp);
        }

        const csmInt32 clipDrawCount = clipContext->_clippingIdCount;
        for (csmInt32 i = 0; i < clipDrawCount; ++i)
        {
            const csmInt32 clipDrawIndex = clipContext->_clippingIdList[i];

            // 頂点情報が更新されておらず、信頼性がない場合は描画をパスする
            if (!model.GetDrawableDynamicFlagVertexPositionsDidChange(clipDrawIndex))
            {
                continue;
            }

            renderer->IsCulling(model.GetDrawableCulling(clipDrawIndex) != 0);

            // マスクがクリアされていないなら処理する
            if (!_clearedMaskBufferFlags[clipContext->_bufferIndex])
            {
                // マスクをクリアする
                // (仮仕様) 1が無効（描かれない）領域、0が有効（描かれる）領域。（シェーダーCd*Csで0に近い値をかけてマスクを作る。1をかけると何も起こらない）
                maskBuffer->Clear(context, 1.0f, 1.0f, 1.0f, 1.0f);
                _clearedMaskBufferFlags[clipContext->_bufferIndex] = true;
            }

            // 今回専用の変換を適用して描く
            // チャンネルも切り替える必要がある(A,R,G,B)
            renderer->SetClippingContextBufferForMask(clipContext);
            renderer->DrawMeshDX11(model, clipDrawIndex);
        }
    }

    // --- 後処理 ---
    _currentMaskBuffer->EndDraw(context);
    renderer->SetClippingContextBufferForMask(NULL);
}

/*********************************************************************************************************************
*                                      CubismClippingContext_D3D11
********************************************************************************************************************/
CubismClippingContext_D3D11::CubismClippingContext_D3D11(CubismClippingManager<CubismClippingContext_D3D11, CubismRenderTarget_D3D11>* manager, CubismModel& model, const csmInt32* clippingDrawableIndices, csmInt32 clipCount)
    : CubismClippingContext(clippingDrawableIndices, clipCount)
{
    _isUsing = false;

    _owner = manager;
}

CubismClippingContext_D3D11::~CubismClippingContext_D3D11()
{
}

CubismClippingManager<CubismClippingContext_D3D11, CubismRenderTarget_D3D11>* CubismClippingContext_D3D11::GetClippingManager()
{
    return _owner;
}

/*********************************************************************************************************************
 *                                      CubismRenderer_D3D11
 ********************************************************************************************************************/
CubismRenderer* CubismRenderer::Create(csmUint32 width, csmUint32 height)
{
    return CSM_NEW CubismRenderer_D3D11(width, height);
}

void CubismRenderer::StaticRelease()
{
}

void CubismRenderer_D3D11::SetConstantSettings(csmUint32 bufferSetNum, ID3D11Device* device)
{
    if (bufferSetNum == 0 || device == NULL)
    {
        return;
    }

    s_bufferSetNum = bufferSetNum;
    s_device = device;
}

void CubismRenderer_D3D11::SetDefaultRenderState(csmUint32 width, csmUint32 height)
{
    // NULLは許されず
    CSM_ASSERT(_context != NULL);

    // Zは無効 描画順で制御
    _deviceInfo->GetRenderState()->SetZEnable(_context,
        CubismRenderState_D3D11::Depth_Disable,
        0);

    // ビューポート
    _deviceInfo->GetRenderState()->SetViewport(_context,
        0.0f,
        0.0f,
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f, 1.0f);
}

CubismRenderer_D3D11::CubismRenderer_D3D11(csmUint32 width, csmUint32 height)
    : CubismRenderer(width, height)
    , _device(NULL)
    , _deviceInfo(NULL)
    , _context(NULL)
    , _vertexBuffers(NULL)
    , _indexBuffers(NULL)
    , _constantBuffers(NULL)
    , _copyVertexBuffer(NULL)
    , _offscreenVertexBuffer(NULL)
    , _commandBufferNum(0)
    , _commandBufferCurrent(0)
    , _copyIndexBuffer(NULL)
    , _copyConstantBuffer(NULL)
    , _drawableNum(0)
    , _drawableClippingManager(NULL)
    , _offscreenClippingManager(NULL)
    , _clippingContextBufferForMask(NULL)
    , _clippingContextBufferForDrawable(NULL)
    , _clippingContextBufferForOffscreen(NULL)
{
    // テクスチャ対応マップの容量を確保しておく.
    _textures.PrepareCapacity(32, true);
}

CubismRenderer_D3D11::~CubismRenderer_D3D11()
{
    {
        for (csmInt32 i = 0; i < _modelRenderTargets.GetSize(); ++i)
        {
            _modelRenderTargets[i].DestroyRenderTarget();
        }
        _modelRenderTargets.Clear();
    }

    {
        for (csmUint32 i = 0; i < _drawableMasks.GetSize(); ++i)
        {
            for (csmUint32 j = 0; j < _drawableMasks[i].GetSize(); ++j)
            {
                _drawableMasks[i][j].DestroyRenderTarget();
            }
            _drawableMasks[i].Clear();
        }
        _drawableMasks.Clear();
    }

    {
        for (csmUint32 i = 0; i < _offscreenMasks.GetSize(); ++i)
        {
            for (csmUint32 j = 0; j < _offscreenMasks[i].GetSize(); ++j)
            {
                _offscreenMasks[i][j].DestroyRenderTarget();
            }
            _offscreenMasks[i].Clear();
        }
        _offscreenMasks.Clear();
    }

    ReleaseCommandBuffer();

    if (_offscreenConstantBuffer)
    {
        _offscreenConstantBuffer->Release();
        _offscreenConstantBuffer = NULL;
    }
    if (_copyConstantBuffer)
    {
        _copyConstantBuffer->Release();
        _copyConstantBuffer = NULL;
    }
    // インデックス
    if (_copyIndexBuffer)
    {
        _copyIndexBuffer->Release();
        _copyIndexBuffer = NULL;
    }
    // 頂点
    if (_copyVertexBuffer)
    {
        _copyVertexBuffer->Release();
        _copyVertexBuffer = NULL;
    }
    if (_offscreenVertexBuffer)
    {
        _offscreenVertexBuffer->Release();
        _offscreenVertexBuffer = NULL;
    }

    CSM_DELETE_SELF(CubismClippingManager_D3D11, _drawableClippingManager);
    CSM_DELETE_SELF(CubismClippingManager_D3D11, _offscreenClippingManager);
}

csmBool CubismRenderer_D3D11::OnDeviceChanged()
{
    // 0は許されず ここに来るまでに設定しなければならない
    if (s_bufferSetNum == 0)
    {
        CubismLogError("ContextNum has not been set.");
        CSM_ASSERT(0);
        return false;
    }
    if (s_device == NULL)
    {
        CubismLogError("Device has not been set.");
        CSM_ASSERT(0);
        return false;
    }

    const csmBool isInitialized = _device != NULL && _commandBufferNum != 0 && (_device != s_device || _commandBufferNum != s_bufferSetNum);

    _device = s_device;
    _deviceInfo = CubismDeviceInfo_D3D11::GetDeviceInfo(s_device);
    if (isInitialized)
    {
        // 既に設定されている場合は設定を更新する
        Initialize(GetModel(), s_bufferSetNum);
    }

    return true;
}

void CubismRenderer_D3D11::StartFrame(ID3D11DeviceContext* context)
{
    // フレームで使用するデバイス設定
    _context = context;

    // レンダーステートフレーム先頭処理
    _deviceInfo->GetRenderState()->StartFrame();

    // シェーダ・頂点宣言
    _deviceInfo->GetShader()->SetupShader(_device, _context);
}

void CubismRenderer_D3D11::EndFrame()
{
    _context = NULL;
}

void CubismRenderer_D3D11::Initialize(CubismModel* model)
{
    Initialize(model, 1);
}

void CubismRenderer_D3D11::Initialize(CubismModel* model, csmInt32 maskBufferCount)
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

    _modelRenderTargets.Clear();
    if (model->IsBlendModeEnabled())
    {
        // オフスクリーンの作成
        // TextureBarrierが使えない環境なので2枚作成する
        for (csmInt32 i = 0; i < 2; ++i)
        {
            CubismRenderTarget_D3D11 offscreenRenderTarget;
            offscreenRenderTarget.CreateRenderTarget(_device, _modelRenderTargetWidth, _modelRenderTargetHeight);
            _modelRenderTargets.PushBack(offscreenRenderTarget);
        }
    }

    if (model->IsUsingMasking())
    {
        _drawableClippingManager = CSM_NEW CubismClippingManager_D3D11();  //クリッピングマスク・バッファ前処理方式を初期化
        _drawableClippingManager->Initialize(
            *model,
            maskBufferCount,
            DrawableObjectType_Drawable
        );

        const csmInt32 bufferWidth = _drawableClippingManager->GetClippingMaskBufferSize().X;
        const csmInt32 bufferHeight = _drawableClippingManager->GetClippingMaskBufferSize().Y;

        _drawableMasks.Clear();

        // バックバッファ分確保
        for (csmUint32 i = 0; i < maskBufferCount; ++i)
        {
            csmVector<CubismRenderTarget_D3D11> vector;
            _drawableMasks.PushBack(vector);
            for (csmUint32 j = 0; j < maskBufferCount; ++j)
            {
                CubismRenderTarget_D3D11 renderTarget;
                renderTarget.CreateRenderTarget(_device, bufferWidth, bufferHeight);
                _drawableMasks[i].PushBack(renderTarget);
            }
        }
    }

    if (model->IsUsingMaskingForOffscreen())
    {
        _offscreenClippingManager = CSM_NEW CubismClippingManager_D3D11();  //クリッピングマスク・バッファ前処理方式を初期化
        _offscreenClippingManager->Initialize(
            *model,
            maskBufferCount,
            DrawableObjectType_Offscreen
        );

        const csmInt32 bufferWidth = _offscreenClippingManager->GetClippingMaskBufferSize().X;
        const csmInt32 bufferHeight = _offscreenClippingManager->GetClippingMaskBufferSize().Y;

        _offscreenMasks.Clear();

        // バックバッファ分確保
        for (csmUint32 i = 0; i < maskBufferCount; ++i)
        {
            csmVector<CubismRenderTarget_D3D11> vector;
            _offscreenMasks.PushBack(vector);
            for (csmUint32 j = 0; j < maskBufferCount; ++j)
            {
                CubismRenderTarget_D3D11 renderTarget;
                renderTarget.CreateRenderTarget(_device, bufferWidth, bufferHeight);
                _offscreenMasks[i].PushBack(renderTarget);
            }
        }
    }

    const csmInt32 drawableCount = model->GetDrawableCount();
    const csmInt32 offscreenCount = model->GetOffscreenCount();

    _sortedObjectsIndexList.Resize(drawableCount + offscreenCount, 0);
    _sortedObjectsTypeList.Resize(drawableCount + offscreenCount, DrawableObjectType_Drawable);

    // オフスクリーンの数が0の場合は何もしない
    if (offscreenCount > 0)
    {
        _offscreenList = csmVector<CubismOffscreenRenderTarget_D3D11>(offscreenCount);
        for (csmInt32 offscreenIndex = 0; offscreenIndex < offscreenCount; ++offscreenIndex)
        {
            CubismOffscreenRenderTarget_D3D11 renderTarget;
            renderTarget.SetOffscreenIndex(offscreenIndex);
            _offscreenList.PushBack(renderTarget);
        }

        // 全てのオフスクリーンを登録し終わってから行う
        SetupParentOffscreens(model, offscreenCount);
    }

    CubismRenderer::Initialize(model, maskBufferCount);  //親クラスの処理を呼ぶ

    ReleaseCommandBuffer();

    // コマンドバッファごとに確保
    // 頂点バッファをコンテキスト分
    _vertexBuffers = static_cast<ID3D11Buffer***>(CSM_MALLOC(sizeof(ID3D11Buffer**) * maskBufferCount));
    _indexBuffers = static_cast<ID3D11Buffer***>(CSM_MALLOC(sizeof(ID3D11Buffer**) * maskBufferCount));
    _constantBuffers = static_cast<ID3D11Buffer***>(CSM_MALLOC(sizeof(ID3D11Buffer**) * maskBufferCount));

    // モデルパーツごとに確保
    _drawableNum = model->GetDrawableCount();

    for (csmUint32 buffer = 0; buffer < maskBufferCount; ++buffer)
    {
        // 頂点バッファ
        _vertexBuffers[buffer] = static_cast<ID3D11Buffer**>(CSM_MALLOC(sizeof(ID3D11Buffer*) * _drawableNum));
        _indexBuffers[buffer] = static_cast<ID3D11Buffer**>(CSM_MALLOC(sizeof(ID3D11Buffer*) * _drawableNum));
        _constantBuffers[buffer] = static_cast<ID3D11Buffer**>(CSM_MALLOC(sizeof(ID3D11Buffer*) * _drawableNum));

        for (csmUint32 drawAssign = 0; drawAssign < _drawableNum; ++drawAssign)
        {
            // 頂点
            const csmInt32 vcount = GetModel()->GetDrawableVertexCount(drawAssign);
            if (vcount != 0)
            {
                D3D11_BUFFER_DESC bufferDesc;
                memset(&bufferDesc, 0, sizeof(bufferDesc));
                bufferDesc.ByteWidth = sizeof(CubismVertexD3D11) * vcount;    // 総長 構造体サイズ*個数
                bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
                bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                bufferDesc.MiscFlags = 0;
                bufferDesc.StructureByteStride = 0;

                // 後で頂点を入れるので領域だけ
                if (FAILED(_device->CreateBuffer(&bufferDesc, NULL, &_vertexBuffers[buffer][drawAssign])))
                {
                    CubismLogError("Vertexbuffer create failed : %d", vcount);
                }
            }
            else
            {
                _vertexBuffers[buffer][drawAssign] = NULL;
            }

            // インデックスはここで要素コピーを済ませる
            _indexBuffers[buffer][drawAssign] = NULL;
            const csmInt32 icount = GetModel()->GetDrawableVertexIndexCount(drawAssign);
            if (icount != 0)
            {
                D3D11_BUFFER_DESC bufferDesc;
                memset(&bufferDesc, 0, sizeof(bufferDesc));
                bufferDesc.ByteWidth = sizeof(WORD) * icount;    // 総長 構造体サイズ*個数
                bufferDesc.Usage = D3D11_USAGE_DEFAULT;
                bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                bufferDesc.CPUAccessFlags = 0;
                bufferDesc.MiscFlags = 0;
                bufferDesc.StructureByteStride = 0;

                D3D11_SUBRESOURCE_DATA subResourceData;
                memset(&subResourceData, 0, sizeof(subResourceData));
                subResourceData.pSysMem = GetModel()->GetDrawableVertexIndices(drawAssign);
                subResourceData.SysMemPitch = 0;
                subResourceData.SysMemSlicePitch = 0;

                if (FAILED(_device->CreateBuffer(&bufferDesc, &subResourceData, &_indexBuffers[buffer][drawAssign])))
                {
                    CubismLogError("Indexbuffer create failed : %d", icount);
                }
            }

            _constantBuffers[buffer][drawAssign] = NULL;
            {
                D3D11_BUFFER_DESC bufferDesc;
                memset(&bufferDesc, 0, sizeof(bufferDesc));
                bufferDesc.ByteWidth = sizeof(CubismConstantBufferD3D11);    // 総長 構造体サイズ*個数
                bufferDesc.Usage = D3D11_USAGE_DEFAULT; // 定数バッファに関しては「Map用にDynamic」にしなくともよい
                bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                bufferDesc.CPUAccessFlags = 0;
                bufferDesc.MiscFlags = 0;
                bufferDesc.StructureByteStride = 0;

                if (FAILED(_device->CreateBuffer(&bufferDesc, NULL, &_constantBuffers[buffer][drawAssign])))
                {
                    CubismLogError("ConstantBuffers create failed");
                }
            }
        }
    }

    _commandBufferNum = maskBufferCount;
    _commandBufferCurrent = 0;

    // バッファ作成
    CreateMeshBuffer();
}

void CubismRenderer_D3D11::SetupParentOffscreens(const CubismModel* model, csmInt32 offscreenCount)
{
    CubismOffscreenRenderTarget_D3D11* parentOffscreen;
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
                if (model->GetOffscreenOwnerIndices()[_offscreenList[i].GetOffscreenIndex()] != parentIndex)
                {
                    continue;  //オフスクリーンのインデックスが親と一致しなければスキップ
                }

                parentOffscreen = &_offscreenList[i];
                break;
            }

            if (parentOffscreen != NULL)
            {
                break;  // 親のオフスクリーンが見つかった場合はループを抜ける
            }

            parentIndex = model->GetPartParentPartIndex(parentIndex);
        }

        // 親のオフスクリーンを設定
        _offscreenList[offscreenIndex].SetParentPartOffscreen(parentOffscreen);
    }
}

void CubismRenderer_D3D11::ReleaseCommandBuffer()
{
    for (csmUint32 buffer = 0; buffer < _commandBufferNum; ++buffer)
    {
        for (csmUint32 drawAssign = 0; drawAssign < _drawableNum; ++drawAssign)
        {
            if (_constantBuffers[buffer][drawAssign])
            {
                _constantBuffers[buffer][drawAssign]->Release();
                _constantBuffers[buffer][drawAssign] = NULL;
            }
            // インデックス
            if (_indexBuffers[buffer][drawAssign])
            {
                _indexBuffers[buffer][drawAssign]->Release();
                _indexBuffers[buffer][drawAssign] = NULL;
            }
            // 頂点
            if (_vertexBuffers[buffer][drawAssign])
            {
                _vertexBuffers[buffer][drawAssign]->Release();
                _vertexBuffers[buffer][drawAssign] = NULL;
            }
        }

        CSM_FREE(_constantBuffers[buffer]);
        CSM_FREE(_indexBuffers[buffer]);
        CSM_FREE(_vertexBuffers[buffer]);
    }

    if (_constantBuffers != NULL)
    {
        CSM_FREE(_constantBuffers);
        _constantBuffers = NULL;
    }
    if (_constantBuffers != NULL)
    {
        CSM_FREE(_indexBuffers);
        _constantBuffers = NULL;
    }
    if (_constantBuffers != NULL)
    {
        CSM_FREE(_vertexBuffers);
        _constantBuffers = NULL;
    }
}

void CubismRenderer_D3D11::CreateMeshBuffer()
{
    D3D11_BUFFER_DESC bufferDesc;
    D3D11_SUBRESOURCE_DATA subResourceData;

    // 頂点バッファ
    memset(&bufferDesc, 0, sizeof(bufferDesc));
    bufferDesc.ByteWidth = sizeof(ModelRenderTargetVertices);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;

    memset(&subResourceData, 0, sizeof(subResourceData));
    subResourceData.pSysMem = ModelRenderTargetVertices;

    if (FAILED(_device->CreateBuffer(&bufferDesc, &subResourceData, &_copyVertexBuffer)))
    {
        CubismLogError("_copyVertexBuffer create failed.");
    }

    bufferDesc.ByteWidth = sizeof(ModelRenderTargetReverseVertices);
    memset(&subResourceData, 0, sizeof(subResourceData));
    subResourceData.pSysMem = ModelRenderTargetReverseVertices;

    if (FAILED(_device->CreateBuffer(&bufferDesc, &subResourceData, &_offscreenVertexBuffer)))
    {
        CubismLogError("_offscreenVertexBuffer create failed.");
    }

    // インデックスバッファ
    memset(&bufferDesc, 0, sizeof(bufferDesc));
    bufferDesc.ByteWidth = sizeof(ModelRenderTargetIndexArray);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;

    memset(&subResourceData, 0, sizeof(subResourceData));
    subResourceData.pSysMem = ModelRenderTargetIndexArray;

    if (FAILED(_device->CreateBuffer(&bufferDesc, &subResourceData, &_copyIndexBuffer)))
    {
        CubismLogError("_copyIndexBuffer create failed.");
    }

    // コンスタントバッファ
    memset(&bufferDesc, 0, sizeof(bufferDesc));
    bufferDesc.ByteWidth = sizeof(CubismConstantBufferD3D11);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;

    if (FAILED(_device->CreateBuffer(&bufferDesc, NULL, &_copyConstantBuffer)))
    {
        CubismLogError("_copyConstantBuffer create failed");
    }

    if (FAILED(_device->CreateBuffer(&bufferDesc, NULL, &_offscreenConstantBuffer)))
    {
        CubismLogError("_offscreenConstantBuffer create failed");
    }
}

void CubismRenderer_D3D11::PreDraw()
{
    SetDefaultRenderState(_modelRenderTargetWidth, _modelRenderTargetHeight);
}

void CubismRenderer_D3D11::PostDraw()
{
    // ダブル・トリプルバッファを回す
    ++_commandBufferCurrent;
    if (_commandBufferNum <= _commandBufferCurrent)
    {
        _commandBufferCurrent = 0;
    }
}

void CubismRenderer_D3D11::DoDrawModel()
{
    // NULLは許されず
    CSM_ASSERT(_device != NULL);
    CSM_ASSERT(_context != NULL);

    BeforeDrawModelRenderTarget();

    PreDraw();

    //------------ クリッピングマスク・バッファ前処理方式の場合 ------------
    if (_drawableClippingManager != NULL)
    {
        // サイズが違う場合はここで作成しなおし
        for (csmInt32 i = 0; i < _drawableClippingManager->GetRenderTextureCount(); ++i)
        {
            if (_drawableMasks[_commandBufferCurrent][i].GetBufferWidth() != static_cast<csmUint32>(_drawableClippingManager->GetClippingMaskBufferSize().X) ||
                _drawableMasks[_commandBufferCurrent][i].GetBufferHeight() != static_cast<csmUint32>(_drawableClippingManager->GetClippingMaskBufferSize().Y))
            {
                _drawableMasks[_commandBufferCurrent][i].CreateRenderTarget(_device,
                    static_cast<csmUint32>(_drawableClippingManager->GetClippingMaskBufferSize().X), static_cast<csmUint32>(_drawableClippingManager->GetClippingMaskBufferSize().Y));
            }
        }

        if (IsUsingHighPrecisionMask())
        {
            _drawableClippingManager->SetupMatrixForHighPrecision(*GetModel(), true, DrawableObjectType_Drawable);
        }
        else
        {
            _drawableClippingManager->SetupClippingContext(_device, _context, _deviceInfo->GetRenderState(), *GetModel(), this, _commandBufferCurrent, DrawableObjectType_Drawable);
        }

        if (!IsUsingHighPrecisionMask())
        {
            // ビューポートを元に戻す
            _deviceInfo->GetRenderState()->SetViewport(_context,
                0.0f,
                0.0f,
                static_cast<float>(_modelRenderTargetWidth),
                static_cast<float>(_modelRenderTargetHeight),
                0.0f, 1.0f);
        }
    }

    if (_offscreenClippingManager != NULL)
    {
        // サイズが違う場合はここで作成しなおし
        for (csmInt32 i = 0; i < _offscreenClippingManager->GetRenderTextureCount(); ++i)
        {
            if (_offscreenMasks[_commandBufferCurrent][i].GetBufferWidth() != static_cast<csmUint32>(_offscreenClippingManager->GetClippingMaskBufferSize().X) ||
                _offscreenMasks[_commandBufferCurrent][i].GetBufferHeight() != static_cast<csmUint32>(_offscreenClippingManager->GetClippingMaskBufferSize().Y))
            {
                _offscreenMasks[_commandBufferCurrent][i].CreateRenderTarget(_device,
                    static_cast<csmUint32>(_offscreenClippingManager->GetClippingMaskBufferSize().X), static_cast<csmUint32>(_offscreenClippingManager->GetClippingMaskBufferSize().Y));
            }
        }

        if (IsUsingHighPrecisionMask())
        {
            _offscreenClippingManager->SetupMatrixForHighPrecision(*GetModel(), true, DrawableObjectType_Offscreen, GetMvpMatrix());
        }
        else
        {
            _offscreenClippingManager->SetupClippingContext(_device, _context, _deviceInfo->GetRenderState(), *GetModel(), this, _commandBufferCurrent, DrawableObjectType_Offscreen);
        }

        if (!IsUsingHighPrecisionMask())
        {
            // ビューポートを元に戻す
            _deviceInfo->GetRenderState()->SetViewport(_context,
                0.0f,
                0.0f,
                static_cast<float>(_modelRenderTargetWidth),
                static_cast<float>(_modelRenderTargetHeight),
                0.0f, 1.0f);
        }
    }

    // モデルの描画順に従って描画する
    DrawObjectLoop();

    // ダブルバッファ・トリプルバッファを回す
    PostDraw();

    AfterDrawModelRenderTarget();
}

void CubismRenderer_D3D11::DrawObjectLoop()
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

    // 描画
    for (csmInt32 i = 0; i < totalCount; ++i)
    {
        const csmInt32 objectIndex = _sortedObjectsIndexList[i];
        const csmInt32 objectType = _sortedObjectsTypeList[i];

        RenderObject(objectIndex, objectType);
    }

    // 残ったオフスクリーンを描画する
    while (_currentOffscreen != NULL)
    {
        _currentOffscreen->GetRenderTarget()->EndDraw(_context);
        DrawOffscreen(_currentOffscreen);
        _currentOffscreen = _currentOffscreen->GetParentPartOffscreen();
    }
}

void CubismRenderer_D3D11::RenderObject(const csmInt32 objectIndex, const csmInt32 objectType)
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

void CubismRenderer_D3D11::DrawDrawable(csmInt32 drawableIndex)
{
    // Drawableが表示状態でなければ処理をパスする
    if (!GetModel()->GetDrawableDynamicFlagIsVisible(drawableIndex))
    {
        return;
    }

    FlushOffscreenChainForDrawable(drawableIndex);

    // クリッピングマスクをセットする
    CubismClippingContext_D3D11* clipContext = (_drawableClippingManager != NULL)
        ? (*_drawableClippingManager->GetClippingContextListForDraw())[drawableIndex]
        : NULL;

    if (clipContext != NULL && IsUsingHighPrecisionMask()) // マスクを書く必要がある
    {
        if (clipContext->_isUsing) // 書くことになっていた
        {
            _deviceInfo->GetRenderState()->SetViewport(_context,
                0,
                0,
                static_cast<FLOAT>(_drawableClippingManager->GetClippingMaskBufferSize().X),
                static_cast<FLOAT>(_drawableClippingManager->GetClippingMaskBufferSize().Y),
                0.0f, 1.0f);

            // 正しいレンダーターゲットを持つオフスクリーンサーフェイスバッファを呼ぶ
            CubismRenderTarget_D3D11* currentHighPrecisionMaskColorBuffer = &_drawableMasks[_commandBufferCurrent][clipContext->_bufferIndex];

            currentHighPrecisionMaskColorBuffer->BeginDraw(_context);
            currentHighPrecisionMaskColorBuffer->Clear(_context, 1.0f, 1.0f, 1.0f, 1.0f);

            const csmInt32 clipDrawCount = clipContext->_clippingIdCount;
            for (csmInt32 ctx = 0; ctx < clipDrawCount; ++ctx)
            {
                const csmInt32 clipDrawIndex = clipContext->_clippingIdList[ctx];

                // 頂点情報が更新されておらず、信頼性がない場合は描画をパスする
                if (!GetModel()->GetDrawableDynamicFlagVertexPositionsDidChange(clipDrawIndex))
                {
                    continue;
                }

                IsCulling(GetModel()->GetDrawableCulling(clipDrawIndex) != 0);

                // 今回専用の変換を適用して描く
                // チャンネルも切り替える必要がある(A,R,G,B)
                SetClippingContextBufferForMask(clipContext);
                DrawMeshDX11(*GetModel(), clipDrawIndex);
            }

            {
                // --- 後処理 ---
                currentHighPrecisionMaskColorBuffer->EndDraw(_context);
                SetClippingContextBufferForMask(NULL);

                // ビューポートを元に戻す
                _deviceInfo->GetRenderState()->SetViewport(_context,
                    0.0f,
                    0.0f,
                    static_cast<float>(_modelRenderTargetWidth),
                    static_cast<float>(_modelRenderTargetHeight),
                    0.0f, 1.0f);

                PreDraw(); // バッファをクリアする
            }
        }
    }

    // クリッピングマスクをセットする
    SetClippingContextBufferForDrawable(clipContext);

    IsCulling(GetModel()->GetDrawableCulling(drawableIndex) != 0);

    DrawMeshDX11(*GetModel(), drawableIndex);
}

void CubismRenderer_D3D11::FlushOffscreenChainForDrawable(csmInt32 drawableIndex)
{
    // オフスクリーンごとにDrawableが含まれているか見ていき、含まれていなければオフスクリーンを描画する
    while (_currentOffscreen != NULL)
    {
        const csmInt32 ownerIndex = GetModel()->GetOffscreenOwnerIndices()[_currentOffscreen->GetOffscreenIndex()];
        csmInt32 parentIndex = GetModel()->GetDrawableParentPartIndex(drawableIndex);
        csmBool canDrawOffscreen = true;

        // オフスクリーンの子以降にDrawableが属していないか繰り返し見ていく
        while (parentIndex != CubismModel::CubismNoIndex_Parent)
        {
            if (ownerIndex != parentIndex)
            {
                parentIndex = GetModel()->GetPartParentPartIndex(parentIndex);
            }
            else
            {
                canDrawOffscreen = false;
                break;
            }
        }

        // オフスクリーンの子以降にDrawableが属していなければオフスクリーンを描画する
        if (canDrawOffscreen)
        {
            _currentOffscreen->GetRenderTarget()->EndDraw(_context);
            DrawOffscreen(_currentOffscreen);
            _currentOffscreen = _currentOffscreen->GetParentPartOffscreen();
        }
        else
        {
            break;
        }
    }
}

void CubismRenderer_D3D11::AddOffscreen(csmInt32 offscreenIndex)
{
    CubismOffscreenRenderTarget_D3D11* offscreen = &_offscreenList[offscreenIndex];

    // 切り替えたいオフスクリーンが現在の子じゃないなら描画する
    while (_currentOffscreen != NULL)
    {
        if (_currentOffscreen != offscreen && _currentOffscreen != offscreen->GetParentPartOffscreen())
        {
            _currentOffscreen->GetRenderTarget()->EndDraw(_context);
            DrawOffscreen(_currentOffscreen);
            _currentOffscreen = _currentOffscreen->GetParentPartOffscreen();
        }
        else
        {
            break;
        }
    }

    offscreen->SetOffscreenRenderTarget(_device, _deviceInfo->GetOffscreenManager(), _modelRenderTargetWidth, _modelRenderTargetHeight);

    // 別バッファに描画を開始
    offscreen->GetRenderTarget()->BeginDraw(_context);
    _deviceInfo->GetRenderState()->SetViewport(_context,
        0,
        0,
        static_cast<float>(_modelRenderTargetWidth),
        static_cast<float>(_modelRenderTargetHeight),
        0.0f, 1.0f);
    offscreen->GetRenderTarget()->Clear(_context, 0.0f, 0.0f, 0.0f, 0.0f);

    // 現在のオフスクリーンレンダリングターゲットを設定
    _currentOffscreen = offscreen;
}

void CubismRenderer_D3D11::DrawOffscreen(CubismOffscreenRenderTarget_D3D11* offscreen)
{
    csmInt32 offscreenIndex = offscreen->GetOffscreenIndex();
    // クリッピングマスクをセットする
    CubismClippingContext_D3D11* clipContext = (_offscreenClippingManager != NULL)
        ? (*_offscreenClippingManager->GetClippingContextListForOffscreen())[offscreenIndex]
        : NULL;

    if (clipContext != NULL && IsUsingHighPrecisionMask()) // マスクを書く必要がある
    {
        if (clipContext->_isUsing) // 書くことになっていた
        {
            _deviceInfo->GetRenderState()->SetViewport(_context,
                0,
                0,
                static_cast<FLOAT>(_offscreenClippingManager->GetClippingMaskBufferSize().X),
                static_cast<FLOAT>(_offscreenClippingManager->GetClippingMaskBufferSize().Y),
                0.0f, 1.0f);

            // 正しいレンダーターゲットを持つオフスクリーンサーフェイスバッファを呼ぶ
            CubismRenderTarget_D3D11* currentHighPrecisionMaskColorBuffer = &_offscreenMasks[_commandBufferCurrent][clipContext->_bufferIndex];

            currentHighPrecisionMaskColorBuffer->BeginDraw(_context);
            currentHighPrecisionMaskColorBuffer->Clear(_context, 1.0f, 1.0f, 1.0f, 1.0f);

            const csmInt32 clipDrawCount = clipContext->_clippingIdCount;
            for (csmInt32 ctx = 0; ctx < clipDrawCount; ++ctx)
            {
                const csmInt32 clipDrawIndex = clipContext->_clippingIdList[ctx];

                // 頂点情報が更新されておらず、信頼性がない場合は描画をパスする
                if (!GetModel()->GetDrawableDynamicFlagVertexPositionsDidChange(clipDrawIndex))
                {
                    continue;
                }

                IsCulling(GetModel()->GetDrawableCulling(clipDrawIndex) != 0);

                // 今回専用の変換を適用して描く
                // チャンネルも切り替える必要がある(A,R,G,B)
                SetClippingContextBufferForMask(clipContext);
                DrawMeshDX11(*GetModel(), clipDrawIndex);
            }

            {
                // --- 後処理 ---
                currentHighPrecisionMaskColorBuffer->EndDraw(_context);
                SetClippingContextBufferForMask(NULL);

                // ビューポートを元に戻す
                _deviceInfo->GetRenderState()->SetViewport(_context,
                    0.0f,
                    0.0f,
                    static_cast<float>(_modelRenderTargetWidth),
                    static_cast<float>(_modelRenderTargetHeight),
                    0.0f, 1.0f);

                PreDraw(); // バッファをクリアする
            }
        }
    }

    // クリッピングマスクをセットする
    SetClippingContextBufferForOffscreen(clipContext);

    IsCulling(GetModel()->GetOffscreenCulling(offscreenIndex) != 0);
    DrawOffscreenDX11(*GetModel(), offscreen);
}

void CubismRenderer_D3D11::ExecuteDrawForMask(const CubismModel& model, const csmInt32 index)
{
    // マスク用ブレンドステート
    _deviceInfo->GetRenderState()->SetBlend(_context,
        CubismRenderState_D3D11::Blend_Mask,
        DirectX::XMFLOAT4(0, 0, 0, 0),
        0xffffffff);

    // テクスチャ + サンプラーセット
    SetTextureViewForDrawable(model, index);
    SetSamplerAccordingToAnisotropy();

    // シェーダーセット
    _context->VSSetShader(_deviceInfo->GetShader()->GetVertexShader(ShaderNames_SetupMask), NULL, 0);
    _context->PSSetShader(_deviceInfo->GetShader()->GetPixelShader(ShaderNames_SetupMask), NULL, 0);

    // 定数バッファ
    {
        CubismConstantBufferD3D11 cb;
        memset(&cb, 0, sizeof(cb));

        // 使用するカラーチャンネルを設定
        CubismClippingContext_D3D11* contextBuffer = GetClippingContextBufferForMask();
        SetColorChannel(cb, contextBuffer);

        // 色
        csmRectF* rect = GetClippingContextBufferForMask()->_layoutBounds;
        CubismTextureColor baseColor = {rect->X * 2.0f - 1.0f, rect->Y * 2.0f - 1.0f, rect->GetRight() * 2.0f - 1.0f, rect->GetBottom() * 2.0f - 1.0f};
        CubismTextureColor multiplyColor = model.GetMultiplyColor(index);
        CubismTextureColor screenColor = model.GetScreenColor(index);
        SetColorConstantBuffer(cb, baseColor, multiplyColor, screenColor);

        // プロジェクションMtx
        SetProjectionMatrix(cb, GetClippingContextBufferForMask()->_matrixForMask);

        // Update
        UpdateConstantBuffer(cb, index);
    }

    // トライアングルリスト
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 描画
    DrawDrawableIndexed(model, index);
}

void CubismRenderer_D3D11::ExecuteDrawForDrawable(const CubismModel& model, const csmInt32 index)
{
    // ブレンドステート
    csmBool isBlendMode;
    const csmBlendMode blendMode = model.GetDrawableBlendModeType(index);
    SetBlendMode(blendMode, isBlendMode);

    // テクスチャ+サンプラーセット
    SetTextureViewForDrawable(model, index, isBlendMode);
    SetSamplerAccordingToAnisotropy();

    // シェーダーセット
    SetShaderForDrawable(model, index, blendMode);

    // 定数バッファ
    {
        CubismConstantBufferD3D11 cb;
        memset(&cb, 0, sizeof(cb));

        const csmBool masked = GetClippingContextBufferForDrawable() != NULL;
        if (masked)
        {
            // View座標をClippingContextの座標に変換するための行列を設定
            DirectX::XMMATRIX clip = ConvertToD3DX(GetClippingContextBufferForDrawable()->_matrixForDraw);
            XMStoreFloat4x4(&cb.clipMatrix, DirectX::XMMatrixTranspose(clip));

            // 使用するカラーチャンネルを設定
            CubismClippingContext_D3D11* contextBuffer = GetClippingContextBufferForDrawable();
            SetColorChannel(cb, contextBuffer);
        }

        // 色
        CubismRenderer::CubismTextureColor baseColor;
        if (model.IsBlendModeEnabled())
        {
            // ブレンドモードではモデルカラーは最後に処理するため不透明度のみ対応させる
            csmFloat32 drawableOpacity = model.GetDrawableOpacity(index);
            baseColor.A = drawableOpacity;
            if (IsPremultipliedAlpha())
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
        SetColorConstantBuffer(cb, baseColor, multiplyColor, screenColor);

        // プロジェクションMtx
        SetProjectionMatrix(cb, GetMvpMatrix());

        // Update
        UpdateConstantBuffer(cb, index);
    }

    // トライアングルリスト
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 描画
    DrawDrawableIndexed(model, index);
}

void CubismRenderer_D3D11::ExecuteDrawForOffscreen(const CubismModel& model, CubismOffscreenRenderTarget_D3D11* offscreen)
{
    csmInt32 offscreenIndex = offscreen->GetOffscreenIndex();

    // ブレンドステート
    csmBool isBlendMode;
    const csmBlendMode blendMode = model.GetOffscreenBlendModeType(offscreenIndex);
    SetBlendMode(blendMode, isBlendMode);

    // テクスチャ + サンプラーセット
    SetTextureViewForOffscreen(model, offscreen);
    SetSamplerAccordingToAnisotropy();

    // シェーダーセット
    SetShaderForOffscreen(model, offscreenIndex, blendMode);

    // 定数バッファ
    {
        CubismConstantBufferD3D11 cb;
        memset(&cb, 0, sizeof(cb));

        const csmBool masked = GetClippingContextBufferForOffscreen() != NULL;
        if (masked)
        {
            // View座標をClippingContextの座標に変換するための行列を設定
            DirectX::XMMATRIX clip = ConvertToD3DX(GetClippingContextBufferForOffscreen()->_matrixForDraw);
            XMStoreFloat4x4(&cb.clipMatrix, DirectX::XMMatrixTranspose(clip));

            // 使用するカラーチャンネルを設定
            CubismClippingContext_D3D11* contextBuffer = GetClippingContextBufferForOffscreen();
            SetColorChannel(cb, contextBuffer);
        }

        // 色
        csmFloat32 offscreenOpacity = model.GetOffscreenOpacity(offscreenIndex);
        // PMAなのと不透明度だけを変更したいためすべてOpacityで初期化
        CubismTextureColor baseColor(offscreenOpacity, offscreenOpacity, offscreenOpacity, offscreenOpacity);
        CubismTextureColor multiplyColor = model.GetMultiplyColorOffscreen(offscreenIndex);
        CubismTextureColor screenColor = model.GetScreenColorOffscreen(offscreenIndex);
        SetColorConstantBuffer(cb, baseColor, multiplyColor, screenColor);

        // プロジェクション行列
        CubismMatrix44 mvpMatrix;
        mvpMatrix.LoadIdentity();
        SetProjectionMatrix(cb, mvpMatrix);

        // Update
        _context->UpdateSubresource(_offscreenConstantBuffer, 0, NULL, &cb, 0, 0);

        _context->VSSetConstantBuffers(0, 1, &_offscreenConstantBuffer);
        _context->PSSetConstantBuffers(0, 1, &_offscreenConstantBuffer);
    }

    // トライアングルリスト
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 描画
    UINT strides = sizeof(csmFloat32) * 4;
    UINT offsets = 0;
    _context->IASetVertexBuffers(0, 1, &_offscreenVertexBuffer, &strides, &offsets);
    _context->IASetIndexBuffer(_copyIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    _context->DrawIndexed(sizeof(ModelRenderTargetIndexArray) / sizeof(WORD), 0, 0);
}

void CubismRenderer_D3D11::DrawDrawableIndexed(const CubismModel& model, const csmInt32 index)
{
    UINT strides = sizeof(Csm::CubismVertexD3D11);
    UINT offsets = 0;
    ID3D11Buffer* vertexBuffer = _vertexBuffers[_commandBufferCurrent][index];
    ID3D11Buffer* indexBuffer = _indexBuffers[_commandBufferCurrent][index];
    const csmInt32 indexCount = model.GetDrawableVertexIndexCount(index);

    _context->IASetVertexBuffers(0, 1, &vertexBuffer, &strides, &offsets);
    _context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
    _context->DrawIndexed(indexCount, 0, 0);
}

void CubismRenderer_D3D11::ExecuteDrawForRenderTarget()
{
    // コピー用ブレンドステート
    _deviceInfo->GetRenderState()->SetBlend(_context,
        CubismRenderState_D3D11::Blend_Copy,
        DirectX::XMFLOAT4(0, 0, 0, 0),
        0xffffffff);

    // テクスチャ+サンプラーセット
    ID3D11ShaderResourceView* const viewArray[3] = { _modelRenderTargets[0].GetTextureView(), NULL, NULL };
    _context->PSSetShaderResources(0, 3, viewArray);
    SetSamplerAccordingToAnisotropy();

    // シェーダーセット
    _context->VSSetShader(_deviceInfo->GetShader()->GetVertexShader(ShaderNames_Copy), NULL, 0);
    _context->PSSetShader(_deviceInfo->GetShader()->GetPixelShader(ShaderNames_Copy), NULL, 0);

    // 定数バッファ
    {
        CubismConstantBufferD3D11 cb;
        memset(&cb, 0, sizeof(cb));

        // プロジェクション行列
        CubismMatrix44 matrix;
        matrix.LoadIdentity();
        SetProjectionMatrix(cb, matrix);

        // ベースカラーを適用
        CubismTextureColor baseColor = GetModelColor();
        baseColor.R *= baseColor.A;
        baseColor.G *= baseColor.A;
        baseColor.B *= baseColor.A;
        XMStoreFloat4(&cb.baseColor, DirectX::XMVectorSet(baseColor.R, baseColor.G, baseColor.B, baseColor.A));

        // Update
        _context->UpdateSubresource(_copyConstantBuffer, 0, NULL, &cb, 0, 0);

        _context->VSSetConstantBuffers(0, 1, &_copyConstantBuffer);
        _context->PSSetConstantBuffers(0, 1, &_copyConstantBuffer);
    }

    // トライアングルリスト
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 描画
    UINT strides = sizeof(csmFloat32) * 4;
    UINT offsets = 0;
    _context->IASetVertexBuffers(0, 1, &_copyVertexBuffer, &strides, &offsets);
    _context->IASetIndexBuffer(_copyIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    _context->DrawIndexed(sizeof(ModelRenderTargetIndexArray) / sizeof(WORD), 0, 0);
}

void CubismRenderer_D3D11::DrawMeshDX11(const CubismModel& model, const csmInt32 index)
{
    // デバイス未設定
    if (_device == NULL)
    {
        return;
    }

    // 描画物無し
    if(model.GetDrawableVertexIndexCount(index) == 0)
    {
        return;
    }

    // 描画不要なら描画処理をスキップする
    if (model.GetDrawableOpacity(index) <= 0.0f && !IsGeneratingMask())
    {
        return;
    }

    // モデルが参照するテクスチャがバインドされていない場合は描画をスキップする
    if (GetTextureViewWithIndex(model, index) == NULL)
    {
        return;
    }

    // 裏面描画の有効・無効
    _deviceInfo->GetRenderState()->SetCullMode(_context, (IsCulling() ? CubismRenderState_D3D11::Cull_Ccw : CubismRenderState_D3D11::Cull_None));

    // 頂点バッファにコピー
    {
        const csmInt32 drawableIndex = index;
        const csmInt32 vertexCount = model.GetDrawableVertexCount(index);
        const csmFloat32* vertexArray = model.GetDrawableVertices(index);
        const csmFloat32* uvArray = reinterpret_cast<const csmFloat32*>(model.GetDrawableVertexUvs(index));
        CopyToBuffer(_context, drawableIndex, vertexCount, vertexArray, uvArray);
    }

    // シェーダーセット・描画
    if (IsGeneratingMask())
    {
        ExecuteDrawForMask(model, index);
    }
    else
    {
        ExecuteDrawForDrawable(model, index);
    }

    SetClippingContextBufferForDrawable(NULL);
    SetClippingContextBufferForMask(NULL);
}

void CubismRenderer_D3D11::DrawOffscreenDX11(const CubismModel& model, CubismOffscreenRenderTarget_D3D11* offscreen)
{
    // デバイス未設定
    if (_device == NULL)
    {
        return;
    }

    // 裏面描画の有効・無効
    _deviceInfo->GetRenderState()->SetCullMode(_context, (IsCulling() ? CubismRenderState_D3D11::Cull_Ccw : CubismRenderState_D3D11::Cull_None));

    ExecuteDrawForOffscreen(model, offscreen);

    offscreen->StopUsingRenderTexture(_deviceInfo->GetOffscreenManager());
    SetClippingContextBufferForOffscreen(NULL);
    SetClippingContextBufferForMask(NULL);
}

void CubismRenderer_D3D11::SaveProfile()
{
    // NULLは許されず
    CSM_ASSERT(_device != NULL);
    CSM_ASSERT(_context != NULL);

    // 現在のレンダリングステートをPush
    _deviceInfo->GetRenderState()->SaveCurrentNativeState(_device, _context);
}

void CubismRenderer_D3D11::RestoreProfile()
{
    // NULLは許されず
    CSM_ASSERT(_device != NULL);
    CSM_ASSERT(_context != NULL);

    // SaveCurrentNativeStateと対
    _deviceInfo->GetRenderState()->RestoreNativeState(_device, _context);
}

void CubismRenderer_D3D11::BeforeDrawModelRenderTarget()
{
    if (_modelRenderTargets.GetSize() == 0)
    {
        return;
    }

    // オフスクリーンのバッファのサイズが違う場合は作り直し
    for (csmInt32 i = 0; i < _modelRenderTargets.GetSize(); ++i)
    {
        if (_modelRenderTargets[i].GetBufferWidth() != _modelRenderTargetWidth || _modelRenderTargets[i].GetBufferHeight() != _modelRenderTargetHeight)
        {
            _modelRenderTargets[i].CreateRenderTarget(_device, _modelRenderTargetWidth, _modelRenderTargetHeight);
        }
    }

    // 別バッファに描画を開始
    _modelRenderTargets[0].BeginDraw(_context);
    _modelRenderTargets[0].Clear(_context, 0.0f, 0.0f, 0.0f, 0.0f);
}

void CubismRenderer_D3D11::AfterDrawModelRenderTarget()
{
    if (_modelRenderTargets.GetSize() == 0)
    {
        return;
    }

    // 元のバッファに描画する
    _modelRenderTargets[0].EndDraw(_context);

    ExecuteDrawForRenderTarget();
}

void CubismRenderer_D3D11::BindTexture(csmUint32 modelTextureAssign, ID3D11ShaderResourceView* textureView)
{
    _textures[modelTextureAssign] = textureView;
}

const csmMap<csmInt32, ID3D11ShaderResourceView*>& CubismRenderer_D3D11::GetBindedTextures() const
{
    return _textures;
}

void CubismRenderer_D3D11::SetDrawableClippingMaskBufferSize(csmFloat32 width, csmFloat32 height)
{
    if (_drawableClippingManager == NULL)
    {
        return;
    }

    // インスタンス破棄前にレンダーテクスチャの数を保存
    const csmInt32 renderTextureCount = _drawableClippingManager->GetRenderTextureCount();

    // RenderTargetのサイズを変更するためにインスタンスを破棄・再作成する
    CSM_DELETE_SELF(CubismClippingManager_D3D11, _drawableClippingManager);

    _drawableClippingManager = CSM_NEW CubismClippingManager_D3D11();

    _drawableClippingManager->SetClippingMaskBufferSize(width, height);

    _drawableClippingManager->Initialize(
        *GetModel(),
        renderTextureCount,
        DrawableObjectType_Drawable
    );
}

csmInt32 CubismRenderer_D3D11::GetDrawableRenderTextureCount() const
{
    return _drawableClippingManager->GetRenderTextureCount();
}

CubismVector2 CubismRenderer_D3D11::GetDrawableClippingMaskBufferSize() const
{
    return _drawableClippingManager->GetClippingMaskBufferSize();
}

void CubismRenderer_D3D11::SetOffscreenClippingMaskBufferSize(csmFloat32 width, csmFloat32 height)
{
    if (_offscreenClippingManager == NULL)
    {
        return;
    }

    // インスタンス破棄前にレンダーテクスチャの数を保存
    const csmInt32 renderTextureCount = _offscreenClippingManager->GetRenderTextureCount();

    // RenderTargetのサイズを変更するためにインスタンスを破棄・再作成する
    CSM_DELETE_SELF(CubismClippingManager_D3D11, _offscreenClippingManager);

    _offscreenClippingManager = CSM_NEW CubismClippingManager_D3D11();

    _offscreenClippingManager->SetClippingMaskBufferSize(width, height);

    _offscreenClippingManager->Initialize(
        *GetModel(),
        renderTextureCount,
        DrawableObjectType_Offscreen
    );
}

csmInt32 CubismRenderer_D3D11::GetOffscreenRenderTextureCount() const
{
    return _offscreenClippingManager->GetRenderTextureCount();
}

CubismVector2 CubismRenderer_D3D11::GetOffscreenClippingMaskBufferSize() const
{
    return _offscreenClippingManager->GetClippingMaskBufferSize();
}

CubismRenderTarget_D3D11* CubismRenderer_D3D11::GetDrawableMaskBuffer(csmUint32 backbufferNum, csmInt32 index)
{
    return &_drawableMasks[backbufferNum][index];
}

CubismRenderTarget_D3D11* CubismRenderer_D3D11::GetOffscreenMaskBuffer(csmUint32 backbufferNum, csmInt32 index)
{
    return &_offscreenMasks[backbufferNum][index];
}

CubismOffscreenRenderTarget_D3D11* CubismRenderer_D3D11::GetCurrentOffscreen() const
{
    return _currentOffscreen;
}

ID3D11ShaderResourceView* CubismRenderer_D3D11::CopyModelRenderTarget()
{
    return CopyRenderTarget(_modelRenderTargets[0]);
}

ID3D11ShaderResourceView* CubismRenderer_D3D11::CopyRenderTarget(CubismRenderTarget_D3D11& srcBuffer)
{
    CubismRenderTarget_D3D11::CopyBuffer(_context, srcBuffer, _modelRenderTargets[1]);
    return _modelRenderTargets[1].GetTextureView();
}

void CubismRenderer_D3D11::SetClippingContextBufferForDrawable(CubismClippingContext_D3D11* clip)
{
    _clippingContextBufferForDrawable = clip;
}

CubismClippingContext_D3D11* CubismRenderer_D3D11::GetClippingContextBufferForDrawable() const
{
    return _clippingContextBufferForDrawable;
}

void CubismRenderer_D3D11::SetClippingContextBufferForOffscreen(CubismClippingContext_D3D11* clip)
{
    _clippingContextBufferForOffscreen = clip;
}

CubismClippingContext_D3D11* CubismRenderer_D3D11::GetClippingContextBufferForOffscreen() const
{
    return _clippingContextBufferForOffscreen;
}

void CubismRenderer_D3D11::SetClippingContextBufferForMask(CubismClippingContext_D3D11* clip)
{
    _clippingContextBufferForMask = clip;
}

CubismClippingContext_D3D11* CubismRenderer_D3D11::GetClippingContextBufferForMask() const
{
    return _clippingContextBufferForMask;
}

void CubismRenderer_D3D11::CopyToBuffer(ID3D11DeviceContext* renderContext, csmInt32 drawAssign, const csmInt32 vcount, const csmFloat32* varray, const csmFloat32* uvarray)
{
    // CubismVertexD3D11の書き込み
    if (_vertexBuffers[_commandBufferCurrent][drawAssign])
    {
        D3D11_MAPPED_SUBRESOURCE subRes;
        if (SUCCEEDED(renderContext->Map(_vertexBuffers[_commandBufferCurrent][drawAssign], 0, D3D11_MAP_WRITE_DISCARD, 0, &subRes)))
        {
            CubismVertexD3D11* lockPointer = reinterpret_cast<CubismVertexD3D11*>(subRes.pData);
            for (csmInt32 ct = 0; ct < vcount * 2; ct += 2)
            {// モデルデータからのコピー
                lockPointer[ct / 2].x = varray[ct + 0];
                lockPointer[ct / 2].y = varray[ct + 1];
                lockPointer[ct / 2].u = uvarray[ct + 0];
                lockPointer[ct / 2].v = uvarray[ct + 1];
            }
            renderContext->Unmap(_vertexBuffers[_commandBufferCurrent][drawAssign], 0);
        }
    }
}

ID3D11ShaderResourceView* CubismRenderer_D3D11::GetTextureViewWithIndex(const CubismModel& model, const csmInt32 index)
{
    ID3D11ShaderResourceView* result = NULL;
    const csmInt32 textureIndex = model.GetDrawableTextureIndex(index);
    if (textureIndex >= 0)
    {
        result = _textures[textureIndex];
    }
    return result;
}

void CubismRenderer_D3D11::SetBlendState(const CubismBlendMode blendMode) const
{
    switch (blendMode)
    {
    case CubismBlendMode_Normal:
    default:
        _deviceInfo->GetRenderState()->SetBlend(_context,
            CubismRenderState_D3D11::Blend_Normal,
            DirectX::XMFLOAT4(0, 0, 0, 0),
            0xffffffff);
        break;

    case CubismBlendMode_Additive:
        _deviceInfo->GetRenderState()->SetBlend(_context,
            CubismRenderState_D3D11::Blend_Add,
            DirectX::XMFLOAT4(0, 0, 0, 0),
            0xffffffff);
        break;

    case CubismBlendMode_Multiplicative:
        _deviceInfo->GetRenderState()->SetBlend(_context,
            CubismRenderState_D3D11::Blend_Mult,
            DirectX::XMFLOAT4(0, 0, 0, 0),
            0xffffffff);
        break;
    }
}

void CubismRenderer_D3D11::SetBlendMode(csmBlendMode blendMode, csmBool& isBlendMode) const
{
    isBlendMode = false;

    if (blendMode.GetColorBlendType() == Core::csmColorBlendType_Normal && blendMode.GetAlphaBlendType() == Core::csmAlphaBlendType_Over)
    {
        SetBlendState(CubismBlendMode_Normal);
    }
    else if (blendMode.GetColorBlendType() == Core::csmColorBlendType_AddCompatible)
    {
        SetBlendState(CubismBlendMode_Additive);
    }
    else if (blendMode.GetColorBlendType() == Core::csmColorBlendType_MultiplyCompatible)
    {
        SetBlendState(CubismBlendMode_Multiplicative);
    }
    else
    {
        _deviceInfo->GetRenderState()->SetBlend(_context,
            CubismRenderState_D3D11::Blend_ColorAlphaBlend,
            DirectX::XMFLOAT4(0, 0, 0, 0),
            0xffffffff);

        isBlendMode = true;
    }
}

void CubismRenderer_D3D11::SetShaderForDrawable(const CubismModel& model, const csmInt32 index, const csmBlendMode blendMode)
{
    const csmBool masked = GetClippingContextBufferForDrawable() != NULL;
    const csmBool premult = IsPremultipliedAlpha();
    const csmBool invertedMask = model.GetDrawableInvertedMask(index);
    const csmInt32 offset = (masked ? (invertedMask ? 2 : 1) : 0) + (premult ? 3 : 0);

    csmInt32 shaderNames = GetShaderNames(blendMode);

    shaderNames += offset;
    _context->VSSetShader(_deviceInfo->GetShader()->GetVertexShader(shaderNames), NULL, 0);
    _context->PSSetShader(_deviceInfo->GetShader()->GetPixelShader(shaderNames), NULL, 0);
}

void CubismRenderer_D3D11::SetShaderForOffscreen(const CubismModel& model, const csmInt32 index, csmBlendMode blendMode)
{
    const csmBool masked = GetClippingContextBufferForOffscreen() != NULL;
    const csmBool invertedMask = model.GetOffscreenInvertedMask(index);
    // オフスクリーンはPMAを使うため 3 を足す
    const csmInt32 offset = (masked ? (invertedMask ? 2 : 1) : 0) + 3;

    csmInt32 shaderNames = GetShaderNames(blendMode);

    shaderNames += offset;
    _context->VSSetShader(_deviceInfo->GetShader()->GetVertexShader(shaderNames), NULL, 0);
    _context->PSSetShader(_deviceInfo->GetShader()->GetPixelShader(shaderNames), NULL, 0);
}

void CubismRenderer_D3D11::SetTextureViewForDrawable(const CubismModel& model, const csmInt32 index, csmBool isBlendMode)
{
    const csmBool masked = GetClippingContextBufferForDrawable() != NULL;
    const csmBool drawing = !IsGeneratingMask();

    ID3D11ShaderResourceView* textureView = GetTextureViewWithIndex(model, index);
    ID3D11ShaderResourceView* maskView = (masked && drawing ? _drawableMasks[_commandBufferCurrent][GetClippingContextBufferForDrawable()->_bufferIndex].GetTextureView() : NULL);
    ID3D11ShaderResourceView* blendView = isBlendMode ?
                                              _currentOffscreen != NULL ?
                                              CopyRenderTarget(*_currentOffscreen->GetRenderTarget()) :
                                              CopyModelRenderTarget() :
                                          NULL;
    ID3D11ShaderResourceView* const viewArray[3] = { textureView, maskView, blendView };
    _context->PSSetShaderResources(0, 3, viewArray);
}

void CubismRenderer_D3D11::SetTextureViewForOffscreen(const CubismModel& model, const CubismOffscreenRenderTarget_D3D11* offscreen)
{
    const csmBool masked = GetClippingContextBufferForOffscreen() != NULL;
    const csmBool drawing = !IsGeneratingMask();

    ID3D11ShaderResourceView* textureView = offscreen->GetRenderTarget()->GetTextureView();
    int a = masked && drawing ? GetClippingContextBufferForOffscreen()->_bufferIndex : -1;
    ID3D11ShaderResourceView* maskView = (masked && drawing ? _offscreenMasks[_commandBufferCurrent][GetClippingContextBufferForOffscreen()->_bufferIndex].GetTextureView() : NULL);
    ID3D11ShaderResourceView* blendView = offscreen->GetParentPartOffscreen() != NULL ?
        CopyRenderTarget(*offscreen->GetParentPartOffscreen()->GetRenderTarget()) :
        CopyModelRenderTarget();
    ID3D11ShaderResourceView* const viewArray[3] = { textureView, maskView, blendView };
    _context->PSSetShaderResources(0, 3, viewArray);
}

void CubismRenderer_D3D11::SetColorConstantBuffer(CubismConstantBufferD3D11& cb, CubismTextureColor& baseColor, CubismTextureColor& multiplyColor, CubismTextureColor& screenColor)
{
    XMStoreFloat4(&cb.baseColor, DirectX::XMVectorSet(baseColor.R, baseColor.G, baseColor.B, baseColor.A));
    XMStoreFloat4(&cb.multiplyColor, DirectX::XMVectorSet(multiplyColor.R, multiplyColor.G, multiplyColor.B, multiplyColor.A));
    XMStoreFloat4(&cb.screenColor, DirectX::XMVectorSet(screenColor.R, screenColor.G, screenColor.B, screenColor.A));
}

void CubismRenderer_D3D11::SetColorChannel(CubismConstantBufferD3D11& cb, CubismClippingContext_D3D11* contextBuffer)
{
    const csmInt32 channelIndex = contextBuffer->_layoutChannelIndex;
    CubismRenderer::CubismTextureColor* colorChannel = contextBuffer->GetClippingManager()->GetChannelFlagAsColor(channelIndex);
    XMStoreFloat4(&cb.channelFlag, DirectX::XMVectorSet(colorChannel->R, colorChannel->G, colorChannel->B, colorChannel->A));
}

void CubismRenderer_D3D11::SetProjectionMatrix(CubismConstantBufferD3D11& cb, CubismMatrix44 matrix)
{
    DirectX::XMMATRIX proj = ConvertToD3DX(matrix);
    XMStoreFloat4x4(&cb.projectMatrix, DirectX::XMMatrixTranspose(proj));
}

void CubismRenderer_D3D11::UpdateConstantBuffer(CubismConstantBufferD3D11& cb, csmInt32 index)
{
    ID3D11Buffer* constantBuffer = _constantBuffers[_commandBufferCurrent][index];
    _context->UpdateSubresource(constantBuffer, 0, NULL, &cb, 0, 0);

    _context->VSSetConstantBuffers(0, 1, &constantBuffer);
    _context->PSSetConstantBuffers(0, 1, &constantBuffer);
}

void CubismRenderer_D3D11::SetSamplerAccordingToAnisotropy()
{
    if (GetAnisotropy() >= 1.0f)
    {
        _deviceInfo->GetRenderState()->SetSampler(_context, CubismRenderState_D3D11::Sampler_Anisotropy, GetAnisotropy(), _device);
    }
    else
    {
        _deviceInfo->GetRenderState()->SetSampler(_context, CubismRenderState_D3D11::Sampler_Normal);
    }
}

const csmBool inline CubismRenderer_D3D11::IsGeneratingMask() const
{
    return (GetClippingContextBufferForMask() != NULL);
}

}}}}

//------------ LIVE2D NAMESPACE ------------
