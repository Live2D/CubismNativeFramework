﻿/*
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at http://live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismRenderer_OpenGLES2.hpp"
#include "Math/CubismMatrix44.hpp"
#include "Type/csmVector.hpp"
#include "Model/CubismModel.hpp"
#include <float.h>

#ifdef CSM_TARGET_WIN_GL
#include <Windows.h>
#endif

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/*********************************************************************************************************************
*                                      CubismClippingManager_OpenGLES2
********************************************************************************************************************/
///< ファイルスコープの変数宣言
namespace {
const csmInt32 ColorChannelCount = 4;   ///< 実験時に1チャンネルの場合は1、RGBだけの場合は3、アルファも含める場合は4
}

CubismClippingManager_OpenGLES2::CubismClippingManager_OpenGLES2() :
                                                                   _maskRenderTexture(0)
                                                                   , _colorBuffer(0)
                                                                   , _currentFrameNo(0)
                                                                   , _clippingMaskBufferSize(256)
{
    CubismRenderer::CubismTextureColor* tmp;
    tmp = CSM_NEW CubismRenderer::CubismTextureColor();
    tmp->R = 1.0f;
    tmp->G = 0.0f;
    tmp->B = 0.0f;
    tmp->A = 0.0f;
    _channelColors.PushBack(tmp);
    tmp = CSM_NEW CubismRenderer::CubismTextureColor();
    tmp->R = 0.0f;
    tmp->G = 1.0f;
    tmp->B = 0.0f;
    tmp->A = 0.0f;
    _channelColors.PushBack(tmp);
    tmp = CSM_NEW CubismRenderer::CubismTextureColor();
    tmp->R = 0.0f;
    tmp->G = 0.0f;
    tmp->B = 1.0f;
    tmp->A = 0.0f;
    _channelColors.PushBack(tmp);
    tmp = CSM_NEW CubismRenderer::CubismTextureColor();
    tmp->R = 0.0f;
    tmp->G = 0.0f;
    tmp->B = 0.0f;
    tmp->A = 1.0f;
    _channelColors.PushBack(tmp);

}

CubismClippingManager_OpenGLES2::~CubismClippingManager_OpenGLES2()
{
    for (csmUint32 i = 0; i < _clippingContextListForMask.GetSize(); i++)
    {
        if (_clippingContextListForMask[i]) CSM_DELETE_SELF(CubismClippingContext, _clippingContextListForMask[i]);
        _clippingContextListForMask[i] = NULL;
    }

    // _clippingContextListForDrawは_clippingContextListForMaskにあるインスタンスを指している。上記の処理により要素ごとのDELETEは不要。
    for (csmUint32 i = 0; i < _clippingContextListForDraw.GetSize(); i++)
    {
        _clippingContextListForDraw[i] = NULL;
    }
    for (csmUint32 i = 0; i < _maskTextures.GetSize(); i++)
    {
        glDeleteFramebuffers(1, &_maskTextures[i]->Texture);
        if (_maskTextures[i]) CSM_DELETE_SELF(CubismRenderTextureResource, _maskTextures[i]);
        _maskTextures[i] = NULL;
    }

    for (csmUint32 i = 0; i < _channelColors.GetSize(); i++)
    {
        if (_channelColors[i]) CSM_DELETE(_channelColors[i]);
        _channelColors[i] = NULL;
    }
}

void CubismClippingManager_OpenGLES2::Initialize(CubismModel& model, csmInt32 drawableCount, const csmInt32** drawableMasks, const csmInt32* drawableMaskCounts)
{
    //クリッピングマスクを使う描画オブジェクトを全て登録する
    //クリッピングマスクは、通常数個程度に限定して使うものとする
    for (csmInt32 i = 0; i < drawableCount; i++)
    {
        if (drawableMaskCounts[i] <= 0)
        {
            //クリッピングマスクが使用されていないアートメッシュ（多くの場合使用しない）
            _clippingContextListForDraw.PushBack(NULL);
            continue;
        }

        // 既にあるClipContextと同じかチェックする
        CubismClippingContext* cc = FindSameClip(drawableMasks[i], drawableMaskCounts[i]);
        if (cc == NULL)
        {
            // 同一のマスクが存在していない場合は生成する
            cc = CSM_NEW CubismClippingContext(this, drawableMasks[i], drawableMaskCounts[i]);
            _clippingContextListForMask.PushBack(cc);
        }

        cc->AddClippedDrawable(i);

        _clippingContextListForDraw.PushBack(cc);
    }
}

CubismClippingContext* CubismClippingManager_OpenGLES2::FindSameClip(const csmInt32* drawableMasks, csmInt32 drawableMaskCounts) const
{
    // 作成済みClippingContextと一致するか確認
    for (csmUint32 i = 0; i < _clippingContextListForMask.GetSize(); i++)
    {
        CubismClippingContext* cc = _clippingContextListForMask[i];
        const csmInt32 count = cc->_clippingIdCount;
        if (count != drawableMaskCounts) continue; //個数が違う場合は別物
        csmInt32 samecount = 0;

        // 同じIDを持つか確認。配列の数が同じなので、一致した個数が同じなら同じ物を持つとする。
        for (csmInt32 j = 0; j < count; j++)
        {
            const csmInt32 clipId = cc->_clippingIdList[j];
            for (csmInt32 k = 0; k < count; k++)
            {
                if (drawableMasks[k] == clipId)
                {
                    samecount++;
                    break;
                }
            }
        }
        if (samecount == count)
        {
            return cc;
        }
    }
    return NULL; //見つからなかった
}

GLuint CubismClippingManager_OpenGLES2::GetMaskRenderTexture()
{
    GLuint ret = 0;

    // テンポラリのRenderTexutreを取得する
    for (csmUint32 i = 0; i < _maskTextures.GetSize(); i++)
    {
        if (_maskTextures[i]->Texture != 0) //前回使ったものを返す
        {
            _maskTextures[i]->FrameNo = _currentFrameNo;
            ret = _maskTextures[i]->Texture;
            break;
        }
    }

    if (ret == 0)
    {
        // FramebufferObjectが存在しない場合、新しく生成する
        const csmInt32 size = _clippingMaskBufferSize;
        glGenTextures(1, &_colorBuffer);
        glBindTexture(GL_TEXTURE_2D, _colorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        GLint tmpFramebufferObject;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &tmpFramebufferObject);

        glGenFramebuffers(1, &ret);
        glBindFramebuffer(GL_FRAMEBUFFER, ret);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorBuffer, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, tmpFramebufferObject);

        _maskTextures.PushBack(CSM_NEW CubismRenderTextureResource(_currentFrameNo, ret));
    }

    return ret;
}

void CubismClippingManager_OpenGLES2::SetupClippingContext(CubismModel& model, CubismRenderer_OpenGLES2* renderer)
{
    _currentFrameNo++;

    // 全てのクリッピングを用意する
    // 同じクリップ（複数の場合はまとめて１つのクリップ）を使う場合は１度だけ設定する
    csmInt32 usingClipCount = 0;
    for (csmUint32 clipIndex = 0; clipIndex < _clippingContextListForMask.GetSize(); clipIndex++)
    {
        // １つのクリッピングマスクに関して
        CubismClippingContext* cc = _clippingContextListForMask[clipIndex];

        // このクリップを利用する描画オブジェクト群全体を囲む矩形を計算
        CalcClippedDrawTotalBounds(model, cc);

        if (cc->_isUsing)
        {
            usingClipCount++; //使用中としてカウント
        }
    }

    // マスク作成処理
    if (usingClipCount > 0)
    {
        // 現在のビューポートの値を退避
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);

        // 生成したFrameBufferと同じサイズでビューポートを設定
        glViewport(0, 0, _clippingMaskBufferSize, _clippingMaskBufferSize);

        // マスクactive切り替え前のFBOを退避
        GLint oldFBO;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);

        // マスクをactiveにする
        _maskRenderTexture = GetMaskRenderTexture();

        // モデル描画時にDrawMeshNowに渡される変換（モデルtoワールド座標変換）
        CubismMatrix44 modelToWorldF = renderer->GetMvpMatrix();

        renderer->PreDraw(); // バッファをクリアする

        // 各マスクのレイアウトを決定していく
        SetupLayoutBounds(usingClipCount);

        // ---------- マスク描画処理 -----------
        // マスク用RenderTextureをactiveにセット
        glBindFramebuffer(GL_FRAMEBUFFER, _maskRenderTexture);

        // マスクをクリアする
        //（仮仕様） 1が無効（描かれない）領域、0が有効（描かれる）領域。（シェーダで Cd*Csで0に近い値をかけてマスクを作る。1をかけると何も起こらない）
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 実際にマスクを生成する
        // 全てのマスクをどの様にレイアウトして描くかを決定し、ClipContext , ClippedDrawContext に記憶する
        for (csmUint32 clipIndex = 0; clipIndex < _clippingContextListForMask.GetSize(); clipIndex++)
        {
            // --- 実際に１つのマスクを描く ---
            CubismClippingContext* clipContext = _clippingContextListForMask[clipIndex];
            csmRectF* allClippedDrawRect = clipContext->_allClippedDrawRect; //このマスクを使う、全ての描画オブジェクトの論理座標上の囲み矩形
            csmRectF* layoutBoundsOnTex01 = clipContext->_layoutBounds; //この中にマスクを収める

            // モデル座標上の矩形を、適宜マージンを付けて使う
            const csmFloat32 MARGIN = 0.05f;
            _tmpBoundsOnModel.SetRect(allClippedDrawRect);
            _tmpBoundsOnModel.Expand(allClippedDrawRect->Width * MARGIN, allClippedDrawRect->Height * MARGIN);
            //########## 本来は割り当てられた領域の全体を使わず必要最低限のサイズがよい

            // シェーダ用の計算式を求める。回転を考慮しない場合は以下のとおり
            // movePeriod' = movePeriod * scaleX + offX		  [[ movePeriod' = (movePeriod - tmpBoundsOnModel.movePeriod)*scale + layoutBoundsOnTex01.movePeriod ]]
            const csmFloat32 scaleX = layoutBoundsOnTex01->Width / _tmpBoundsOnModel.Width;
            const csmFloat32 scaleY = layoutBoundsOnTex01->Height / _tmpBoundsOnModel.Height;

            // マスク生成時に使う行列を求める
            {
                // シェーダに渡す行列を求める <<<<<<<<<<<<<<<<<<<<<<<< 要最適化（逆順に計算すればシンプルにできる）
                _tmpMatrix.LoadIdentity();
                {
                    // Layout0..1 を -1..1に変換
                    _tmpMatrix.TranslateRelative(-1.0f, -1.0f);
                    _tmpMatrix.ScaleRelative(2.0f, 2.0f);
                }
                {
                    // view to Layout0..1
                    _tmpMatrix.TranslateRelative(layoutBoundsOnTex01->X, layoutBoundsOnTex01->Y); //new = [translate]
                    _tmpMatrix.ScaleRelative(scaleX, scaleY); //new = [translate][scale]
                    _tmpMatrix.TranslateRelative(-_tmpBoundsOnModel.X, -_tmpBoundsOnModel.Y);
                    //new = [translate][scale][translate]
                }
                // tmpMatrixForMask が計算結果
                _tmpMatrixForMask.SetMatrix(_tmpMatrix.GetArray());
            }

            //--------- draw時の mask 参照用行列を計算
            {
                // シェーダに渡す行列を求める <<<<<<<<<<<<<<<<<<<<<<<< 要最適化（逆順に計算すればシンプルにできる）
                _tmpMatrix.LoadIdentity();
                {
                    _tmpMatrix.TranslateRelative(layoutBoundsOnTex01->X, layoutBoundsOnTex01->Y); //new = [translate]
                    _tmpMatrix.ScaleRelative(scaleX, scaleY); //new = [translate][scale]
                    _tmpMatrix.TranslateRelative(-_tmpBoundsOnModel.X, -_tmpBoundsOnModel.Y);
                    //new = [translate][scale][translate]
                }

                _tmpMatrixForDraw.SetMatrix(_tmpMatrix.GetArray());
            }

            clipContext->_matrixForMask.SetMatrix(_tmpMatrixForMask.GetArray());

            clipContext->_matrixForDraw.SetMatrix(_tmpMatrixForDraw.GetArray());

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

                // 今回専用の変換を適用して描く
                // チャンネルも切り替える必要がある(A,R,G,B)
                renderer->SetClippingContextBufferForMask(clipContext);
                renderer->DrawMesh(
                    model.GetDrawableTextureIndices(clipDrawIndex),
                    model.GetDrawableVertexIndexCount(clipDrawIndex),
                    model.GetDrawableVertexCount(clipDrawIndex),
                    const_cast<csmUint16*>(model.GetDrawableVertexIndices(clipDrawIndex)),
                    const_cast<csmFloat32*>(model.GetDrawableVertices(clipDrawIndex)),
                    reinterpret_cast<csmFloat32*>(const_cast<Core::csmVector2*>(model.GetDrawableVertexUvs(clipDrawIndex))),
                    model.GetDrawableOpacity(clipDrawIndex),
                    CubismRenderer::CubismBlendMode::CubismBlendMode_Normal   //クリッピングは通常描画を強制
                );
            }
        }

        // --- 後処理 ---
        glBindFramebuffer(GL_FRAMEBUFFER, oldFBO); // 描画対象を戻す
        renderer->SetClippingContextBufferForMask(NULL);

        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }
}

void CubismClippingManager_OpenGLES2::CalcClippedDrawTotalBounds(CubismModel& model, CubismClippingContext* clippingContext)
{
    // 被クリッピングマスク（マスクされる描画オブジェクト）の全体の矩形
    csmFloat32 clippedDrawTotalMinX = FLT_MAX, clippedDrawTotalMinY = FLT_MAX;
    csmFloat32 clippedDrawTotalMaxX = FLT_MIN, clippedDrawTotalMaxY = FLT_MIN;

    // このマスクが実際に必要か判定する
    // このクリッピングを利用する「描画オブジェクト」がひとつでも使用可能であればマスクを生成する必要がある

    const csmInt32 clippedDrawCount = clippingContext->_clippedDrawableIndexList->GetSize();
    for (csmInt32 clippedDrawableIndex = 0; clippedDrawableIndex < clippedDrawCount; clippedDrawableIndex++)
    {
        // マスクを使用する描画オブジェクトの描画される矩形を求める
        const csmInt32 drawableIndex = (*clippingContext->_clippedDrawableIndexList)[clippedDrawableIndex];

        const csmInt32 drawableVertexCount = model.GetDrawableVertexCount(drawableIndex);
        csmFloat32* drawableVertexes = const_cast<csmFloat32*>(model.GetDrawableVertices(drawableIndex));

        csmFloat32 minX = FLT_MAX, minY = FLT_MAX;
        csmFloat32 maxX = FLT_MIN, maxY = FLT_MIN;

        csmInt32 loop = drawableVertexCount * Constant::VertexStep;
        for (csmInt32 pi = Constant::VertexOffset; pi < loop; pi += Constant::VertexStep)
        {
            csmFloat32 x = drawableVertexes[pi];
            csmFloat32 y = drawableVertexes[pi + 1];
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }

        //
        if (minX == FLT_MAX) continue; //有効な点がひとつも取れなかったのでスキップする

        // 全体の矩形に反映
        if (minX < clippedDrawTotalMinX) clippedDrawTotalMinX = minX;
        if (minY < clippedDrawTotalMinY) clippedDrawTotalMinY = minY;
        if (maxX > clippedDrawTotalMaxX) clippedDrawTotalMaxX = maxX;
        if (maxY > clippedDrawTotalMaxY) clippedDrawTotalMaxY = maxY;
    }
    if (clippedDrawTotalMinX == FLT_MAX)
    {
        clippingContext->_allClippedDrawRect->X = 0.0f;
        clippingContext->_allClippedDrawRect->Y = 0.0f;
        clippingContext->_allClippedDrawRect->Width = 0.0f;
        clippingContext->_allClippedDrawRect->Height = 0.0f;
        clippingContext->_isUsing = false;
    }
    else
    {
        clippingContext->_isUsing = true;
        csmFloat32 w = clippedDrawTotalMaxX - clippedDrawTotalMinX;
        csmFloat32 h = clippedDrawTotalMaxY - clippedDrawTotalMinY;
        clippingContext->_allClippedDrawRect->X = clippedDrawTotalMinX;
        clippingContext->_allClippedDrawRect->Y = clippedDrawTotalMinY;
        clippingContext->_allClippedDrawRect->Width = w;
        clippingContext->_allClippedDrawRect->Height = h;
    }
}

void CubismClippingManager_OpenGLES2::SetupLayoutBounds(csmInt32 usingClipCount) const
{
    // ひとつのRenderTextureを極力いっぱいに使ってマスクをレイアウトする
    // マスクグループの数が4以下ならRGBA各チャンネルに１つずつマスクを配置し、5以上6以下ならRGBAを2,2,1,1と配置する

    // RGBAを順番に使っていく。
    const csmInt32 div = usingClipCount / ColorChannelCount; //１チャンネルに配置する基本のマスク個数
    const csmInt32 mod = usingClipCount % ColorChannelCount; //余り、この番号のチャンネルまでに１つずつ配分する

    // RGBAそれぞれのチャンネルを用意していく(0:R , 1:G , 2:B, 3:A, )
    csmInt32 curClipIndex = 0; //順番に設定していく

    for (csmInt32 channelNo = 0; channelNo < ColorChannelCount; channelNo++)
    {
        // このチャンネルにレイアウトする数
        const csmInt32 layoutCount = div + (channelNo < mod ? 1 : 0);

        // 分割方法を決定する
        if (layoutCount == 0)
        {
            // 何もしない
        }
        else if (layoutCount == 1)
        {
            //全てをそのまま使う
            CubismClippingContext* cc = _clippingContextListForMask[curClipIndex++];
            cc->_layoutChannelNo = channelNo;
            cc->_layoutBounds->X = 0.0f;
            cc->_layoutBounds->Y = 0.0f;
            cc->_layoutBounds->Width = 1.0f;
            cc->_layoutBounds->Height = 1.0f;
        }
        else if (layoutCount == 2)
        {
            for (csmInt32 i = 0; i < layoutCount; i++)
            {
                const csmInt32 xpos = i % 2;

                CubismClippingContext* cc = _clippingContextListForMask[curClipIndex++];
                cc->_layoutChannelNo = channelNo;

                cc->_layoutBounds->X = xpos * 0.5f;
                cc->_layoutBounds->Y = 0.0f;
                cc->_layoutBounds->Width = 0.5f;
                cc->_layoutBounds->Height = 1.0f;
                //UVを2つに分解して使う
            }
        }
        else if (layoutCount <= 4)
        {
            //4分割して使う
            for (csmInt32 i = 0; i < layoutCount; i++)
            {
                const csmInt32 xpos = i % 2;
                const csmInt32 ypos = i / 2;

                CubismClippingContext* cc = _clippingContextListForMask[curClipIndex++];
                cc->_layoutChannelNo = channelNo;

                cc->_layoutBounds->X = xpos * 0.5f;
                cc->_layoutBounds->Y = ypos * 0.5f;
                cc->_layoutBounds->Width = 0.5f;
                cc->_layoutBounds->Height = 0.5f;
            }
        }
        else if (layoutCount <= 9)
        {
            //9分割して使う
            for (csmInt32 i = 0; i < layoutCount; i++)
            {
                const csmInt32 xpos = i % 3;
                const csmInt32 ypos = i / 3;

                CubismClippingContext* cc = _clippingContextListForMask[curClipIndex++];
                cc->_layoutChannelNo = channelNo;

                cc->_layoutBounds->X = xpos / 3.0f;
                cc->_layoutBounds->Y = ypos / 3.0f;
                cc->_layoutBounds->Width = 1.0f / 3.0f;
                cc->_layoutBounds->Height = 1.0f / 3.0f;
            }
        }
        else
        {
            CubismLogError("not supported mask count : %d", layoutCount);
        }
    }
}

CubismRenderer::CubismTextureColor* CubismClippingManager_OpenGLES2::GetChannelFlagAsColor(csmInt32 channelNo)
{
    return _channelColors[channelNo];
}

GLuint CubismClippingManager_OpenGLES2::GetColorBuffer() const
{
    return _colorBuffer;
}

csmVector<CubismClippingContext*>* CubismClippingManager_OpenGLES2::GetClippingContextListForDraw()
{
    return &_clippingContextListForDraw;
}

void CubismClippingManager_OpenGLES2::SetClippingMaskBufferSize(csmInt32 size)
{
    _clippingMaskBufferSize = size;
}

csmInt32 CubismClippingManager_OpenGLES2::GetClippingMaskBufferSize() const
{
    return _clippingMaskBufferSize;
}

/*********************************************************************************************************************
*                                      CubismClippingContext
********************************************************************************************************************/
CubismClippingContext::CubismClippingContext(CubismClippingManager_OpenGLES2* manager, const csmInt32* clippingDrawableIndices, csmInt32 clipCount)
{
    _owner = manager;

    // クリップしている（＝マスク用の）Drawableのインデックスリスト
    _clippingIdList = clippingDrawableIndices;

    // マスクの数
    _clippingIdCount = clipCount;

    _allClippedDrawRect = CSM_NEW csmRectF();
    _layoutBounds = CSM_NEW csmRectF();

    _clippedDrawableIndexList = CSM_NEW csmVector<csmInt32>();
}

CubismClippingContext::~CubismClippingContext()
{
    if (_layoutBounds != NULL)
    {
        CSM_DELETE(_layoutBounds);
        _layoutBounds = NULL;
    }

    if (_allClippedDrawRect != NULL)
    {
        CSM_DELETE(_allClippedDrawRect);
        _allClippedDrawRect = NULL;
    }

    if (_clippedDrawableIndexList != NULL)
    {
        CSM_DELETE(_clippedDrawableIndexList);
        _clippedDrawableIndexList = NULL;
    }
}

void CubismClippingContext::AddClippedDrawable(csmInt32 drawableIndex)
{
    _clippedDrawableIndexList->PushBack(drawableIndex);
}

CubismClippingManager_OpenGLES2* CubismClippingContext::GetClippingManager()
{
    return _owner;
}



/*********************************************************************************************************************
*                                      CubismDrawProfile_OpenGL
********************************************************************************************************************/
void CubismRendererProfile_OpenGLES2::SetGlEnable(GLenum index, GLboolean enabled)
{
    if (enabled == GL_TRUE) glEnable(index);
    else glDisable(index);
}

void CubismRendererProfile_OpenGLES2::SetGlEnableVertexAttribArray(GLuint index, GLint enabled)
{
    if (enabled) glEnableVertexAttribArray(index);
    else glDisableVertexAttribArray(index);
}

void CubismRendererProfile_OpenGLES2::Save()
{
    //-- push state --
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &_lastArrayBufferBinding);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &_lastElementArrayBufferBinding);
    glGetIntegerv(GL_CURRENT_PROGRAM, &_lastProgram);

    glGetIntegerv(GL_ACTIVE_TEXTURE, &_lastActiveTexture);
    glActiveTexture(GL_TEXTURE1); //テクスチャユニット1をアクティブに（以後の設定対象とする）
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &_lastTexture1Binding2D);

    glActiveTexture(GL_TEXTURE0); //テクスチャユニット0をアクティブに（以後の設定対象とする）
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &_lastTexture0Binding2D);

    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &_lastVertexAttribArrayEnabled[0]);
    glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &_lastVertexAttribArrayEnabled[1]);
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &_lastVertexAttribArrayEnabled[2]);
    glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &_lastVertexAttribArrayEnabled[3]);

    _lastScissorTest = glIsEnabled(GL_SCISSOR_TEST);
    _lastStencilTest = glIsEnabled(GL_STENCIL_TEST);
    _lastDepthTest = glIsEnabled(GL_DEPTH_TEST);
    _lastCullFace = glIsEnabled(GL_CULL_FACE);
    _lastBlend = glIsEnabled(GL_BLEND);

    glGetIntegerv(GL_FRONT_FACE, &_lastFrontFace);

    glGetBooleanv(GL_COLOR_WRITEMASK, _lastColorMask);

    // backup blending
    glGetIntegerv(GL_BLEND_SRC_RGB, &_lastBlending[0]);
    glGetIntegerv(GL_BLEND_DST_RGB, &_lastBlending[1]);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &_lastBlending[2]);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &_lastBlending[3]);
}

void CubismRendererProfile_OpenGLES2::Restore()
{
    glUseProgram(_lastProgram);

    SetGlEnableVertexAttribArray(0, _lastVertexAttribArrayEnabled[0]);
    SetGlEnableVertexAttribArray(1, _lastVertexAttribArrayEnabled[1]);
    SetGlEnableVertexAttribArray(2, _lastVertexAttribArrayEnabled[2]);
    SetGlEnableVertexAttribArray(3, _lastVertexAttribArrayEnabled[3]);

    SetGlEnable(GL_SCISSOR_TEST, _lastScissorTest);
    SetGlEnable(GL_STENCIL_TEST, _lastStencilTest);
    SetGlEnable(GL_DEPTH_TEST, _lastDepthTest);
    SetGlEnable(GL_CULL_FACE, _lastCullFace);
    SetGlEnable(GL_BLEND, _lastBlend);

    glFrontFace(_lastFrontFace);

    glColorMask(_lastColorMask[0], _lastColorMask[1], _lastColorMask[2], _lastColorMask[3]);

    glBindBuffer(GL_ARRAY_BUFFER, _lastArrayBufferBinding); //前にバッファがバインドされていたら破棄する必要がある
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _lastElementArrayBufferBinding);

    glActiveTexture(GL_TEXTURE1); //テクスチャユニット1を復元
    glBindTexture(GL_TEXTURE_2D, _lastTexture1Binding2D);

    glActiveTexture(GL_TEXTURE0); //テクスチャユニット0を復元
    glBindTexture(GL_TEXTURE_2D, _lastTexture0Binding2D);

    glActiveTexture(_lastActiveTexture);

    // restore blending
    glBlendFuncSeparate(_lastBlending[0], _lastBlending[1], _lastBlending[2], _lastBlending[3]);
}


/*********************************************************************************************************************
*                                       CubismShader_OpenGLES2
********************************************************************************************************************/
namespace {
    const csmInt32 ShaderCount = 13; ///< シェーダの数 = マスク生成用 + (通常 + 加算 + 乗算) * (マスク無 + マスク有 + マスク無の乗算済アルファ対応版 + マスク有の乗算済アルファ対応版)
    CubismShader_OpenGLES2* s_instance;
}

enum ShaderNames
{
    // SetupMask
    ShaderNames_SetupMask,

    //Normal
    ShaderNames_Normal,
    ShaderNames_NormalMasked,
    ShaderNames_NormalPremultipliedAlpha,
    ShaderNames_NormalMaskedPremultipliedAlpha,

    //Add
    ShaderNames_Add,
    ShaderNames_AddMasked,
    ShaderNames_AddPremultipliedAlpha,
    ShaderNames_AddMaskedPremultipliedAlpha,

    //Mult
    ShaderNames_Mult,
    ShaderNames_MultMasked,
    ShaderNames_MultPremultipliedAlpha,
    ShaderNames_MultMaskedPremultipliedAlpha,
};

void CubismShader_OpenGLES2::ReleaseShaderProgram()
{
    for (csmUint32 i = 0; i < _shaderSets.GetSize(); i++)
    {
        if (_shaderSets[i]->ShaderProgram)
        {
            glDeleteProgram(_shaderSets[i]->ShaderProgram);
            _shaderSets[i]->ShaderProgram = 0;
            CSM_DELETE(_shaderSets[i]);
        }
    }
}

// SetupMask
static const csmChar* VertShaderSrcSetupMask =
        "attribute vec4 a_position;"
        "attribute vec2 a_texCoord;"
        "varying vec2 v_texCoord;"
        "varying vec4 v_myPos;"
        "uniform mat4 u_clipMatrix;"
        "void main()"
        "{"
        "gl_Position = u_clipMatrix * a_position;"
        "v_myPos = u_clipMatrix * a_position;"
        "v_texCoord = a_texCoord;"
        "v_texCoord.y = 1.0 - v_texCoord.y;"
        "}";
static const csmChar* FragShaderSrcSetupMask =
#if defined(CSM_TARGET_ANDROID_ES2) || defined(CSM_TARGET_IPHONE_ES2)
        "precision mediump float;"
#endif
        "varying vec2 v_texCoord;"
        "varying vec4 v_myPos;"
        "uniform sampler2D s_texture0;"
        "uniform vec4 u_channelFlag;"
        "uniform vec4 u_baseColor;"
        "void main()"
        "{"
        "float isInside = "
        "  step(u_baseColor.x, v_myPos.x/v_myPos.w)"
        "* step(u_baseColor.y, v_myPos.y/v_myPos.w)"
        "* step(v_myPos.x/v_myPos.w, u_baseColor.z)"
        "* step(v_myPos.y/v_myPos.w, u_baseColor.w);"

        "gl_FragColor = u_channelFlag * texture2D(s_texture0 , v_texCoord).a * isInside;"
        "}";

//----- バーテックスシェーダプログラム -----
// Normal & Add & Mult 共通
static const csmChar* VertShaderSrc =
        "attribute vec4 a_position;" //v.vertex
        "attribute vec2 a_texCoord;" //v.texcoord
        "varying vec2 v_texCoord;" //v2f.texcoord
        "uniform mat4 u_matrix;"
        "void main()"
        "{"
        "gl_Position = u_matrix * a_position;"
        "v_texCoord = a_texCoord;"
        "v_texCoord.y = 1.0 - v_texCoord.y;"
        "}";

// Normal & Add & Mult 共通（クリッピングされたものの描画用）
static const csmChar* VertShaderSrcMasked =
        "attribute vec4 a_position;"
        "attribute vec2 a_texCoord;"
        "varying vec2 v_texCoord;"
        "varying vec4 v_clipPos;"
        "uniform mat4 u_matrix;"
        "uniform mat4 u_clipMatrix;"
        "void main()"
        "{"
        "gl_Position = u_matrix * a_position;"
        "v_clipPos = u_clipMatrix * a_position;"
        "v_texCoord = a_texCoord;"
        "v_texCoord.y = 1.0 - v_texCoord.y;"
        "}";

//----- フラグメントシェーダプログラム -----
// Normal & Add & Mult 共通
static const csmChar* FragShaderSrc =
#if defined(CSM_TARGET_ANDROID_ES2) || defined(CSM_TARGET_IPHONE_ES2)
        "precision mediump float;"
#endif
        "varying vec2 v_texCoord;" //v2f.texcoord
        "uniform sampler2D s_texture0;" //_MainTex
        "uniform vec4 u_baseColor;" //v2f.color
        "void main()"
        "{"
        "vec4 color = texture2D(s_texture0 , v_texCoord) * u_baseColor;"
        "gl_FragColor = vec4(color.rgb * color.a,  color.a);"
        "}";

// Normal & Add & Mult 共通 （PremultipliedAlpha）
static const csmChar* FragShaderSrcPremultipliedAlpha =
#if defined(CSM_TARGET_ANDROID_ES2) || defined(CSM_TARGET_IPHONE_ES2)
        "precision mediump float;"
#endif
        "varying vec2 v_texCoord;" //v2f.texcoord
        "uniform sampler2D s_texture0;" //_MainTex
        "uniform vec4 u_baseColor;" //v2f.color
        "void main()"
        "{"
        "gl_FragColor = texture2D(s_texture0 , v_texCoord) * u_baseColor;"
        "}";

// Normal （クリッピングされたものの描画用）
static const csmChar* FragShaderSrcNormalMask =
#if defined(CSM_TARGET_ANDROID_ES2) || defined(CSM_TARGET_IPHONE_ES2)
        "precision mediump float;"
#endif
        "varying vec2 v_texCoord;"
        "varying vec4 v_clipPos;"
        "uniform sampler2D s_texture0;"
        "uniform sampler2D s_texture1;"
        "uniform vec4 u_channelFlag;"
        "uniform vec4 u_baseColor;"
        "void main()"
        "{"
        "vec4 col_formask = texture2D(s_texture0 , v_texCoord) * u_baseColor;"
        "col_formask.rgb = col_formask.rgb  * col_formask.a ;"
        "vec4 clipMask = (1.0 - texture2D(s_texture1, v_clipPos.xy / v_clipPos.w)) * u_channelFlag;"
        "float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;"
        "col_formask = col_formask * maskVal;"
        "gl_FragColor = col_formask;"
        "}";

// Normal （クリッピングされたものの描画用、PremultipliedAlpha兼用）
static const csmChar* FragShaderSrcNormalMaskPremultipliedAlpha =
#if defined(CSM_TARGET_ANDROID_ES2) || defined(CSM_TARGET_IPHONE_ES2)
        "precision mediump float;"
#endif
        "varying vec2 v_texCoord;"
        "varying vec4 v_clipPos;"
        "uniform sampler2D s_texture0;"
        "uniform sampler2D s_texture1;"
        "uniform vec4 u_channelFlag;"
        "uniform vec4 u_baseColor;"
        "void main()"
        "{"
        "vec4 col_formask = texture2D(s_texture0 , v_texCoord) * u_baseColor;"
        "vec4 clipMask = (1.0 - texture2D(s_texture1, v_clipPos.xy / v_clipPos.w)) * u_channelFlag;"
        "float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;"
        "col_formask = col_formask * maskVal;"
        "gl_FragColor = col_formask;"
        "}";

// Add （クリッピングされたものの描画用）
static const csmChar* FragShaderSrcAddMask =
#if defined(CSM_TARGET_ANDROID_ES2) || defined(CSM_TARGET_IPHONE_ES2)
        "precision mediump float;"
#endif
        "varying vec2 v_texCoord;"
        "varying vec4 v_clipPos;"
        "uniform sampler2D s_texture0;"
        "uniform sampler2D s_texture1;"
        "uniform vec4 u_channelFlag;"
        "uniform vec4 u_baseColor;"
        "void main()"
        "{"
        "vec4 col_formask = texture2D(s_texture0 , v_texCoord) * u_baseColor;"
        "col_formask.rgb = col_formask.rgb  * col_formask.a ;"
        "vec4 clipMask = (1.0 - texture2D(s_texture1, v_clipPos.xy / v_clipPos.w)) * u_channelFlag;"
        "float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;"
        "col_formask.a = col_formask.a * maskVal;"
        "gl_FragColor = col_formask;"
        "}";

// Add （クリッピングされたものの描画用、テクスチャがPremultipliedAlphaの場合）
static const csmChar* FragShaderSrcAddMaskPremultipliedAlpha =
#if defined(CSM_TARGET_ANDROID_ES2) || defined(CSM_TARGET_IPHONE_ES2)
        "precision mediump float;"
#endif
        "varying vec2 v_texCoord;"
        "varying vec4 v_clipPos;"
        "uniform sampler2D s_texture0;"
        "uniform sampler2D s_texture1;"
        "uniform vec4 u_channelFlag;"
        "uniform vec4 u_baseColor;"
        "void main()"
        "{"
        "vec4 col_formask = texture2D(s_texture0 , v_texCoord) * u_baseColor;"
        "vec4 clipMask = (1.0 - texture2D(s_texture1, v_clipPos.xy / v_clipPos.w)) * u_channelFlag;"
        "float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;"
        "col_formask = col_formask * maskVal;"
        "gl_FragColor = col_formask;"
        "}";

// Mult （クリッピングされたものの描画用）
static const csmChar* FragShaderSrcMultMask =
#if defined(CSM_TARGET_ANDROID_ES2) || defined(CSM_TARGET_IPHONE_ES2)
        "precision mediump float;"
#endif
        "varying vec2 v_texCoord;"
        "varying vec4 v_clipPos;"
        "uniform sampler2D s_texture0;"
        "uniform sampler2D s_texture1;"
        "uniform vec4 u_channelFlag;"
        "uniform vec4 u_baseColor;"
        "void main()"
        "{"
        "vec4 col_formask = texture2D(s_texture0 , v_texCoord) * u_baseColor;"
        "col_formask.rgb = col_formask.rgb  * col_formask.a ;"
        "vec4 clipMask = (1.0 - texture2D(s_texture1, v_clipPos.xy / v_clipPos.w)) * u_channelFlag;"
        "float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;"
        "col_formask = col_formask * maskVal;"
        "gl_FragColor = col_formask;"
        "}";

// Mult （クリッピングされたものの描画用、テクスチャがPremultipliedAlphaの場合）
static const csmChar* FragShaderSrcMultMaskPremultipliedAlpha =
#if defined(CSM_TARGET_ANDROID_ES2) || defined(CSM_TARGET_IPHONE_ES2)
        "precision mediump float;"
#endif
        "varying vec2 v_texCoord;"
        "varying vec4 v_clipPos;"
        "uniform sampler2D s_texture0;"
        "uniform sampler2D s_texture1;"
        "uniform vec4 u_channelFlag;"
        "uniform vec4 u_baseColor;" //v2f.color
        "void main()"
        "{"
        "vec4 tex = texture2D(s_texture0 , v_texCoord) * u_baseColor;"
        "vec4 col_formask = tex;"
        "vec4 clipMask = (1.0 - texture2D(s_texture1, v_clipPos.xy / v_clipPos.w)) * u_channelFlag;"
        "float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;"
        "col_formask = col_formask * maskVal;"
        "gl_FragColor = col_formask;"
        "}";

//テクスチャを使わないデバッグ用の表示
static const csmChar* VertShaderSrcDebug =
        "varying lowp vec4 colorVarying;"
        "void main()"
        "{"
        "gl_Position = a_position;vec4 diffuseColor = vec4(0.0, 0.0 , 1.0, 0.5);"
        "colorVarying = diffuseColor ;"
        "}";

static const csmChar* FragShaderSrcDebug =
#if defined(CSM_TARGET_ANDROID_ES2) || defined(CSM_TARGET_IPHONE_ES2)
        "precision mediump float;"
#endif
        "varying lowp vec4 colorVarying; "
        "void main()"
        "{"
        "gl_FragColor = colorVarying;"
        "}";

#ifdef CSM_TARGET_ANDROID_ES2
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//----- Tegra用フラグメントシェーダプログラム -----

    // マスク生成用
    const static csmChar* FragShaderSrcSetupMaskForTegra =
        "#extension GL_NV_shader_framebuffer_fetch : enable\n"

        "varying vec2 v_texCoord;"
        "varying vec4 v_myPos;"
        "uniform sampler2D s_texture0;"
        "uniform vec4 u_channelFlag;"
        "uniform vec4 u_baseColor;"
        "void main()"
        "{"
        "float isInside = "
        "  step(u_baseColor.x, v_myPos.x/v_myPos.w)"
        "* step(u_baseColor.y, v_myPos.y/v_myPos.w)"
        "* step(v_myPos.x/v_myPos.w, u_baseColor.z)"
        "* step(v_myPos.y/v_myPos.w, u_baseColor.w);"
        "vec4 Cs = u_channelFlag * texture2D(s_texture0 , v_texCoord).a * isInside;"
        "float As = Cs.a;"
        "Cs.r = gl_LastFragColor.r*(1.0-Cs.r);"
        "Cs.g = gl_LastFragColor.g*(1.0-Cs.g);"
        "Cs.b = gl_LastFragColor.b*(1.0-Cs.b);"
        "Cs.a = gl_LastFragColor.a*(1.0-As);"
        "gl_FragColor = Cs;"
        "}";


    // 共通（上）
#define  FragShaderSrcHeaderForTegra \
        "#extension GL_NV_shader_framebuffer_fetch : enable\n"\
        \
        "varying vec2 v_texCoord;"\
        "uniform sampler2D s_texture0;"\
        "uniform vec4 u_baseColor;"\
        "void main()"\
        "{"\
            "vec4 Cs = texture2D(s_texture0 , v_texCoord) * u_baseColor;"\
            "float As = Cs.a;"
    // 共通（下）
#define FragShaderSrcFooterForTegra \
            "gl_FragColor = Cs ;"\
        "}"

    // 共通：マスク（上）
#define  FragShaderSrcHeaderMaskForTegra \
        "#extension GL_NV_shader_framebuffer_fetch : enable\n"\
        \
        "varying vec2 v_texCoord;"\
        "varying vec4 v_clipPos;"\
        "uniform sampler2D s_texture0;"\
        "uniform sampler2D s_texture1;"\
        "uniform vec4 u_channelFlag;"\
        "uniform vec4 u_baseColor;"\
        "void main()"\
        "{"\
            "vec4 col_formask = texture2D(s_texture0 , v_texCoord) * u_baseColor;"\
            "float As = col_formask.a;"

    // 共通：マスク（下）
#define  FragShaderSrcFooterMaskForTegra \
        "vec4 clipMask = (1.0 - texture2D(s_texture1, v_clipPos.xy / v_clipPos.w)) * u_channelFlag;"\
        "float maskVal = clipMask.r + clipMask.g + clipMask.b + clipMask.a;"\
        "col_formask = col_formask * maskVal;"\
        "gl_FragColor = col_formask;"\
        "}"




    // Normal
#define  FragShaderSrcNormalForTegra \
        "col_formask.r = col_formask.r * As + gl_LastFragColor.r*(1.0-As);"\
        "col_formask.g = col_formask.g * As + gl_LastFragColor.g*(1.0-As);"\
        "col_formask.b = col_formask.b * As + gl_LastFragColor.b*(1.0-As);"\
        "col_formask.a = col_formask.a      + gl_LastFragColor.a*(1.0-As);"

    // Normal （PremultipliedAlpha）
#define  FragShaderSrcNormalPremultipliedAlphaForTegra \
        "col_formask.r = col_formask.r + gl_LastFragColor.r*(1.0-As);"\
        "col_formask.g = col_formask.g + gl_LastFragColor.g*(1.0-As);"\
        "col_formask.b = col_formask.b + gl_LastFragColor.b*(1.0-As);"\
        "col_formask.a = col_formask.a + gl_LastFragColor.a*(1.0-As);"

    // Add
#define  FragShaderSrcAddForTegra \
        "col_formask.r = col_formask.r * As + gl_LastFragColor.r;"\
        "col_formask.g = col_formask.g * As + gl_LastFragColor.g;"\
        "col_formask.b = col_formask.b * As + gl_LastFragColor.b;"\
        "col_formask.a = gl_LastFragColor.a;"

    // Add （PremultipliedAlpha）
#define  FragShaderSrcAddPremultipliedAlphaForTegra \
        "col_formask.r = col_formask.r + gl_LastFragColor.r;"\
        "col_formask.g = col_formask.g + gl_LastFragColor.g;"\
        "col_formask.b = col_formask.b + gl_LastFragColor.b;"\
        "col_formask.a = gl_LastFragColor.a;"

    // Mult
#define  FragShaderSrcMultForTegra \
        "col_formask.r = col_formask.r * gl_LastFragColor.r * As + gl_LastFragColor.r*(1.0-As);"\
        "col_formask.g = col_formask.g * gl_LastFragColor.g * As + gl_LastFragColor.g*(1.0-As);"\
        "col_formask.b = col_formask.b * gl_LastFragColor.b * As + gl_LastFragColor.b*(1.0-As);"\
        "col_formask.a = gl_LastFragColor.a;"

    // Mult （PremultipliedAlpha）
#define  FragShaderSrcMultPremultipliedAlphaForTegra \
        "col_formask.r = col_formask.r * gl_LastFragColor.r + gl_LastFragColor.r*(1.0-As);"\
        "col_formask.g = col_formask.g * gl_LastFragColor.g + gl_LastFragColor.g*(1.0-As);"\
        "col_formask.b = col_formask.b * gl_LastFragColor.b + gl_LastFragColor.b*(1.0-As);"\
        "col_formask.a = gl_LastFragColor.a;"

    static const csmChar * FragSrcNormal = FragShaderSrcHeaderForTegra FragShaderSrcNormalForTegra FragShaderSrcFooterForTegra;
    static const csmChar * FragSrcNormalMask = FragShaderSrcHeaderMaskForTegra FragShaderSrcNormalForTegra FragShaderSrcFooterMaskForTegra;
    static const csmChar * FragSrcNormalPremultipliedAlpha = FragShaderSrcHeaderForTegra FragShaderSrcNormalPremultipliedAlphaForTegra FragShaderSrcFooterForTegra;
    static const csmChar * FragSrcNormalMaskPremultipliedAlpha = FragShaderSrcHeaderMaskForTegra FragShaderSrcNormalPremultipliedAlphaForTegra FragShaderSrcFooterMaskForTegra;

    static const csmChar * FragSrcAdd = FragShaderSrcHeaderForTegra FragShaderSrcAddForTegra FragShaderSrcFooterForTegra;
    static const csmChar * FragSrcAddMask = FragShaderSrcHeaderMaskForTegra FragShaderSrcAddForTegra FragShaderSrcFooterMaskForTegra;
    static const csmChar * FragSrcAddPremultipliedAlpha = FragShaderSrcHeaderForTegra FragShaderSrcAddPremultipliedAlphaForTegra FragShaderSrcFooterForTegra;
    static const csmChar * FragSrcAddMaskPremultipliedAlpha = FragShaderSrcHeaderMaskForTegra FragShaderSrcAddPremultipliedAlphaForTegra FragShaderSrcFooterMaskForTegra;

    static const csmChar * FragSrcMult = FragShaderSrcHeaderForTegra FragShaderSrcMultForTegra FragShaderSrcFooterForTegra;
    static const csmChar * FragSrcMultMask = FragShaderSrcHeaderMaskForTegra FragShaderSrcMultForTegra FragShaderSrcFooterMaskForTegra;
    static const csmChar * FragSrcMultPremultipliedAlpha = FragShaderSrcHeaderForTegra FragShaderSrcMultPremultipliedAlphaForTegra FragShaderSrcFooterForTegra;
    static const csmChar * FragSrcMultMaskPremultipliedAlpha = FragShaderSrcHeaderMaskForTegra FragShaderSrcMultPremultipliedAlphaForTegra FragShaderSrcFooterMaskForTegra;

#endif

CubismShader_OpenGLES2::CubismShader_OpenGLES2()
{ }

CubismShader_OpenGLES2::~CubismShader_OpenGLES2()
{
    ReleaseShaderProgram();
}

CubismShader_OpenGLES2* CubismShader_OpenGLES2::GetInstance()
{
    if (s_instance == NULL)
    {
        s_instance = CSM_NEW CubismShader_OpenGLES2();
    }
    return s_instance;
}

void CubismShader_OpenGLES2::DeleteInstance()
{
    if (s_instance)
    {
        CSM_DELETE_SELF(CubismShader_OpenGLES2, s_instance);
        s_instance = NULL;
    }
}

#ifdef CSM_TARGET_ANDROID_ES2
csmBool CubismShader_OpenGLES2::s_extMode = false;
csmBool CubismShader_OpenGLES2::s_extPAMode = false;
void CubismShader_OpenGLES2::SetExtShaderMode(csmBool extMode, csmBool extPAMode) {
    s_extMode = extMode;
    s_extPAMode = extPAMode;
}
#endif

void CubismShader_OpenGLES2::GenerateShaders()
{
    for (csmInt32 i = 0; i < ShaderCount; i++)
    {
        _shaderSets.PushBack(CSM_NEW CubismShaderSet());
    }

#ifdef CSM_TARGET_ANDROID_ES2
    if (s_extMode)
    {
        _shaderSets[0]->ShaderProgram = LoadShaderProgram(VertShaderSrcSetupMask, FragShaderSrcSetupMaskForTegra);

        _shaderSets[1]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragSrcNormal);
        _shaderSets[2]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragSrcNormalMask);
        _shaderSets[3]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragSrcNormalPremultipliedAlpha);
        _shaderSets[4]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragSrcNormalMaskPremultipliedAlpha);

        _shaderSets[5]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragSrcAdd);
        _shaderSets[6]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragSrcAddMask);
        _shaderSets[7]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragSrcAddPremultipliedAlpha);
        _shaderSets[8]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragSrcAddMaskPremultipliedAlpha);

        _shaderSets[9]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragSrcMult);
        _shaderSets[10]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragSrcMultMask);
        _shaderSets[11]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragSrcMultMask);
        _shaderSets[12]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragSrcMultMaskPremultipliedAlpha);
    }
    else
    {
        _shaderSets[0]->ShaderProgram = LoadShaderProgram(VertShaderSrcSetupMask, FragShaderSrcSetupMask);

        _shaderSets[1]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrc);
        _shaderSets[2]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcNormalMask);
        _shaderSets[3]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrcPremultipliedAlpha);
        _shaderSets[4]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcNormalMaskPremultipliedAlpha);

        _shaderSets[5]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrc);
        _shaderSets[6]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcAddMask);
        _shaderSets[7]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrcPremultipliedAlpha);
        _shaderSets[8]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcAddMaskPremultipliedAlpha);

        _shaderSets[9]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrc);
        _shaderSets[10]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcMultMask);
        _shaderSets[11]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrcPremultipliedAlpha);
        _shaderSets[12]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcMultMaskPremultipliedAlpha);
    }

#else

    _shaderSets[0]->ShaderProgram = LoadShaderProgram(VertShaderSrcSetupMask, FragShaderSrcSetupMask);

    _shaderSets[1]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrc);
    _shaderSets[2]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcNormalMask);
    _shaderSets[3]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrcPremultipliedAlpha);
    _shaderSets[4]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcNormalMaskPremultipliedAlpha);

    _shaderSets[5]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrc);
    _shaderSets[6]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcAddMask);
    _shaderSets[7]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrcPremultipliedAlpha);
    _shaderSets[8]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcAddMaskPremultipliedAlpha);

    _shaderSets[9]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrc);
    _shaderSets[10]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcMultMask);
    _shaderSets[11]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrcPremultipliedAlpha);
    _shaderSets[12]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcMultMaskPremultipliedAlpha);
#endif

    // SetupMask
    _shaderSets[0]->AttributePositionLocation = glGetAttribLocation(_shaderSets[0]->ShaderProgram, "a_position");
    _shaderSets[0]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[0]->ShaderProgram, "a_texCoord");
    _shaderSets[0]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[0]->ShaderProgram, "s_texture0");
    _shaderSets[0]->UniformClipMatrixLocation = glGetUniformLocation(_shaderSets[0]->ShaderProgram, "u_clipMatrix");
    _shaderSets[0]->UnifromChannelFlagLocation = glGetUniformLocation(_shaderSets[0]->ShaderProgram, "u_channelFlag");
    _shaderSets[0]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[0]->ShaderProgram, "u_baseColor");

    // 通常
    _shaderSets[1]->AttributePositionLocation = glGetAttribLocation(_shaderSets[1]->ShaderProgram, "a_position");
    _shaderSets[1]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[1]->ShaderProgram, "a_texCoord");
    _shaderSets[1]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[1]->ShaderProgram, "s_texture0");
    _shaderSets[1]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[1]->ShaderProgram, "u_matrix");
    _shaderSets[1]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[1]->ShaderProgram, "u_baseColor");

    // 通常（クリッピング）
    _shaderSets[2]->AttributePositionLocation = glGetAttribLocation(_shaderSets[2]->ShaderProgram, "a_position");
    _shaderSets[2]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[2]->ShaderProgram, "a_texCoord");
    _shaderSets[2]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[2]->ShaderProgram, "s_texture0");
    _shaderSets[2]->SamplerTexture1Location = glGetUniformLocation(_shaderSets[2]->ShaderProgram, "s_texture1");
    _shaderSets[2]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[2]->ShaderProgram, "u_matrix");
    _shaderSets[2]->UniformClipMatrixLocation = glGetUniformLocation(_shaderSets[2]->ShaderProgram, "u_clipMatrix");
    _shaderSets[2]->UnifromChannelFlagLocation = glGetUniformLocation(_shaderSets[2]->ShaderProgram, "u_channelFlag");
    _shaderSets[2]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[2]->ShaderProgram, "u_baseColor");

    // 通常（PremultipliedAlpha）
    _shaderSets[3]->AttributePositionLocation = glGetAttribLocation(_shaderSets[3]->ShaderProgram, "a_position");
    _shaderSets[3]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[3]->ShaderProgram, "a_texCoord");
    _shaderSets[3]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[3]->ShaderProgram, "s_texture0");
    _shaderSets[3]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[3]->ShaderProgram, "u_matrix");
    _shaderSets[3]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[3]->ShaderProgram, "u_baseColor");

    // 通常（クリッピング、PremultipliedAlpha）
    _shaderSets[4]->AttributePositionLocation = glGetAttribLocation(_shaderSets[4]->ShaderProgram, "a_position");
    _shaderSets[4]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[4]->ShaderProgram, "a_texCoord");
    _shaderSets[4]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[4]->ShaderProgram, "s_texture0");
    _shaderSets[4]->SamplerTexture1Location = glGetUniformLocation(_shaderSets[4]->ShaderProgram, "s_texture1");
    _shaderSets[4]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[4]->ShaderProgram, "u_matrix");
    _shaderSets[4]->UniformClipMatrixLocation = glGetUniformLocation(_shaderSets[4]->ShaderProgram, "u_clipMatrix");
    _shaderSets[4]->UnifromChannelFlagLocation = glGetUniformLocation(_shaderSets[4]->ShaderProgram, "u_channelFlag");
    _shaderSets[4]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[4]->ShaderProgram, "u_baseColor");

    // 加算
    _shaderSets[5]->AttributePositionLocation = glGetAttribLocation(_shaderSets[5]->ShaderProgram, "a_position");
    _shaderSets[5]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[5]->ShaderProgram, "a_texCoord");
    _shaderSets[5]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[5]->ShaderProgram, "s_texture0");
    _shaderSets[5]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[5]->ShaderProgram, "u_matrix");
    _shaderSets[5]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[5]->ShaderProgram, "u_baseColor");

    // 加算（クリッピング）
    _shaderSets[6]->AttributePositionLocation = glGetAttribLocation(_shaderSets[6]->ShaderProgram, "a_position");
    _shaderSets[6]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[6]->ShaderProgram, "a_texCoord");
    _shaderSets[6]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[6]->ShaderProgram, "s_texture0");
    _shaderSets[6]->SamplerTexture1Location = glGetUniformLocation(_shaderSets[6]->ShaderProgram, "s_texture1");
    _shaderSets[6]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[6]->ShaderProgram, "u_matrix");
    _shaderSets[6]->UniformClipMatrixLocation = glGetUniformLocation(_shaderSets[6]->ShaderProgram, "u_clipMatrix");
    _shaderSets[6]->UnifromChannelFlagLocation = glGetUniformLocation(_shaderSets[6]->ShaderProgram, "u_channelFlag");
    _shaderSets[6]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[6]->ShaderProgram, "u_baseColor");

    // 加算（PremultipliedAlpha）
    _shaderSets[7]->AttributePositionLocation = glGetAttribLocation(_shaderSets[7]->ShaderProgram, "a_position");
    _shaderSets[7]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[7]->ShaderProgram, "a_texCoord");
    _shaderSets[7]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[7]->ShaderProgram, "s_texture0");
    _shaderSets[7]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[7]->ShaderProgram, "u_matrix");
    _shaderSets[7]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[7]->ShaderProgram, "u_baseColor");

    // 加算（クリッピング、PremultipliedAlpha）
    _shaderSets[8]->AttributePositionLocation = glGetAttribLocation(_shaderSets[8]->ShaderProgram, "a_position");
    _shaderSets[8]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[8]->ShaderProgram, "a_texCoord");
    _shaderSets[8]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[8]->ShaderProgram, "s_texture0");
    _shaderSets[8]->SamplerTexture1Location = glGetUniformLocation(_shaderSets[8]->ShaderProgram, "s_texture1");
    _shaderSets[8]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[8]->ShaderProgram, "u_matrix");
    _shaderSets[8]->UniformClipMatrixLocation = glGetUniformLocation(_shaderSets[8]->ShaderProgram, "u_clipMatrix");
    _shaderSets[8]->UnifromChannelFlagLocation = glGetUniformLocation(_shaderSets[8]->ShaderProgram, "u_channelFlag");
    _shaderSets[8]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[8]->ShaderProgram, "u_baseColor");

    // 乗算
    _shaderSets[9]->AttributePositionLocation = glGetAttribLocation(_shaderSets[9]->ShaderProgram, "a_position");
    _shaderSets[9]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[9]->ShaderProgram, "a_texCoord");
    _shaderSets[9]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[9]->ShaderProgram, "s_texture0");
    _shaderSets[9]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[9]->ShaderProgram, "u_matrix");
    _shaderSets[9]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[9]->ShaderProgram, "u_baseColor");

    // 乗算（クリッピング）
    _shaderSets[10]->AttributePositionLocation = glGetAttribLocation(_shaderSets[10]->ShaderProgram, "a_position");
    _shaderSets[10]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[10]->ShaderProgram, "a_texCoord");
    _shaderSets[10]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[10]->ShaderProgram, "s_texture0");
    _shaderSets[10]->SamplerTexture1Location = glGetUniformLocation(_shaderSets[10]->ShaderProgram, "s_texture1");
    _shaderSets[10]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[10]->ShaderProgram, "u_matrix");
    _shaderSets[10]->UniformClipMatrixLocation = glGetUniformLocation(_shaderSets[10]->ShaderProgram, "u_clipMatrix");
    _shaderSets[10]->UnifromChannelFlagLocation = glGetUniformLocation(_shaderSets[10]->ShaderProgram, "u_channelFlag");
    _shaderSets[10]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[10]->ShaderProgram, "u_baseColor");

    // 乗算（PremultipliedAlpha）
    _shaderSets[11]->AttributePositionLocation = glGetAttribLocation(_shaderSets[11]->ShaderProgram, "a_position");
    _shaderSets[11]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[11]->ShaderProgram, "a_texCoord");
    _shaderSets[11]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[11]->ShaderProgram, "s_texture0");
    _shaderSets[11]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[11]->ShaderProgram, "u_matrix");
    _shaderSets[11]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[11]->ShaderProgram, "u_baseColor");

    // 乗算（クリッピング、PremultipliedAlpha）
    _shaderSets[12]->AttributePositionLocation = glGetAttribLocation(_shaderSets[12]->ShaderProgram, "a_position");
    _shaderSets[12]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[12]->ShaderProgram, "a_texCoord");
    _shaderSets[12]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[12]->ShaderProgram, "s_texture0");
    _shaderSets[12]->SamplerTexture1Location = glGetUniformLocation(_shaderSets[12]->ShaderProgram, "s_texture1");
    _shaderSets[12]->UniformMatrixLocation = glGetUniformLocation(_shaderSets[12]->ShaderProgram, "u_matrix");
    _shaderSets[12]->UniformClipMatrixLocation = glGetUniformLocation(_shaderSets[12]->ShaderProgram, "u_clipMatrix");
    _shaderSets[12]->UnifromChannelFlagLocation = glGetUniformLocation(_shaderSets[12]->ShaderProgram, "u_channelFlag");
    _shaderSets[12]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[12]->ShaderProgram, "u_baseColor");
}

void CubismShader_OpenGLES2::SetupShaderProgram(CubismRenderer_OpenGLES2* renderer, GLuint textureId
                                                , csmInt32 vertexCount, csmFloat32* vertexArray
                                                , csmFloat32* uvArray, csmFloat32 opacity
                                                , CubismRenderer::CubismBlendMode colorBlendMode
                                                , CubismRenderer::CubismTextureColor baseColor
                                                , csmBool isPremultipliedAlpha, CubismMatrix44 matrix4x4)
{
    if (_shaderSets.GetSize() == 0)
    {
        GenerateShaders();
    }

    // Blending
    csmInt32 SRC_COLOR;
    csmInt32 DST_COLOR;
    csmInt32 SRC_ALPHA;
    csmInt32 DST_ALPHA;

    if (renderer->GetClippingContextBufferForMask() != NULL) // マスク生成時
    {
        CubismShaderSet* shaderSet = _shaderSets[ShaderNames_SetupMask];
        glUseProgram(shaderSet->ShaderProgram);

        //テクスチャ設定
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glUniform1i(shaderSet->SamplerTexture0Location, 0);

        // 頂点配列の設定
        glEnableVertexAttribArray(shaderSet->AttributePositionLocation);
        glVertexAttribPointer(shaderSet->AttributePositionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(csmFloat32) * 2, vertexArray);
        // テクスチャ頂点の設定
        glEnableVertexAttribArray(shaderSet->AttributeTexCoordLocation);
        glVertexAttribPointer(shaderSet->AttributeTexCoordLocation, 2, GL_FLOAT, GL_FALSE, sizeof(csmFloat32) * 2, uvArray);

        // チャンネル
        const csmInt32 channelNo = renderer->GetClippingContextBufferForMask()->_layoutChannelNo;
        CubismRenderer::CubismTextureColor* colorChannel = renderer->GetClippingContextBufferForMask()->GetClippingManager()->GetChannelFlagAsColor(channelNo);
        glUniform4f(shaderSet->UnifromChannelFlagLocation, colorChannel->R, colorChannel->G, colorChannel->B, colorChannel->A);

        glUniformMatrix4fv(shaderSet->UniformClipMatrixLocation, 1, GL_FALSE, renderer->GetClippingContextBufferForMask()->_matrixForMask.GetArray());

        csmRectF* rect = renderer->GetClippingContextBufferForMask()->_layoutBounds;

        glUniform4f(shaderSet->UniformBaseColorLocation,
                    rect->X * 2.0f - 1.0f,
                    rect->Y * 2.0f - 1.0f,
                    rect->GetRight() * 2.0f - 1.0f,
                    rect->GetBottom() * 2.0f - 1.0f);

        SRC_COLOR = GL_ZERO;
        DST_COLOR = GL_ONE_MINUS_SRC_COLOR;
        SRC_ALPHA = GL_ZERO;
        DST_ALPHA = GL_ONE_MINUS_SRC_ALPHA;
    }
    else // マスク生成以外の場合
    {
        const csmBool masked = renderer->GetClippingContextBufferForDraw() != NULL;  // この描画オブジェクトはマスク対象か
        const csmInt32 offset = (masked ? 1 : 0) + (isPremultipliedAlpha ? 2 : 0);

        CubismShaderSet* shaderSet;
        switch (colorBlendMode)
        {
        case CubismRenderer::CubismBlendMode::CubismBlendMode_Normal:
        default:
            shaderSet = _shaderSets[ShaderNames_Normal + offset];
            SRC_COLOR = GL_ONE;
            DST_COLOR = GL_ONE_MINUS_SRC_ALPHA;
            SRC_ALPHA = GL_ONE;
            DST_ALPHA = GL_ONE_MINUS_SRC_ALPHA;
            break;

        case CubismRenderer::CubismBlendMode::CubismBlendMode_Additive:
            shaderSet = _shaderSets[ShaderNames_Add + offset];
            SRC_COLOR = GL_ONE;
            DST_COLOR = GL_ONE;
            SRC_ALPHA = GL_ZERO;
            DST_ALPHA = GL_ONE;
            break;

        case CubismRenderer::CubismBlendMode::CubismBlendMode_Multiplicative:
            shaderSet = _shaderSets[ShaderNames_Mult + offset];
            SRC_COLOR = GL_DST_COLOR;
            DST_COLOR = GL_ONE_MINUS_SRC_ALPHA;
            SRC_ALPHA = GL_ZERO;
            DST_ALPHA = GL_ONE;
            break;
        }

        glUseProgram(shaderSet->ShaderProgram);

        // 頂点配列の設定
        glEnableVertexAttribArray(shaderSet->AttributePositionLocation);
        glVertexAttribPointer(shaderSet->AttributePositionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(csmFloat32) * 2, vertexArray);
        // テクスチャ頂点の設定
        glEnableVertexAttribArray(shaderSet->AttributeTexCoordLocation);
        glVertexAttribPointer(shaderSet->AttributeTexCoordLocation, 2, GL_FLOAT, GL_FALSE, sizeof(csmFloat32) * 2, uvArray);

        if (masked)
        {
            glActiveTexture(GL_TEXTURE1);
            GLuint tex = renderer->GetClippingContextBufferForDraw()->GetClippingManager()->GetColorBuffer();
            glBindTexture(GL_TEXTURE_2D, tex);
            glUniform1i(shaderSet->SamplerTexture1Location, 1);

            // View座標をClippingContextの座標に変換するための行列を設定
            glUniformMatrix4fv(shaderSet->UniformClipMatrixLocation, 1, 0, renderer->GetClippingContextBufferForDraw()->_matrixForDraw.GetArray());

            // 使用するカラーチャンネルを設定
            const csmInt32 channelNo = renderer->GetClippingContextBufferForDraw()->_layoutChannelNo;
            CubismRenderer::CubismTextureColor* colorChannel = renderer->GetClippingContextBufferForDraw()->GetClippingManager()->GetChannelFlagAsColor(channelNo);
            glUniform4f(shaderSet->UnifromChannelFlagLocation, colorChannel->R, colorChannel->G, colorChannel->B, colorChannel->A);
        }

        //テクスチャ設定
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glUniform1i(shaderSet->SamplerTexture0Location, 0);

        //座標変換
        glUniformMatrix4fv(shaderSet->UniformMatrixLocation, 1, 0, matrix4x4.GetArray()); //

        glUniform4f(shaderSet->UniformBaseColorLocation, baseColor.R, baseColor.G, baseColor.B, baseColor.A);
    }

    glBlendFuncSeparate(SRC_COLOR, DST_COLOR, SRC_ALPHA, DST_ALPHA);
}

csmBool CubismShader_OpenGLES2::CompileShaderSource(GLuint* outShader, GLenum shaderType, const csmChar* shaderSource)
{
    GLint status;
    const GLchar* source = shaderSource;

    *outShader = glCreateShader(shaderType);
    glShaderSource(*outShader, 1, &source, NULL);
    glCompileShader(*outShader);

    GLint logLength;
    glGetShaderiv(*outShader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar* log = reinterpret_cast<GLchar*>(CSM_MALLOC(logLength));
        glGetShaderInfoLog(*outShader, logLength, &logLength, log);
        CubismLogError("Shader compile log: %s", log);
        CSM_FREE(log);
    }

    glGetShaderiv(*outShader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        glDeleteShader(*outShader);
        return false;
    }

    return true;
}

csmBool CubismShader_OpenGLES2::LinkProgram(GLuint shaderProgram)
{
    GLint status;
    glLinkProgram(shaderProgram);

    GLint logLength;
    glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar* log = reinterpret_cast<GLchar*>(CSM_MALLOC(logLength));
        glGetProgramInfoLog(shaderProgram, logLength, &logLength, log);
        CubismLogError("Program link log: %s", log);
        CSM_FREE(log);
    }

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        return false;
    }

    return true;
}

csmBool CubismShader_OpenGLES2::ValidateProgram(GLuint shaderProgram)
{
    GLint logLength, status;

    glValidateProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar* log = reinterpret_cast<GLchar*>(CSM_MALLOC(logLength));
        glGetProgramInfoLog(shaderProgram, logLength, &logLength, log);
        CubismLogError("Validate program log: %s", log);
        CSM_FREE(log);
    }

    glGetProgramiv(shaderProgram, GL_VALIDATE_STATUS, &status);
    if (status == GL_FALSE)
    {
        return false;
    }

    return true;
}

GLuint CubismShader_OpenGLES2::LoadShaderProgram(const csmChar* vertShaderSrc, const csmChar* fragShaderSrc)
{
    GLuint vertShader, fragShader;

    // Create shader program.
    GLuint shaderProgram = glCreateProgram();

    if (!CompileShaderSource(&vertShader, GL_VERTEX_SHADER, vertShaderSrc))
    {
        CubismLogError("Vertex shader compile error!");
        return 0;
    }

    // Create and compile fragment shader.
    if (!CompileShaderSource(&fragShader, GL_FRAGMENT_SHADER, fragShaderSrc))
    {
        CubismLogError("Fragment shader compile error!");
        return 0;
    }

    // Attach vertex shader to program.
    glAttachShader(shaderProgram, vertShader);

    // Attach fragment shader to program.
    glAttachShader(shaderProgram, fragShader);

    // Link program.
    if (!LinkProgram(shaderProgram))
    {
        CubismLogError("Failed to link program: %d", shaderProgram);

        if (vertShader)
        {
            glDeleteShader(vertShader);
            vertShader = 0;
        }
        if (fragShader)
        {
            glDeleteShader(fragShader);
            fragShader = 0;
        }
        if (shaderProgram)
        {
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
        }

        return 0;
    }

    // Release vertex and fragment shaders.
    if (vertShader)
    {
        glDetachShader(shaderProgram, vertShader);
        glDeleteShader(vertShader);
    }

    if (fragShader)
    {
        glDetachShader(shaderProgram, fragShader);
        glDeleteShader(fragShader);
    }

    return shaderProgram;
}


/*********************************************************************************************************************
 *                                      CubismRenderer_OpenGLES2
 ********************************************************************************************************************/
#ifdef CSM_TARGET_ANDROID_ES2
void CubismRenderer_OpenGLES2::SetExtShaderMode(csmBool extMode, csmBool extPAMode)
{
    CubismShader_OpenGLES2::SetExtShaderMode(extMode, extPAMode);
    CubismShader_OpenGLES2::DeleteInstance();
}

void CubismRenderer_OpenGLES2::ReloadShader()
{
    CubismShader_OpenGLES2::DeleteInstance();
}
#endif


#ifdef CSM_TARGET_WIN_GL

namespace {
PFNGLACTIVETEXTUREPROC glActiveTexture;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
//PFNGLBINDVERTEXARRAY            glBindVertexArrayOES ;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM4FPROC glUniform4f;

PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLVALIDATEPROGRAMPROC glValidateProgram;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;

PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLDELETEPROGRAMPROC glDeleteProgram;

PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLDETACHSHADERPROC glDetachShader;
PFNGLDELETESHADERPROC glDeleteShader;

PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffers;
PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffers;
PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebuffer;
PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbuffer;
PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffers;
PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffers;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2D;

PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorage;
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbuffer;

PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatus;

PFNGLGETVERTEXATTRIBIVPROC glGetVertexAttribiv;

csmBool s_isInitializeGlFunctionsSuccess = false;     ///< 初期化が完了したかどうか。trueなら初期化完了
csmBool s_isFirstInitializeGlFunctions = true;        ///< 最初の初期化実行かどうか。trueなら最初の初期化実行
}

void CubismRenderer_OpenGLES2::InitializeGlFunctions()
{
    if (!s_isFirstInitializeGlFunctions) return;

    s_isInitializeGlFunctionsSuccess = true; //失敗した場合にフラグをfalseにする

    glActiveTexture = (PFNGLACTIVETEXTUREPROC)WinGlGetProcAddress("glActiveTexture");
    //最初の初期化処理が成功したら、以降は初期化処理を行わない
    if (glActiveTexture) s_isFirstInitializeGlFunctions = false;
    else return;

    glBindBuffer = (PFNGLBINDBUFFERPROC)WinGlGetProcAddress("glBindBuffer");
    glUseProgram = (PFNGLUSEPROGRAMPROC)WinGlGetProcAddress("glUseProgram");

    glUniform1i = (PFNGLUNIFORM1IPROC)WinGlGetProcAddress("glUniform1i");
    glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)WinGlGetProcAddress("glGetAttribLocation");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)WinGlGetProcAddress("glGetUniformLocation");
    glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)WinGlGetProcAddress("glBlendFuncSeparate");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)WinGlGetProcAddress("glEnableVertexAttribArray");
    glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)WinGlGetProcAddress("glDisableVertexAttribArray");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)WinGlGetProcAddress("glVertexAttribPointer");

    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)WinGlGetProcAddress("glUniformMatrix4fv");
    glUniform1f = (PFNGLUNIFORM1FPROC)WinGlGetProcAddress("glUniform1f");
    glUniform4f = (PFNGLUNIFORM4FPROC)WinGlGetProcAddress("glUniform4f");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)WinGlGetProcAddress("glLinkProgram");

    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)WinGlGetProcAddress("glGetProgramiv");
    glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)WinGlGetProcAddress("glValidateProgram");

    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)WinGlGetProcAddress("glGetProgramInfoLog");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)WinGlGetProcAddress("glCreateProgram");

    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)WinGlGetProcAddress("glDeleteProgram");
    glShaderSource = (PFNGLSHADERSOURCEPROC)WinGlGetProcAddress("glShaderSource");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)WinGlGetProcAddress("glGetShaderiv");
    glCompileShader = (PFNGLCOMPILESHADERPROC)WinGlGetProcAddress("glCompileShader");
    glCreateShader = (PFNGLCREATESHADERPROC)WinGlGetProcAddress("glCreateShader");
    glAttachShader = (PFNGLATTACHSHADERPROC)WinGlGetProcAddress("glAttachShader");
    glDetachShader = (PFNGLDETACHSHADERPROC)WinGlGetProcAddress("glDetachShader");
    glDeleteShader = (PFNGLDELETESHADERPROC)WinGlGetProcAddress("glDeleteShader");

    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSEXTPROC)WinGlGetProcAddress("glGenFramebuffers");
    glGenRenderbuffers = (PFNGLGENRENDERBUFFERSEXTPROC)WinGlGetProcAddress("glGenRenderbuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFEREXTPROC)WinGlGetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)WinGlGetProcAddress("glFramebufferTexture2D");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSEXTPROC)WinGlGetProcAddress("glDeleteFramebuffers");
    glBindRenderbuffer = (PFNGLBINDRENDERBUFFEREXTPROC)WinGlGetProcAddress("glBindRenderbuffer");
    glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSEXTPROC)WinGlGetProcAddress("glDeleteRenderbuffers");

    glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEEXTPROC)WinGlGetProcAddress("glRenderbufferStorage");
    glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)WinGlGetProcAddress("glFramebufferRenderbuffer");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)WinGlGetProcAddress("glCheckFramebufferStatus");
    glGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)WinGlGetProcAddress("glGetVertexAttribiv");
}

void* CubismRenderer_OpenGLES2::WinGlGetProcAddress(const csmChar* name)
{
    void* ptr = wglGetProcAddress(name);
    if (ptr == NULL)
    {
        CubismLogError("WinGlGetProcAddress() failed :: %s" , name ) ;
        s_isInitializeGlFunctionsSuccess = false;
    }
    return ptr;
}

void CubismRenderer_OpenGLES2::CheckGlError(const csmChar* message)
{
    GLenum errcode = glGetError();
    if (errcode != GL_NO_ERROR)
    {
        CubismLogError("0x%04X(%4d) : %s", errcode, errcode, message);
    }
}

#endif  //CSM_TARGET_WIN_GL

CubismRenderer* CubismRenderer::Create()
{
    return CSM_NEW CubismRenderer_OpenGLES2();
}

void CubismRenderer::StaticRelease()
{
    CubismRenderer_OpenGLES2::DoStaticRelease();
}

CubismRenderer_OpenGLES2::CubismRenderer_OpenGLES2() : _clippingContextBufferForMask(NULL)
                                                     , _clippingContextBufferForDraw(NULL)
                                                     , _clippingManager(NULL)
{
    // テクスチャ対応マップの容量を確保しておく.
    _textures.PrepareCapacity(32, true);
}

CubismRenderer_OpenGLES2::~CubismRenderer_OpenGLES2()
{
    CSM_DELETE_SELF(CubismClippingManager_OpenGLES2, _clippingManager);
}

void CubismRenderer_OpenGLES2::DoStaticRelease()
{
#ifdef CSM_TARGET_WINGL
    s_isInitializeGlFunctionsSuccess = false;     ///< 初期化が完了したかどうか。trueなら初期化完了
    s_isFirstInitializeGlFunctions = true;        ///< 最初の初期化実行かどうか。trueなら最初の初期化実行
#endif
    CubismShader_OpenGLES2::DeleteInstance();
}

void CubismRenderer_OpenGLES2::Initialize(CubismModel* model)
{
    if (model->IsUsingMasking())
    {
        _clippingManager = CSM_NEW CubismClippingManager_OpenGLES2();  //クリッピングマスク・バッファ前処理方式を初期化
        _clippingManager->Initialize(
            *model,
            model->GetDrawableCount(),
            model->GetDrawableMasks(),
            model->GetDrawableMaskCounts()
        );
    }

    _sortedDrawableIndexList.Resize(model->GetDrawableCount(), 0);

    CubismRenderer::Initialize(model);  //親クラスの処理を呼ぶ
}

void CubismRenderer_OpenGLES2::PreDraw()
{
#ifdef CSM_TARGET_WIN_GL
    if (s_isFirstInitializeGlFunctions) InitializeGlFunctions();
    if (!s_isInitializeGlFunctionsSuccess) return;
#endif

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glColorMask(1, 1, 1, 1);

#ifdef CSM_TARGET_IPHONE_ES2
    glBindVertexArrayOES(0);
#endif

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); //前にバッファがバインドされていたら破棄する必要がある

    //異方性フィルタリング。プラットフォームのOpenGLによっては未対応の場合があるので、未設定のときは設定しない
    if (GetAnisotropy() > 0.0f)
    {
        for (csmUint32 i = 0; i < _textures.GetSize(); i++)
        {
            glBindTexture(GL_TEXTURE_2D, _textures[i]);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GetAnisotropy());
        }
    }
}


void CubismRenderer_OpenGLES2::DoDrawModel()
{
    //------------ クリッピングマスク・バッファ前処理方式の場合 ------------
    if (_clippingManager != NULL)
    {
        PreDraw();
        _clippingManager->SetupClippingContext(*GetModel(), this);
    }

    // 上記クリッピング処理内でも一度PreDrawを呼ぶので注意!!
    PreDraw();

    const csmInt32 drawableCount = GetModel()->GetDrawableCount();
    const csmInt32* renderOrder = GetModel()->GetDrawableRenderOrders();

    // インデックスを描画順でソート
    for (csmInt32 i = 0; i < drawableCount; ++i)
    {
        const csmInt32 order = renderOrder[i];
        _sortedDrawableIndexList[order] = i;
    }

    // 描画
    for (csmInt32 i = 0; i < drawableCount; ++i)
    {
        const csmInt32 drawableIndex = _sortedDrawableIndexList[i];

        // Drawableが表示状態でなければ処理をパスする
        if (!GetModel()->GetDrawableDynamicFlagIsVisible(drawableIndex))
        {
            continue;
        }

        // クリッピングマスクをセットする
        SetClippingContextBufferForDraw((_clippingManager != NULL)
            ? (*_clippingManager->GetClippingContextListForDraw())[drawableIndex]
            : NULL);

        IsCulling(GetModel()->GetDrawableCulling(drawableIndex) != 0);

        DrawMesh(
            GetModel()->GetDrawableTextureIndices(drawableIndex),
            GetModel()->GetDrawableVertexIndexCount(drawableIndex),
            GetModel()->GetDrawableVertexCount(drawableIndex),
            const_cast<csmUint16*>(GetModel()->GetDrawableVertexIndices(drawableIndex)),
            const_cast<csmFloat32*>(GetModel()->GetDrawableVertices(drawableIndex)),
            reinterpret_cast<csmFloat32*>(const_cast<Core::csmVector2*>(GetModel()->GetDrawableVertexUvs(drawableIndex))),
            GetModel()->GetDrawableOpacity(drawableIndex),
            GetModel()->GetDrawableBlendMode(drawableIndex)
        );
    }

    //
    PostDraw();

}

void CubismRenderer_OpenGLES2::DrawMesh(csmInt32 textureNo, csmInt32 indexCount, csmInt32 vertexCount
                                        , csmUint16* indexArray, csmFloat32* vertexArray, csmFloat32* uvArray
                                        , csmFloat32 opacity, CubismBlendMode colorBlendMode)
{

#ifdef CSM_TARGET_WIN_GL
    if (s_isFirstInitializeGlFunctions) return;  // WindowsプラットフォームではGL命令のバインドを済ませておく必要がある
#endif

#ifndef CSM_DEBUG
    if (_textures[textureNo] == NULL) return;    // モデルが参照するテクスチャがバインドされていない場合は描画をスキップする
#endif

    // 裏面描画の有効・無効
    if (IsCulling())
    {
        glEnable(GL_CULL_FACE);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }

    glFrontFace(GL_CCW);    // Cubism3 OpenGLはマスク・アートメッシュ共にCCWが表面

    CubismTextureColor modelColorRGBA = GetModelColor();

    if (GetClippingContextBufferForMask() == NULL) // マスク生成時以外
    {
        modelColorRGBA.A *= opacity;
        if (IsPremultipliedAlpha())
        {
            modelColorRGBA.R *= modelColorRGBA.A;
            modelColorRGBA.G *= modelColorRGBA.A;
            modelColorRGBA.B *= modelColorRGBA.A;
        }
    }

    GLuint drawTextureId;   // シェーダに渡すテクスチャID

    // テクスチャマップからバインド済みテクスチャIDを取得
    // バインドされていなければダミーのテクスチャIDをセットする
    if(_textures[textureNo] != NULL)
    {
        drawTextureId = _textures[textureNo];
    }
    else
    {
        drawTextureId = -1;
    }

    CubismShader_OpenGLES2::GetInstance()->SetupShaderProgram(
        this, drawTextureId, vertexCount, vertexArray, uvArray
        , opacity, colorBlendMode, modelColorRGBA, IsPremultipliedAlpha()
        , GetMvpMatrix()
    );

    // ポリゴンメッシュを描画する
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indexArray);

    // 後処理
    glUseProgram(0);
    SetClippingContextBufferForDraw(NULL);
    SetClippingContextBufferForMask(NULL);
}

void CubismRenderer_OpenGLES2::SaveProfile()
{
    _rendererProfile.Save();
}

void CubismRenderer_OpenGLES2::RestoreProfile()
{
    _rendererProfile.Restore();
}

void CubismRenderer_OpenGLES2::BindTexture(csmUint32 modelTextureNo, GLuint glTextureNo)
{
    _textures[modelTextureNo] = glTextureNo;
}

const csmMap<csmInt32, GLuint>& CubismRenderer_OpenGLES2::GetBindedTextures() const
{
    return _textures;
}

void CubismRenderer_OpenGLES2::SetClippingMaskBufferSize(csmInt32 size)
{
    //FrameBufferのサイズを変更するためにインスタンスを破棄・再作成する
    CSM_DELETE_SELF(CubismClippingManager_OpenGLES2, _clippingManager);

    _clippingManager = CSM_NEW CubismClippingManager_OpenGLES2();

    _clippingManager->SetClippingMaskBufferSize(size);

    _clippingManager->Initialize(
        *GetModel(),
        GetModel()->GetDrawableCount(),
        GetModel()->GetDrawableMasks(),
        GetModel()->GetDrawableMaskCounts()
    );
}

csmInt32 CubismRenderer_OpenGLES2::GetClippingMaskBufferSize() const
{
    return _clippingManager->GetClippingMaskBufferSize();
}

void CubismRenderer_OpenGLES2::SetClippingContextBufferForMask(CubismClippingContext* clip)
{
    _clippingContextBufferForMask = clip;
}

CubismClippingContext* CubismRenderer_OpenGLES2::GetClippingContextBufferForMask() const
{
    return _clippingContextBufferForMask;
}

void CubismRenderer_OpenGLES2::SetClippingContextBufferForDraw(CubismClippingContext* clip)
{
    _clippingContextBufferForDraw = clip;
}

CubismClippingContext* CubismRenderer_OpenGLES2::GetClippingContextBufferForDraw() const
{
    return _clippingContextBufferForDraw;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
