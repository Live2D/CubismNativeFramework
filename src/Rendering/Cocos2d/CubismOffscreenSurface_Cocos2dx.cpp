﻿/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismOffscreenSurface_Cocos2dx.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

CubismOffscreenSurface_Cocos2dx::CubismOffscreenSurface_Cocos2dx()
    : _renderTexture(NULL)
    , _colorBuffer(NULL)
    , _isInheritedRenderTexture(false)
    , _previousColorBuffer(NULL)
    , _bufferWidth(0)
    , _bufferHeight(0)
{
}


void CubismOffscreenSurface_Cocos2dx::BeginDraw(CubismCommandBuffer_Cocos2dx* commandBuffer, cocos2d::Texture2D* colorBufferOnFinishDrawing)
{
    if (!IsValid())
    {
        return;
    }

    // バックバッファのサーフェイスを記憶しておく
    if (colorBufferOnFinishDrawing == NULL)
    {
        _previousColorBuffer = commandBuffer->GetColorBuffer();
        if (_previousColorBuffer == NULL)
        {
            _previousColorBuffer = GetCocos2dRenderer()->getColorAttachment();
        }
    }
    else
    {
        _previousColorBuffer = colorBufferOnFinishDrawing;
    }

    // マスク用RenderTextureをactiveにセット
    commandBuffer->SetColorBuffer(_renderTexture->getSprite()->getTexture());
}

void CubismOffscreenSurface_Cocos2dx::EndDraw(CubismCommandBuffer_Cocos2dx* commandBuffer)
{
    if (!IsValid())
    {
        return;
    }

    // 描画対象を戻す
    commandBuffer->SetColorBuffer(_previousColorBuffer);
}

void CubismOffscreenSurface_Cocos2dx::Clear(CubismCommandBuffer_Cocos2dx* commandBuffer, float r, float g, float b, float a)
{
    // マスクをクリアする
    commandBuffer->Clear(r, g, b, a);
}

csmBool CubismOffscreenSurface_Cocos2dx::CreateOffscreenSurface(csmUint32 displayBufferWidth, csmUint32 displayBufferHeight, cocos2d::RenderTexture* renderTexture)
{
    // 一旦削除
    DestroyOffscreenSurface();

    do
    {
        if (!renderTexture)
        {
            // The inherited RenderTexture is nothing.
            // Make a RenderTexture.


            csmBool initResult = false;


            _renderTexture = cocos2d::RenderTexture::create(displayBufferWidth, displayBufferHeight);

            if (!_renderTexture)
            {
                break;
            }

            _renderTexture->retain();


            _renderTexture->getSprite()->getTexture()->setTexParameters(
                cocos2d::Texture2D::TexParams(
                    cocos2d::backend::SamplerFilter::LINEAR,                    // MagFilter
                    cocos2d::backend::SamplerFilter::LINEAR,                    // MinFilter
                    cocos2d::backend::SamplerAddressMode::CLAMP_TO_EDGE,      // AddressingMode S
                    cocos2d::backend::SamplerAddressMode::CLAMP_TO_EDGE       // AddressingMode T
                )
            );

            _colorBuffer = _renderTexture->getSprite()->getTexture();
            _isInheritedRenderTexture = false;
        }
        else
        {
            // Use the inherited RenderTexture.
            _renderTexture = renderTexture;
            _colorBuffer = _renderTexture->getSprite()->getTexture();


            _isInheritedRenderTexture = true;
        }

        if (_colorBuffer)
        {
            _viewPortSize = csmRectF(0.0f, 0.0f, _colorBuffer->getContentSizeInPixels().width, _colorBuffer->getContentSizeInPixels().height);
        }
        else
        {
            _viewPortSize = csmRectF(0.0f, 0.0f, _bufferWidth, _bufferHeight);
        }

        _bufferWidth = displayBufferWidth;
        _bufferHeight = displayBufferHeight;


        // 成功
        return true;

    } while (0);

    // 失敗したので削除
    DestroyOffscreenSurface();

    return false;
}

void CubismOffscreenSurface_Cocos2dx::DestroyOffscreenSurface()
{
    if ((_renderTexture != NULL) && !_isInheritedRenderTexture)
    {
        CC_SAFE_RELEASE_NULL(_renderTexture);
        _colorBuffer = NULL;
    }
}

cocos2d::Texture2D* CubismOffscreenSurface_Cocos2dx::GetColorBuffer() const
{
    return _renderTexture->getSprite()->getTexture();
}

csmUint32 CubismOffscreenSurface_Cocos2dx::GetBufferWidth() const
{
    return _bufferWidth;
}

csmUint32 CubismOffscreenSurface_Cocos2dx::GetBufferHeight() const
{
    return _bufferHeight;
}

csmRectF CubismOffscreenSurface_Cocos2dx::GetViewPortSize() const
{
    return _viewPortSize;
}

csmBool CubismOffscreenSurface_Cocos2dx::IsValid() const
{
    return _renderTexture != NULL;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
