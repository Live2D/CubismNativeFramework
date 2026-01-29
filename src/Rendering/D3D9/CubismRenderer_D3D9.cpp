/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismRenderer_D3D9.hpp"

#include <cfloat> // FLT_MAX,MIN

#include "Math/CubismMatrix44.hpp"
#include "Type/csmVectorSort.hpp"
#include "Model/CubismModel.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

namespace {
    csmUint32         s_bufferSetNum = 0;         ///< 作成コンテキストの数。モデルロード前に毎回設定されている必要あり。
    LPDIRECT3DDEVICE9 s_device = NULL;            ///< 使用デバイス。モデルロード前に毎回設定されている必要あり。
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

    /**
     * コピーする際に使用する
     */
    const csmUint32 ModelRenderTargetIndexArray[] = {
        0, 1, 2,
        1, 3, 2
    };

    csmString GetTechniqueName(const csmBool masked, const csmBool premult, const csmBool invertedMask, const csmBool isBlendMode, const csmBlendMode blendMode)
    {
        csmString techniqueName;

        if (masked)
        {
            if (premult)
            {
                if (invertedMask)
                {
                    techniqueName = "ShaderNames_MaskedInvertedPremultipliedAlpha";
                }
                else
                {
                    techniqueName = "ShaderNames_MaskedPremultipliedAlpha";
                }
            }
            else
            {
                if (invertedMask)
                {
                    techniqueName = "ShaderNames_MaskedInverted";
                }
                else
                {
                    techniqueName = "ShaderNames_Masked";
                }
            }
        }
        else
        {
            if (premult)
            {
                techniqueName = "ShaderNames_NormalPremultipliedAlpha";
            }
            else
            {
                techniqueName = "ShaderNames_Normal";
            }
        }
        if (isBlendMode)
        {
            techniqueName += "Blend_";
            techniqueName += blendMode.GetColorBlendTypeString();
            techniqueName += blendMode.GetAlphaBlendTypeString();
        }

        return techniqueName;
    }
}

D3DXMATRIX ConvertToD3DX(CubismMatrix44& mtx)
{
    D3DXMATRIX retMtx;
    retMtx._11 = mtx.GetArray()[ 0];
    retMtx._12 = mtx.GetArray()[ 1];
    retMtx._13 = mtx.GetArray()[ 2];
    retMtx._14 = mtx.GetArray()[ 3];

    retMtx._21 = mtx.GetArray()[ 4];
    retMtx._22 = mtx.GetArray()[ 5];
    retMtx._23 = mtx.GetArray()[ 6];
    retMtx._24 = mtx.GetArray()[ 7];

    retMtx._31 = mtx.GetArray()[ 8];
    retMtx._32 = mtx.GetArray()[ 9];
    retMtx._33 = mtx.GetArray()[10];
    retMtx._34 = mtx.GetArray()[11];

    retMtx._41 = mtx.GetArray()[12];
    retMtx._42 = mtx.GetArray()[13];
    retMtx._43 = mtx.GetArray()[14];
    retMtx._44 = mtx.GetArray()[15];

    return retMtx;
}

/*********************************************************************************************************************
*                                      CubismClippingManager_DX9
********************************************************************************************************************/
void CubismClippingManager_DX9::SetupClippingContext(LPDIRECT3DDEVICE9 device, CubismRenderState_D3D9* renderState, CubismModel& model, CubismRenderer_D3D9* renderer, csmInt32 currentRenderTarget, CubismRenderer::DrawableObjectType drawableObjectType)
{
    // 全てのクリッピングを用意する
    // 同じクリップ（複数の場合はまとめて１つのクリップ）を使う場合は１度だけ設定する
    csmInt32 usingClipCount = 0;
    for (csmUint32 clipIndex = 0; clipIndex < _clippingContextListForMask.GetSize(); clipIndex++)
    {
        // １つのクリッピングマスクに関して
        CubismClippingContext_D3D9* cc = _clippingContextListForMask[clipIndex];

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
    renderState->SetViewport(
            device,
            0,
            0,
            _clippingMaskBufferSize.X,
            _clippingMaskBufferSize.Y,
            0.0f, 1.0f);

    // 後の計算のためにインデックスの最初をセットする
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
    _currentMaskBuffer->BeginDraw(device);

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
        CubismClippingContext_D3D9* clipContext = _clippingContextListForMask[clipIndex];
        csmRectF* allClippedDrawRect = clipContext->_allClippedDrawRect; //このマスクを使う、全ての描画オブジェクトの論理座標上の囲み矩形
        csmRectF* layoutBoundsOnTex01 = clipContext->_layoutBounds; //この中にマスクを収める
        const csmFloat32 MARGIN = 0.05f;
        const csmBool isRightHanded = true;

        // clipContextに設定したレンダーターゲットをインデックスで取得
        CubismRenderTarget_D3D9* maskBuffer = NULL;
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

        // 現在のレンダーターゲットがclipContextのものと異なる場合
        if (_currentMaskBuffer != maskBuffer)
        {
            _currentMaskBuffer->EndDraw(device);
            _currentMaskBuffer = maskBuffer;

            // マスク用RenderTextureをactiveにセット
            _currentMaskBuffer->BeginDraw(device);
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
        for (csmInt32 i = 0; i < clipDrawCount; i++)
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
                maskBuffer->Clear(device, 1.0f, 1.0f, 1.0f, 1.0f);
                _clearedMaskBufferFlags[clipContext->_bufferIndex] = true;
            }

            // 今回専用の変換を適用して描く
            // チャンネルも切り替える必要がある(A,R,G,B)
            renderer->SetClippingContextBufferForMask(clipContext);
            renderer->DrawMeshDX9(model, clipDrawIndex);
        }
    }

    _currentMaskBuffer->EndDraw(device);
    renderer->SetClippingContextBufferForMask(NULL);
}

/*********************************************************************************************************************
*                                      CubismClippingContext_D3D9
********************************************************************************************************************/
CubismClippingContext_D3D9::CubismClippingContext_D3D9(CubismClippingManager<CubismClippingContext_D3D9, CubismRenderTarget_D3D9>* manager, CubismModel& model, const csmInt32* clippingDrawableIndices, csmInt32 clipCount)
    : CubismClippingContext(clippingDrawableIndices, clipCount)
{
    _owner = manager;
}

CubismClippingContext_D3D9::~CubismClippingContext_D3D9()
{
}

CubismClippingManager<CubismClippingContext_D3D9, CubismRenderTarget_D3D9>* CubismClippingContext_D3D9::GetClippingManager()
{
    return _owner;
}

/*********************************************************************************************************************
 *                                      CubismRenderer_D3D9
 ********************************************************************************************************************/
CubismRenderer* CubismRenderer::Create(csmUint32 width, csmUint32 height)
{
    return CSM_NEW CubismRenderer_D3D9(width, height);
}

void CubismRenderer::StaticRelease()
{
}

void CubismRenderer_D3D9::SetConstantSettings(csmUint32 bufferSetNum, LPDIRECT3DDEVICE9 device)
{
    if (bufferSetNum == 0 || device == NULL)
    {
        return;
    }

    s_bufferSetNum = bufferSetNum;
    s_device = device;
}

void CubismRenderer_D3D9::SetDefaultRenderState(csmUint32 width, csmUint32 height)
{
    // Zは無効 描画順で制御
    _deviceInfo->GetRenderState()->SetZEnable(_device,
        D3DZB_FALSE,
        D3DCMP_LESS);

    // ビューポート
    _deviceInfo->GetRenderState()->SetViewport(_device,
        0,
        0,
        width,
        height,
        0.0f, 1.0);

    // カラーマスク
    _deviceInfo->GetRenderState()->SetColorMask(_device,
        D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED);

    // カリング
    _deviceInfo->GetRenderState()->SetCullMode(_device,
        D3DCULL_CW); // CWを消す

    // フィルタ
    _deviceInfo->GetRenderState()->SetTextureFilter(_device,
        0, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTADDRESS_WRAP, D3DTADDRESS_WRAP
    );
}

CubismRenderer_D3D9::CubismRenderer_D3D9(csmUint32 width, csmUint32 height)
    : CubismRenderer(width, height)
    , _device(NULL)
    , _deviceInfo(NULL)
    , _drawableNum(0)
    , _vertexStore(NULL)
    , _indexStore(NULL)
    , _commandBufferNum(0)
    , _commandBufferCurrent(0)
    , _copyVertexBuffer(NULL)
    , _offscreenVertexBuffer(NULL)
    , _copyIndexBuffer(NULL)
    , _drawableClippingManager(NULL)
    , _offscreenClippingManager(NULL)
    , _clippingContextBufferForMask(NULL)
    , _clippingContextBufferForDrawable(NULL)
    , _clippingContextBufferForOffscreen(NULL)
    , _currentOffscreen(NULL)
{
    // テクスチャ対応マップの容量を確保しておく.
    _textures.PrepareCapacity(32, true);
}

CubismRenderer_D3D9::~CubismRenderer_D3D9()
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

    const csmInt32 drawableCount = _drawableNum; //GetModel()->GetDrawableCount();

    for (csmInt32 drawAssign = 0; drawAssign < drawableCount; drawAssign++)
    {
        // インデックス
        if (_indexStore[drawAssign])
        {
            CSM_FREE(_indexStore[drawAssign]);
        }
        // 頂点
        if (_vertexStore[drawAssign])
        {
            CSM_FREE(_vertexStore[drawAssign]);
        }
    }

    {
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

        if (_copyIndexBuffer)
        {
            _copyIndexBuffer->Release();
            _copyIndexBuffer = nullptr;
        }
    }

    CSM_FREE(_indexStore);
    CSM_FREE(_vertexStore);
    _indexStore = NULL;
    _vertexStore = NULL;

    CSM_DELETE_SELF(CubismClippingManager_DX9, _drawableClippingManager);
    CSM_DELETE_SELF(CubismClippingManager_DX9, _offscreenClippingManager);
}

csmBool CubismRenderer_D3D9::OnDeviceChanged()
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
    _deviceInfo = CubismDeviceInfo_D3D9::GetDeviceInfo(s_device);
    if (isInitialized)
    {
        // 既に設定されている場合は設定を更新する
        Initialize(GetModel(), s_bufferSetNum);
    }

    return true;
}

void CubismRenderer_D3D9::StartFrame()
{
    // レンダーステートフレーム先頭処理
    _deviceInfo->GetRenderState()->StartFrame();

    // シェーダ・頂点宣言
    _deviceInfo->GetShader()->SetupShader(_device);
}

void CubismRenderer_D3D9::EndFrame()
{
    // 使用シェーダエフェクト取得
    {
        ID3DXEffect* shaderEffect = _deviceInfo->GetShader()->GetShaderEffect();
        // テクスチャの参照を外しておく
        if (shaderEffect)
        {
            shaderEffect->SetTexture("mainTexture", NULL);
            shaderEffect->SetTexture("maskTexture", NULL);
            shaderEffect->CommitChanges();
        }
    }
}

void CubismRenderer_D3D9::Initialize(CubismModel* model)
{
    Initialize(model, 1);
}

void CubismRenderer_D3D9::Initialize(CubismModel* model, csmInt32 maskBufferCount)
{
    // デバイスが設定されていない場合は設定する
    if (_commandBufferNum == 0 || _device == NULL)
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
            CubismRenderTarget_D3D9 renderTarget;
            renderTarget.CreateRenderTarget(_device, _modelRenderTargetWidth, _modelRenderTargetHeight);
            _modelRenderTargets.PushBack(renderTarget);
        }
    }

    if (model->IsUsingMasking())
    {
        if (_drawableClippingManager != NULL)
        {
            CSM_DELETE_SELF(CubismClippingManager_DX9, _drawableClippingManager);
        }

        _drawableClippingManager = CSM_NEW CubismClippingManager_DX9();  //クリッピングマスク・バッファ前処理方式を初期化
        _drawableClippingManager->Initialize(
            *model,
            maskBufferCount,
            DrawableObjectType_Drawable
        );

        const csmInt32 bufferWidth = _drawableClippingManager->GetClippingMaskBufferSize().X;
        const csmInt32 bufferHeight = _drawableClippingManager->GetClippingMaskBufferSize().Y;

        _drawableMasks.Clear();

        for (csmUint32 i = 0; i < maskBufferCount; ++i)
        {
            csmVector<CubismRenderTarget_D3D9> vector;
            _drawableMasks.PushBack(vector);
            for (csmUint32 j = 0; j < maskBufferCount; ++j)
            {
                CubismRenderTarget_D3D9 offscreenRenderTarget;
                offscreenRenderTarget.CreateRenderTarget(_device, bufferWidth, bufferHeight);
                _drawableMasks[i].PushBack(offscreenRenderTarget);
            }
        }
    }

    if (model->IsUsingMaskingForOffscreen())
    {
        if (_offscreenClippingManager != NULL)
        {
            CSM_DELETE_SELF(CubismClippingManager_DX9, _offscreenClippingManager);
        }

        _offscreenClippingManager = CSM_NEW CubismClippingManager_DX9();  //クリッピングマスク・バッファ前処理方式を初期化
        _offscreenClippingManager->Initialize(
            *model,
            maskBufferCount,
            DrawableObjectType_Offscreen
        );

        const csmInt32 bufferWidth = _offscreenClippingManager->GetClippingMaskBufferSize().X;
        const csmInt32 bufferHeight = _offscreenClippingManager->GetClippingMaskBufferSize().Y;

        _offscreenMasks.Clear();

        for (csmUint32 i = 0; i < maskBufferCount; ++i)
        {
            csmVector<CubismRenderTarget_D3D9> vector;
            _offscreenMasks.PushBack(vector);
            for (csmUint32 j = 0; j < maskBufferCount; ++j)
            {
                CubismRenderTarget_D3D9 offscreenRenderTarget;
                offscreenRenderTarget.CreateRenderTarget(_device, bufferWidth, bufferHeight);
                _offscreenMasks[i].PushBack(offscreenRenderTarget);
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
        _offscreenList.Clear();
        _offscreenList = csmVector<CubismOffscreenRenderTarget_D3D9>(offscreenCount);
        for (csmInt32 offscreenIndex = 0; offscreenIndex < offscreenCount; ++offscreenIndex)
        {
            CubismOffscreenRenderTarget_D3D9 renderTarget;
            renderTarget.SetOffscreenIndex(offscreenIndex);
            _offscreenList.PushBack(renderTarget);
        }

        // 全てのオフスクリーンを登録し終わってから行う
        SetupParentOffscreens(model, offscreenCount);
    }

    CubismRenderer::Initialize(model, maskBufferCount);  //親クラスの処理を呼ぶ

    // モデルパーツごとに確保
    _drawableNum = drawableCount;

    _vertexStore = static_cast<CubismVertexD3D9**>(CSM_MALLOC(sizeof(CubismVertexD3D9*) * drawableCount));
    _indexStore = static_cast<csmUint16**>(CSM_MALLOC(sizeof(csmUint16*) * drawableCount));

    for (csmInt32 drawAssign = 0; drawAssign < drawableCount; ++drawAssign)
    {
        // 頂点
        const csmInt32 vcount = GetModel()->GetDrawableVertexCount(drawAssign);
        if (vcount != 0)
        {
            _vertexStore[drawAssign] = static_cast<CubismVertexD3D9*>(CSM_MALLOC(sizeof(CubismVertexD3D9) * vcount));
        }
        else
        {
            _vertexStore[drawAssign] = NULL;
        }

        // インデックスはここで要素コピーを済ませる
        const csmInt32 icount = GetModel()->GetDrawableVertexIndexCount(drawAssign);
        if (icount != 0)
        {
            _indexStore[drawAssign] = static_cast<csmUint16*>(CSM_MALLOC(sizeof(csmUint16) * icount));
            const csmUint16* idxarray = GetModel()->GetDrawableVertexIndices(drawAssign);
            for (csmInt32 ct = 0; ct < icount; ++ct)
            {// モデルデータからのコピー
                _indexStore[drawAssign][ct] = idxarray[ct];
            }
        }
        else
        {
            _indexStore[drawAssign] = NULL;
        }
    }

    _commandBufferNum = s_bufferSetNum;
    _commandBufferCurrent = 0;

    // バッファ作成
    CreateMeshBuffer();
}

void CubismRenderer_D3D9::SetupParentOffscreens(const CubismModel* model, csmInt32 offscreenCount)
{
    CubismOffscreenRenderTarget_D3D9* parentOffscreen;
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

void CubismRenderer_D3D9::FrameRenderingInit()
{
}

void CubismRenderer_D3D9::CreateMeshBuffer()
{
    void* ptr = NULL;

    // copy用頂点作成
    _device->CreateVertexBuffer(
        sizeof(ModelRenderTargetVertices),
        D3DUSAGE_WRITEONLY,
        0,
        D3DPOOL_MANAGED,
        &_copyVertexBuffer,
        nullptr);

    _copyVertexBuffer->Lock(0, 0, &ptr, 0);
    memcpy(ptr, ModelRenderTargetVertices, sizeof(ModelRenderTargetVertices));
    _copyVertexBuffer->Unlock();

    // offscreen用頂点作成
    _device->CreateVertexBuffer(
        sizeof(ModelRenderTargetReverseVertices),
        D3DUSAGE_WRITEONLY,
        0,
        D3DPOOL_MANAGED,
        &_offscreenVertexBuffer,
        nullptr);

    _offscreenVertexBuffer->Lock(0, 0, &ptr, 0);
    memcpy(ptr, ModelRenderTargetReverseVertices, sizeof(ModelRenderTargetReverseVertices));
    _offscreenVertexBuffer->Unlock();

    // インデックス作成
    const csmUint32 indexSize = sizeof(DWORD) * (sizeof(ModelRenderTargetIndexArray) / sizeof(csmUint32));
    _device->CreateIndexBuffer(
        indexSize,
        D3DUSAGE_WRITEONLY,
        D3DFMT_INDEX32,
        D3DPOOL_MANAGED,
        &_copyIndexBuffer,
        nullptr);

    _copyIndexBuffer->Lock(0, 0, &ptr, 0);
    memcpy(ptr, ModelRenderTargetIndexArray, indexSize);
    _copyIndexBuffer->Unlock();
}

void CubismRenderer_D3D9::PreDraw()
{
    SetDefaultRenderState(_modelRenderTargetWidth, _modelRenderTargetHeight);
}

void CubismRenderer_D3D9::PostDraw()
{
    // ダブル・トリプルバッファを回す
    ++_commandBufferCurrent;
    if (_commandBufferNum <= _commandBufferCurrent)
    {
        _commandBufferCurrent = 0;
    }
}

void CubismRenderer_D3D9::DoDrawModel()
{
    // NULLは許されず
    CSM_ASSERT(_device != NULL);

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
            _drawableClippingManager->SetupClippingContext(_device, _deviceInfo->GetRenderState(), *GetModel(), this, _commandBufferCurrent, DrawableObjectType_Drawable);
        }

        if (!IsUsingHighPrecisionMask())
        {
            // ビューポートを元に戻す
            _deviceInfo->GetRenderState()->SetViewport(_device,
                0,
                0,
                _modelRenderTargetWidth,
                _modelRenderTargetHeight,
                0.0f, 1.0);
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
            _offscreenClippingManager->SetupClippingContext(_device, _deviceInfo->GetRenderState(), *GetModel(), this, _commandBufferCurrent, DrawableObjectType_Offscreen);
        }

        if (!IsUsingHighPrecisionMask())
        {
            // ビューポートを元に戻す
            _deviceInfo->GetRenderState()->SetViewport(_device,
                0,
                0,
                _modelRenderTargetWidth,
                _modelRenderTargetHeight,
                0.0f, 1.0);
        }
    }

    // モデルの描画順に従って描画する
    DrawObjectLoop();

    // ダブルバッファ・トリプルバッファを回す
    PostDraw();

    AfterDrawModelRenderTarget();
}

void CubismRenderer_D3D9::DrawObjectLoop()
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
        _currentOffscreen->GetRenderTarget()->EndDraw(_device);
        DrawOffscreen(_currentOffscreen);
        _currentOffscreen = _currentOffscreen->GetParentPartOffscreen();
    }
}

void CubismRenderer_D3D9::RenderObject(const csmInt32 objectIndex, const csmInt32 objectType)
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

void CubismRenderer_D3D9::DrawDrawable(csmInt32 drawableIndex)
{
    // Drawableが表示状態でなければ処理をパスする
    if (!GetModel()->GetDrawableDynamicFlagIsVisible(drawableIndex))
    {
        return;
    }

    FlushOffscreenChainForDrawable(drawableIndex);

    // クリッピングマスクをセットする
    CubismClippingContext_D3D9* clipContext = (_drawableClippingManager != NULL) ?
        (*_drawableClippingManager->GetClippingContextListForDraw())[drawableIndex] :
        NULL;

    if (clipContext != NULL && IsUsingHighPrecisionMask()) // マスクを書く必要がある
    {
        if (clipContext->_isUsing) // 書くことになっていた
        {
            _deviceInfo->GetRenderState()->SetViewport(_device,
                0,
                0,
                _drawableClippingManager->GetClippingMaskBufferSize().X,
                _drawableClippingManager->GetClippingMaskBufferSize().Y,
                0.0f, 1.0f);

            // 正しいレンダーターゲットを持つオフスクリーンサーフェイスバッファを呼ぶ
            CubismRenderTarget_D3D9* currentHighPrecisionMaskColorBuffer = &_drawableMasks[_commandBufferCurrent][clipContext->_bufferIndex];

            currentHighPrecisionMaskColorBuffer->BeginDraw(_device);

            // 1が無効（描かれない）領域、0が有効（描かれる）領域。（シェーダで Cd*Csで0に近い値をかけてマスクを作る。1をかけると何も起こらない）
            currentHighPrecisionMaskColorBuffer->Clear(_device, 1.0f, 1.0f, 1.0f, 1.0f);

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
                DrawMeshDX9(*GetModel(), clipDrawIndex);
            }

            // --- 後処理 ---
            currentHighPrecisionMaskColorBuffer->EndDraw(_device);
            SetClippingContextBufferForMask(NULL);

            // ビューポートを元に戻す
            _deviceInfo->GetRenderState()->SetViewport(_device,
                0,
                0,
                _modelRenderTargetWidth,
                _modelRenderTargetHeight,
                0.0f, 1.0f);
        }
    }

    // クリッピングマスクをセットする
    SetClippingContextBufferForDrawable(clipContext);

    IsCulling(GetModel()->GetDrawableCulling(drawableIndex) != 0);
    DrawMeshDX9(*GetModel(), drawableIndex);
}

void CubismRenderer_D3D9::FlushOffscreenChainForDrawable(csmInt32 drawableIndex)
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
            _currentOffscreen->GetRenderTarget()->EndDraw(_device);
            DrawOffscreen(_currentOffscreen);
            _currentOffscreen = _currentOffscreen->GetParentPartOffscreen();
        }
        else
        {
            break;
        }
    }
}

void CubismRenderer_D3D9::AddOffscreen(csmInt32 offscreenIndex)
{
    CubismOffscreenRenderTarget_D3D9* offscreen = &_offscreenList[offscreenIndex];

    // 切り替えたいオフスクリーンが現在の子じゃないなら描画する
    while (_currentOffscreen != NULL)
    {
        if (_currentOffscreen != offscreen && _currentOffscreen != offscreen->GetParentPartOffscreen())
        {
            _currentOffscreen->GetRenderTarget()->EndDraw(_device);
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
    offscreen->GetRenderTarget()->BeginDraw(_device);
    _deviceInfo->GetRenderState()->SetViewport(_device,
        0,
        0,
        _modelRenderTargetWidth,
        _modelRenderTargetHeight,
        0.0f, 1.0f);
    offscreen->GetRenderTarget()->Clear(_device, 0.0f, 0.0f, 0.0f, 0.0f);

    // 現在のオフスクリーンレンダリングターゲットを設定
    _currentOffscreen = offscreen;
}

void CubismRenderer_D3D9::DrawOffscreen(CubismOffscreenRenderTarget_D3D9* offscreen)
{
    csmInt32 offscreenIndex = offscreen->GetOffscreenIndex();
    // クリッピングマスク
    CubismClippingContext_D3D9* clipContext = (_offscreenClippingManager != NULL) ?
        (*_offscreenClippingManager->GetClippingContextListForOffscreen())[offscreenIndex] :
        NULL;

    if (clipContext != NULL && IsUsingHighPrecisionMask()) // マスクを書く必要がある
    {
        if (clipContext->_isUsing) // 書くことになっていた
        {
            _deviceInfo->GetRenderState()->SetViewport(_device,
                0,
                0,
                _offscreenClippingManager->GetClippingMaskBufferSize().X,
                _offscreenClippingManager->GetClippingMaskBufferSize().Y,
                0.0f, 1.0f);

            // 正しいレンダーターゲットを持つオフスクリーンサーフェイスバッファを呼ぶ
            CubismRenderTarget_D3D9* currentHighPrecisionMaskColorBuffer = &_offscreenMasks[_commandBufferCurrent][clipContext->_bufferIndex];

            currentHighPrecisionMaskColorBuffer->BeginDraw(_device);

            // 1が無効（描かれない）領域、0が有効（描かれる）領域。（シェーダで Cd*Csで0に近い値をかけてマスクを作る。1をかけると何も起こらない）
            currentHighPrecisionMaskColorBuffer->Clear(_device, 1.0f, 1.0f, 1.0f, 1.0f);

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
                DrawMeshDX9(*GetModel(), clipDrawIndex);
            }

            // --- 後処理 ---
            currentHighPrecisionMaskColorBuffer->EndDraw(_device);
            SetClippingContextBufferForMask(NULL);

            // ビューポートを元に戻す
            _deviceInfo->GetRenderState()->SetViewport(_device,
                0,
                0,
                _modelRenderTargetWidth,
                _modelRenderTargetHeight,
                0.0f, 1.0f);
        }
    }

    // クリッピングマスクをセットする
    SetClippingContextBufferForOffscreen(clipContext);

    IsCulling(GetModel()->GetOffscreenCulling(offscreenIndex) != 0);
    DrawOffscreenDX9(*GetModel(), offscreen);
}

void CubismRenderer_D3D9::ExecuteDrawForDrawable(const CubismModel& model, const csmInt32 index)
{
    ID3DXEffect* shaderEffect = _deviceInfo->GetShader()->GetShaderEffect();
    if (!shaderEffect)
    {
        return;
    }

    csmBool isBlendMode;
    csmBlendMode blendMode = model.GetDrawableBlendModeType(index);
    SetBlendMode(blendMode, isBlendMode);
    if (isBlendMode)
    {
        int stop = 0;
    }
    SetTextureFilter();
    SetTechniqueForDrawable(model, index, isBlendMode, blendMode);

    // numPassには指定のtechnique内に含まれるpassの数が返る
    UINT numPass = 0;
    shaderEffect->Begin(&numPass, 0);
    shaderEffect->BeginPass(0);

    // テスクチャ
    SetExecutionTexturesForDrawable(model, index, shaderEffect, isBlendMode);

    // 定数バッファ
    {
        const csmBool masked = GetClippingContextBufferForDrawable() != NULL;
        if (masked)
        {
            // View座標をClippingContextの座標に変換するための行列を設定
            CubismMatrix44 ClipF = GetClippingContextBufferForDrawable()->_matrixForDraw;
            D3DXMATRIX clipM = ConvertToD3DX(ClipF);
            shaderEffect->SetMatrix("clipMatrix", &clipM);

            // 使用するカラーチャンネルを設定
            CubismClippingContext_D3D9* contextBuffer = GetClippingContextBufferForDrawable();
            SetColorChannel(shaderEffect, contextBuffer);
        }

        // プロジェクション行列
        SetProjectionMatrix(shaderEffect, GetMvpMatrix());

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
        SetColorVectors(shaderEffect, baseColor, multiplyColor, screenColor);

        // D3D9はピクセル中心が違うため補正用に渡す
        D3DXVECTOR4 screenSize(static_cast<csmFloat32>(_modelRenderTargetWidth), static_cast<csmFloat32>(_modelRenderTargetHeight), 0.0f, 0.0f);
        shaderEffect->SetVector("screenSize", &screenSize);

        // パラメータ反映
        shaderEffect->CommitChanges();
    }

    // 描画
    DrawIndexedPrimiteveWithSetup(model, index);

    shaderEffect->EndPass();
    shaderEffect->End();
}

void CubismRenderer_D3D9::ExecuteDrawForOffscreen(const CubismModel& model, CubismOffscreenRenderTarget_D3D9* offscreen)
{
    ID3DXEffect* shaderEffect = _deviceInfo->GetShader()->GetShaderEffect();
    if (!shaderEffect)
    {
        return;
    }

    csmInt32 offscreenIndex = offscreen->GetOffscreenIndex();
    csmBool isBlendMode;
    csmBlendMode blendMode = model.GetOffscreenBlendModeType(offscreenIndex);
    SetBlendMode(blendMode, isBlendMode);
    SetOffscreenTextureFilter();
    SetTechniqueForOffscreen(model, offscreenIndex, isBlendMode, blendMode);

    // numPassには指定のtechnique内に含まれるpassの数が返る
    UINT numPass = 0;
    shaderEffect->Begin(&numPass, 0);
    shaderEffect->BeginPass(0);

    // テスクチャ
    SetExecutionTexturesForOffscreen(model, offscreen, shaderEffect);

    // 定数バッファ
    {
        const csmBool masked = GetClippingContextBufferForOffscreen() != NULL;
        if (masked)
        {
            // View座標をClippingContextの座標に変換するための行列を設定
            CubismMatrix44 ClipF = GetClippingContextBufferForOffscreen()->_matrixForDraw;
            D3DXMATRIX clipM = ConvertToD3DX(ClipF);
            shaderEffect->SetMatrix("clipMatrix", &clipM);

            // 使用するカラーチャンネルを設定
            CubismClippingContext_D3D9* contextBuffer = GetClippingContextBufferForOffscreen();
            SetColorChannel(shaderEffect, contextBuffer);
        }

        // プロジェクション行列
        CubismMatrix44 mvpMatrix;
        mvpMatrix.LoadIdentity();
        SetProjectionMatrix(shaderEffect, mvpMatrix);

        // 色
        csmFloat32 offscreenOpacity = model.GetOffscreenOpacity(offscreenIndex);
        // PMAなのと不透明度だけを変更したいためすべてOpacityで初期化
        CubismTextureColor modelColorRGBA(offscreenOpacity, offscreenOpacity, offscreenOpacity, offscreenOpacity);
        CubismTextureColor multiplyColor = model.GetMultiplyColorOffscreen(offscreenIndex);
        CubismTextureColor screenColor = model.GetScreenColorOffscreen(offscreenIndex);
        SetColorVectors(shaderEffect, modelColorRGBA, multiplyColor, screenColor);

        // D3D9はピクセル中心が違うため補正用に渡す
        D3DXVECTOR4 screenSize(static_cast<csmFloat32>(_modelRenderTargetWidth), static_cast<csmFloat32>(_modelRenderTargetHeight), 0.0f, 0.0f);
        shaderEffect->SetVector("screenSize", &screenSize);

        // パラメータ反映
        shaderEffect->CommitChanges();
    }

    // 描画
    _device->SetStreamSource(0, _offscreenVertexBuffer, 0, sizeof(csmFloat32) * 4);
    _device->SetIndices(_copyIndexBuffer);
    _device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);

    shaderEffect->EndPass();
    shaderEffect->End();
}

void CubismRenderer_D3D9::ExecuteDrawForMask(const CubismModel& model, const csmInt32 index)
{
    ID3DXEffect* shaderEffect = _deviceInfo->GetShader()->GetShaderEffect();
    if (!shaderEffect)
    {
        return;
    }

    shaderEffect->SetTechnique("ShaderNames_SetupMask");
    SetBlendMode(CubismBlendMode_Mask);

    // numPassには指定のtechnique内に含まれるpassの数が返る
    UINT numPass = 0;
    shaderEffect->Begin(&numPass, 0);
    shaderEffect->BeginPass(0);

    // マスクには線形補間を適用
    _deviceInfo->GetRenderState()->SetTextureFilter(_device, 1, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_NONE, D3DTADDRESS_WRAP, D3DTADDRESS_WRAP);

    // 定数バッファ
    {
        // プロジェクション行列
        SetProjectionMatrix(shaderEffect, GetClippingContextBufferForMask()->_matrixForMask);

        // 色
        csmRectF* rect = GetClippingContextBufferForMask()->_layoutBounds;
        CubismTextureColor baseColor = {rect->X * 2.0f - 1.0f, rect->Y * 2.0f - 1.0f, rect->GetRight() * 2.0f - 1.0f, rect->GetBottom() * 2.0f - 1.0f};
        CubismTextureColor multiplyColor = model.GetMultiplyColor(index);
        CubismTextureColor screenColor = model.GetScreenColor(index);
        SetColorVectors(shaderEffect, baseColor, multiplyColor, screenColor);

        // 使用するカラーチャンネルを設定
        CubismClippingContext_D3D9* contextBuffer = GetClippingContextBufferForMask();
        SetColorChannel(shaderEffect, contextBuffer);

        // テスクチャ
        SetExecutionTexturesForDrawable(model, index, shaderEffect);

        CubismVector2 maskSize = contextBuffer->GetClippingManager()->GetClippingMaskBufferSize();

        // D3D9はピクセル中心が違うため補正用に渡す
        D3DXVECTOR4 screenSize(maskSize.X, maskSize.Y, 0.0f, 0.0f);
        shaderEffect->SetVector("screenSize", &screenSize);

        // パラメータ反映
        shaderEffect->CommitChanges();
    }

    // 描画
    DrawIndexedPrimiteveWithSetup(model, index);

    shaderEffect->EndPass();
    shaderEffect->End();
}

void CubismRenderer_D3D9::ExecuteDrawForRenderTarget()
{
    ID3DXEffect* shaderEffect = _deviceInfo->GetShader()->GetShaderEffect();
    if (!shaderEffect)
    {
        return;
    }

    shaderEffect->SetTechnique("ShaderNames_Copy");
    SetOffscreenTextureFilter();
    SetBlendMode(CubismBlendMode_Copy);

    // numPassには指定のtechnique内に含まれるpassの数が返る
    UINT numPass = 0;
    shaderEffect->Begin(&numPass, 0);
    shaderEffect->BeginPass(0);

    // テスクチャ
    LPDIRECT3DTEXTURE9 mainTexture = _modelRenderTargets[0].GetTexture();
    shaderEffect->SetTexture("mainTexture", mainTexture);

    // 定数バッファ
    {
        // プロジェクション行列
        CubismMatrix44 matrix;
        matrix.LoadIdentity();
        SetProjectionMatrix(shaderEffect, matrix);

        // ベースカラーを適用
        CubismRenderer::CubismTextureColor baseColor = GetModelColor();
        baseColor.R *= baseColor.A;
        baseColor.G *= baseColor.A;
        baseColor.B *= baseColor.A;
        D3DXVECTOR4 shaderBaseColor(baseColor.R, baseColor.G, baseColor.B, baseColor.A);
        shaderEffect->SetVector("baseColor", &shaderBaseColor);

        // D3D9はピクセル中心が違うため補正用に渡す
        D3DXVECTOR4 screenSize(static_cast<csmFloat32>(_modelRenderTargetWidth), static_cast<csmFloat32>(_modelRenderTargetHeight), 0.0f, 0.0f);
        shaderEffect->SetVector("screenSize", &screenSize);

        // パラメータ反映
        shaderEffect->CommitChanges();
    }

    // 描画
    _device->SetStreamSource(0, _copyVertexBuffer, 0, sizeof(csmFloat32) * 4);
    _device->SetIndices(_copyIndexBuffer);
    _device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);

    shaderEffect->EndPass();
    shaderEffect->End();
}

void CubismRenderer_D3D9::DrawMeshDX9(const CubismModel& model, const csmInt32 index)
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
    if (GetTextureWithIndex(model, index) == NULL)
    {
        return;
    }

    // 裏面描画の有効・無効
    _deviceInfo->GetRenderState()->SetCullMode(_device, (IsCulling() ? D3DCULL_CW : D3DCULL_NONE));

    // 頂点バッファにコピー
    {
        const csmInt32 drawableIndex = index;
        const csmInt32 vertexCount = model.GetDrawableVertexCount(index);
        const csmFloat32* vertexArray = model.GetDrawableVertices(index);
        const csmFloat32* uvArray = reinterpret_cast<const csmFloat32*>(model.GetDrawableVertexUvs(index));
        CopyToBuffer(drawableIndex, vertexCount, vertexArray, uvArray);
    }

    // シェーダーセット
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

void CubismRenderer_D3D9::DrawOffscreenDX9(const CubismModel& model, CubismOffscreenRenderTarget_D3D9* offscreen)
{
    // デバイス未設定
    if (_device == NULL)
    {
        return;
    }

    // 裏面描画の有効・無効
    _deviceInfo->GetRenderState()->SetCullMode(_device, (IsCulling() ? D3DCULL_CW : D3DCULL_NONE));

    ExecuteDrawForOffscreen(model, offscreen);

    offscreen->StopUsingRenderTexture(_deviceInfo->GetOffscreenManager());
    SetClippingContextBufferForOffscreen(NULL);
    SetClippingContextBufferForMask(NULL);
}

void CubismRenderer_D3D9::SaveProfile()
{
    // NULLは許されず
    CSM_ASSERT(_device != NULL);

    // 現在のレンダリングステートをPush
    _deviceInfo->GetRenderState()->SaveCurrentNativeState(_device);
}

void CubismRenderer_D3D9::RestoreProfile()
{
    // NULLは許されず
    CSM_ASSERT(_device != NULL);

    // SaveCurrentNativeStateと対
    _deviceInfo->GetRenderState()->RestoreNativeState(_device);
}

void CubismRenderer_D3D9::BeforeDrawModelRenderTarget()
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
    _modelRenderTargets[0].BeginDraw(_device);
    _modelRenderTargets[0].Clear(_device, 0.0f, 0.0f, 0.0f, 0.0f);
}

void CubismRenderer_D3D9::AfterDrawModelRenderTarget()
{
    if (_modelRenderTargets.GetSize() == 0)
    {
        return;
    }

    // 元のバッファに描画する
    _modelRenderTargets[0].EndDraw(_device);

    ExecuteDrawForRenderTarget();
}

void CubismRenderer_D3D9::BindTexture(csmUint32 modelTextureAssign, LPDIRECT3DTEXTURE9 texture)
{
    _textures[modelTextureAssign] = texture;
}

const csmMap<csmInt32, LPDIRECT3DTEXTURE9>& CubismRenderer_D3D9::GetBindedTextures() const
{
    return _textures;
}

void CubismRenderer_D3D9::SetDrawableClippingMaskBufferSize(csmFloat32 width, csmFloat32 height)
{
    if (_drawableClippingManager == NULL)
    {
        return;
    }

    // インスタンス破棄前にレンダーテクスチャの数を保存
    const csmInt32 renderTextureCount = _drawableClippingManager->GetRenderTextureCount();


    // RenderTargetのサイズを変更するためにインスタンスを破棄・再作成する
    CSM_DELETE_SELF(CubismClippingManager_DX9, _drawableClippingManager);

    _drawableClippingManager = CSM_NEW CubismClippingManager_DX9();

    _drawableClippingManager->SetClippingMaskBufferSize(width, height);

    _drawableClippingManager->Initialize(
        *GetModel(),
        renderTextureCount,
        DrawableObjectType_Drawable
    );
}

void CubismRenderer_D3D9::SetOffscreenClippingMaskBufferSize(csmFloat32 width, csmFloat32 height)
{
    if (_offscreenClippingManager == NULL)
    {
        return;
    }

    // インスタンス破棄前にレンダーテクスチャの数を保存
    const csmInt32 renderTextureCount = _offscreenClippingManager->GetRenderTextureCount();


    // RenderTargetのサイズを変更するためにインスタンスを破棄・再作成する
    CSM_DELETE_SELF(CubismClippingManager_DX9, _offscreenClippingManager);

    _offscreenClippingManager = CSM_NEW CubismClippingManager_DX9();

    _offscreenClippingManager->SetClippingMaskBufferSize(width, height);

    _offscreenClippingManager->Initialize(
        *GetModel(),
        renderTextureCount,
        DrawableObjectType_Offscreen
    );
}

csmInt32 CubismRenderer_D3D9::GetDrawableRenderTextureCount() const
{
    return _drawableClippingManager->GetRenderTextureCount();
}

csmInt32 CubismRenderer_D3D9::GetOffscreenRenderTextureCount() const
{
    return _offscreenClippingManager->GetRenderTextureCount();
}

CubismVector2 CubismRenderer_D3D9::GetDrawableClippingMaskBufferSize() const
{
    return _drawableClippingManager->GetClippingMaskBufferSize();
}

CubismVector2 CubismRenderer_D3D9::GetOffscreenClippingMaskBufferSize() const
{
    return _offscreenClippingManager->GetClippingMaskBufferSize();
}

LPDIRECT3DTEXTURE9 CubismRenderer_D3D9::CopyModelRenderTarget()
{
    return CopyRenderTarget(_modelRenderTargets[0]);
}

LPDIRECT3DTEXTURE9 CubismRenderer_D3D9::CopyRenderTarget(CubismRenderTarget_D3D9& srcBuffer)
{
    _modelRenderTargets[1].BeginDraw(_device);

    CubismRenderTarget_D3D9::CopyBuffer(_device, srcBuffer, _modelRenderTargets[1]);

    _modelRenderTargets[1].EndDraw(_device);

    return _modelRenderTargets[1].GetTexture();
}

CubismRenderTarget_D3D9* CubismRenderer_D3D9::GetDrawableMaskBuffer(csmUint32 backbufferNum, csmInt32 offscreenIndex)
{
    return &_drawableMasks[backbufferNum][offscreenIndex];
}

CubismRenderTarget_D3D9* CubismRenderer_D3D9::GetOffscreenMaskBuffer(csmUint32 backbufferNum, csmInt32 offscreenIndex)
{
    return &_offscreenMasks[backbufferNum][offscreenIndex];
}

CubismOffscreenRenderTarget_D3D9* CubismRenderer_D3D9::GetCurrentOffscreen() const
{
    return _currentOffscreen;
}

void CubismRenderer_D3D9::SetClippingContextBufferForDrawable(CubismClippingContext_D3D9* clip)
{
    _clippingContextBufferForDrawable = clip;
}

CubismClippingContext_D3D9* CubismRenderer_D3D9::GetClippingContextBufferForDrawable() const
{
    return _clippingContextBufferForDrawable;
}

void CubismRenderer_D3D9::SetClippingContextBufferForOffscreen(CubismClippingContext_D3D9* clip)
{
    _clippingContextBufferForOffscreen = clip;
}

CubismClippingContext_D3D9* CubismRenderer_D3D9::GetClippingContextBufferForOffscreen() const
{
    return _clippingContextBufferForOffscreen;
}

void CubismRenderer_D3D9::SetClippingContextBufferForMask(CubismClippingContext_D3D9* clip)
{
    _clippingContextBufferForMask = clip;
}

CubismClippingContext_D3D9* CubismRenderer_D3D9::GetClippingContextBufferForMask() const
{
    return _clippingContextBufferForMask;
}

void CubismRenderer_D3D9::CopyToBuffer(csmInt32 drawAssign, const csmInt32 vcount, const csmFloat32* varray, const csmFloat32* uvarray)
{
    for (csmInt32 ct = 0; ct < vcount * 2; ct += 2)
    {// モデルデータからのコピー
        _vertexStore[drawAssign][ct / 2].x = varray[ct + 0];
        _vertexStore[drawAssign][ct / 2].y = varray[ct + 1];

        _vertexStore[drawAssign][ct / 2].u = uvarray[ct + 0];
        _vertexStore[drawAssign][ct / 2].v = uvarray[ct + 1];
    }
}

LPDIRECT3DTEXTURE9 CubismRenderer_D3D9::GetTextureWithIndex(const CubismModel& model, const csmInt32 index)
{
    LPDIRECT3DTEXTURE9 result = NULL;
    const csmInt32 textureIndex = model.GetDrawableTextureIndex(index);
    if (textureIndex >= 0)
    {
        result = _textures[textureIndex];
    }

    return result;
}

const csmBool inline CubismRenderer_D3D9::IsGeneratingMask() const
{
    return (GetClippingContextBufferForMask() != NULL);
}

void CubismRenderer_D3D9::SetBlendMode(CubismBlendMode blendMode) const
{
    switch (blendMode)
    {
    case CubismBlendMode::CubismBlendMode_Normal:
    default:
        _deviceInfo->GetRenderState()->SetBlend(_device,
            true,
            true,
            D3DBLEND_ONE,
            D3DBLENDOP_ADD,
            D3DBLEND_INVSRCALPHA,
            D3DBLEND_ONE,
            D3DBLENDOP_ADD,
            D3DBLEND_INVSRCALPHA);
        break;

    case CubismBlendMode::CubismBlendMode_Additive:
        _deviceInfo->GetRenderState()->SetBlend(_device,
            true,
            true,
            D3DBLEND_ONE,
            D3DBLENDOP_ADD,
            D3DBLEND_ONE,
            D3DBLEND_ZERO,
            D3DBLENDOP_ADD,
            D3DBLEND_ONE);
        break;

    case CubismBlendMode::CubismBlendMode_Multiplicative:
        _deviceInfo->GetRenderState()->SetBlend(_device,
        true,
        true,
        D3DBLEND_DESTCOLOR,
        D3DBLENDOP_ADD,
        D3DBLEND_INVSRCALPHA,
        D3DBLEND_ZERO,
        D3DBLENDOP_ADD,
        D3DBLEND_ONE);
        break;

    case CubismBlendMode::CubismBlendMode_Mask:
        _deviceInfo->GetRenderState()->SetBlend(_device,
            true,
            true,
            D3DBLEND_ZERO,
            D3DBLENDOP_ADD,
            D3DBLEND_INVSRCCOLOR,
            D3DBLEND_ZERO,
            D3DBLENDOP_ADD,
            D3DBLEND_INVSRCALPHA);
            break;
    case CubismBlendMode::CubismBlendMode_Copy:
        _deviceInfo->GetRenderState()->SetBlend(_device,
            true,
            true,
            D3DBLEND_ONE,
            D3DBLENDOP_ADD,
            D3DBLEND_INVSRCALPHA,
            D3DBLEND_ONE,
            D3DBLENDOP_ADD,
            D3DBLEND_INVSRCALPHA);
        break;
    }
}

void CubismRenderer_D3D9::SetBlendMode(const csmBlendMode blendMode, csmBool& isBlendMode) const
{
    isBlendMode = false;

    if (blendMode.GetColorBlendType() == Core::csmColorBlendType_Normal && blendMode.GetAlphaBlendType() == Core::csmAlphaBlendType_Over)
    {
        SetBlendMode(CubismBlendMode_Normal);
    }
    else if (blendMode.GetColorBlendType() == Core::csmColorBlendType_AddCompatible)
    {
        SetBlendMode(CubismBlendMode_Additive);
    }
    else if (blendMode.GetColorBlendType() == Core::csmColorBlendType_MultiplyCompatible)
    {
        SetBlendMode(CubismBlendMode_Multiplicative);
    }
    else
    {
        _deviceInfo->GetRenderState()->SetBlend(_device,
            true,
            true,
            D3DBLEND_ONE,
            D3DBLENDOP_ADD,
            D3DBLEND_ZERO,
            D3DBLEND_ONE,
            D3DBLENDOP_ADD,
            D3DBLEND_ZERO);

        isBlendMode = true;
    }
}

void CubismRenderer_D3D9::SetTechniqueForDrawable(const CubismModel& model, const csmInt32 index, csmBool isBlendMode, csmBlendMode blendMode) const
{
    ID3DXEffect* shaderEffect = _deviceInfo->GetShader()->GetShaderEffect();
    const csmBool masked = GetClippingContextBufferForDrawable() != NULL;  // この描画オブジェクトはマスク対象か
    const csmBool premult = IsPremultipliedAlpha();
    const csmBool invertedMask = model.GetDrawableInvertedMask(index);
    csmString shaderName = GetTechniqueName(masked, premult, invertedMask, isBlendMode, blendMode);
    shaderEffect->SetTechnique(GetTechniqueName(masked, premult, invertedMask, isBlendMode, blendMode).GetRawString());
}

void CubismRenderer_D3D9::SetTechniqueForOffscreen(const CubismModel& model, csmInt32 index, csmBool isBlendMode, csmBlendMode blendMode) const
{
    ID3DXEffect* shaderEffect = _deviceInfo->GetShader()->GetShaderEffect();
    const csmBool masked = GetClippingContextBufferForOffscreen() != NULL;  // この描画オブジェクトはマスク対象か
    const csmBool premult = true; // オフスクリーンはPMAを使う
    const csmBool invertedMask = model.GetOffscreenInvertedMask(index);
    shaderEffect->SetTechnique(GetTechniqueName(masked, premult, invertedMask, isBlendMode, blendMode).GetRawString());
}

void CubismRenderer_D3D9::SetTextureFilter() const
{
    if (GetAnisotropy() > 1.0f)
    {
        _deviceInfo->GetRenderState()->SetTextureFilter(_device, 0, D3DTEXF_ANISOTROPIC, D3DTEXF_ANISOTROPIC, D3DTEXF_LINEAR, D3DTADDRESS_WRAP, D3DTADDRESS_WRAP, GetAnisotropy());
    }
    else
    {
        _deviceInfo->GetRenderState()->SetTextureFilter(_device, 0, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTADDRESS_WRAP, D3DTADDRESS_WRAP);
    }
}

void CubismRenderer_D3D9::SetOffscreenTextureFilter() const
{
    _deviceInfo->GetRenderState()->SetTextureFilter(_device, 0, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_NONE, D3DTADDRESS_WRAP, D3DTADDRESS_WRAP);
}

void CubismRenderer_D3D9::SetColorVectors(ID3DXEffect* shaderEffect, CubismTextureColor& baseColor, CubismTextureColor& multiplyColor, CubismTextureColor& screenColor)
{
    D3DXVECTOR4 shaderBaseColor(baseColor.R, baseColor.G, baseColor.B, baseColor.A);
    D3DXVECTOR4 shaderMultiplyColor(multiplyColor.R, multiplyColor.G, multiplyColor.B, multiplyColor.A);
    D3DXVECTOR4 shaderScreenColor(screenColor.R, screenColor.G, screenColor.B, screenColor.A);

    shaderEffect->SetVector("baseColor", &shaderBaseColor);
    shaderEffect->SetVector("multiplyColor", &shaderMultiplyColor);
    shaderEffect->SetVector("screenColor", &shaderScreenColor);
}

void CubismRenderer_D3D9::SetExecutionTexturesForDrawable(const CubismModel& model, const csmInt32 index, ID3DXEffect* shaderEffect, csmBool isBlendMode)
{
    const csmBool drawing = !IsGeneratingMask();
    const csmBool masked = GetClippingContextBufferForDrawable() != NULL;
    LPDIRECT3DTEXTURE9 mainTexture = GetTextureWithIndex(model, index);
    LPDIRECT3DTEXTURE9 maskTexture = ((drawing && masked) ? _drawableMasks[_commandBufferCurrent][GetClippingContextBufferForDrawable()->_bufferIndex].GetTexture() : NULL);

    shaderEffect->SetTexture("mainTexture", mainTexture);
    shaderEffect->SetTexture("maskTexture", maskTexture);
    if (isBlendMode)
    {
        LPDIRECT3DTEXTURE9 blendTexture = _currentOffscreen != NULL ?
            CopyRenderTarget(*_currentOffscreen->GetRenderTarget()) :
            CopyModelRenderTarget();
        shaderEffect->SetTexture("blendTexture", blendTexture);
    }
}

void CubismRenderer_D3D9::SetExecutionTexturesForOffscreen(const CubismModel& model, const CubismOffscreenRenderTarget_D3D9* offscreen, ID3DXEffect* shaderEffect)
{
    csmInt32 offscreenIndex = offscreen->GetOffscreenIndex();
    const csmBool masked = GetClippingContextBufferForOffscreen() != NULL;
    LPDIRECT3DTEXTURE9 mainTexture = offscreen->GetRenderTarget()->GetTexture();
    LPDIRECT3DTEXTURE9 maskTexture = masked ? _offscreenMasks[_commandBufferCurrent][GetClippingContextBufferForOffscreen()->_bufferIndex].GetTexture() : NULL;
    LPDIRECT3DTEXTURE9 blendTexture = offscreen->GetParentPartOffscreen() != NULL ?
        CopyRenderTarget(*offscreen->GetParentPartOffscreen()->GetRenderTarget()) :
        CopyModelRenderTarget();

    shaderEffect->SetTexture("mainTexture", mainTexture);
    shaderEffect->SetTexture("maskTexture", maskTexture);
    shaderEffect->SetTexture("blendTexture", blendTexture);
}

void CubismRenderer_D3D9::SetColorChannel(ID3DXEffect* shaderEffect, CubismClippingContext_D3D9* contextBuffer)
{
    const csmInt32 channelIndex = contextBuffer->_layoutChannelIndex;
    CubismRenderer::CubismTextureColor* colorChannel = contextBuffer->GetClippingManager()->GetChannelFlagAsColor(channelIndex);
    D3DXVECTOR4 channel(colorChannel->R, colorChannel->G, colorChannel->B, colorChannel->A);
    shaderEffect->SetVector("channelFlag", &channel);
}

void CubismRenderer_D3D9::SetProjectionMatrix(ID3DXEffect* shaderEffect, CubismMatrix44& matrix)
{
    D3DXMATRIX proj = ConvertToD3DX(matrix);
    shaderEffect->SetMatrix("projectMatrix", &proj);
}

void CubismRenderer_D3D9::DrawIndexedPrimiteveWithSetup(const CubismModel& model, const csmInt32 index)
{
    const csmInt32 vertexCount = model.GetDrawableVertexCount(index);
    const csmInt32 indexCount = model.GetDrawableVertexIndexCount(index);
    const csmInt32 triangleCount = indexCount / 3;
    const csmUint16* indexArray = _indexStore[index];
    const CubismVertexD3D9* vertexArray = _vertexStore[index];
    _device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, vertexCount, triangleCount, indexArray, D3DFMT_INDEX16, vertexArray, sizeof(CubismVertexD3D9));
}

}}}}

//------------ LIVE2D NAMESPACE ------------
