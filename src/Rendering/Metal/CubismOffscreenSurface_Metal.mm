/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismOffscreenSurface_Metal.hpp"
#include "CubismRenderingInstanceSingleton_Metal.h"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

CubismOffscreenSurface_Metal::CubismOffscreenSurface_Metal()
    : _colorBuffer(NULL)
    , _renderPassDescriptor(NULL)
    , _bufferWidth(0)
    , _bufferHeight(0)
    , _pixelFormat(MTLPixelFormatRGBA8Unorm)
    , _clearColorR(1.0f)
    , _clearColorG(1.0f)
    , _clearColorB(1.0f)
    , _clearColorA(1.0f)
{
}

csmBool CubismOffscreenSurface_Metal::CreateOffscreenSurface(csmUint32 displayBufferWidth, csmUint32 displayBufferHeight, id <MTLTexture> colorBuffer)
{
    // 一旦削除
    DestroyOffscreenSurface();

    do
    {
        if (colorBuffer == NULL)
        {
            csmBool initResult = false;

            _renderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];
            _renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
            _renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
            _renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(_clearColorR, _clearColorG, _clearColorB, _clearColorA);

            _renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
            _renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
            _renderPassDescriptor.depthAttachment.clearDepth = 1.0;

            _renderPassDescriptor.renderTargetWidth = displayBufferWidth;
            _renderPassDescriptor.renderTargetHeight = displayBufferHeight;

            // Set up a texture for rendering to and sampling from
            MTLTextureDescriptor *texDescriptor = [[[MTLTextureDescriptor alloc] init] autorelease];
            texDescriptor.textureType = MTLTextureType2D;
            texDescriptor.width = displayBufferWidth;
            texDescriptor.height = displayBufferHeight;
            texDescriptor.pixelFormat = _pixelFormat;
            texDescriptor.usage = MTLTextureUsageRenderTarget |
                                  MTLTextureUsageShaderRead;
            CubismRenderingInstanceSingleton_Metal *single = [CubismRenderingInstanceSingleton_Metal sharedManager];
            id <MTLDevice> device = [single getMTLDevice];
            _colorBuffer = [device newTextureWithDescriptor:texDescriptor];

        }
        else
        {
            _colorBuffer = colorBuffer;
        }

        if (_colorBuffer)
        {
            _renderPassDescriptor.colorAttachments[0].texture = _colorBuffer;
            _viewPort = { 0.0f, 0.0f, static_cast<double>(_colorBuffer.width), static_cast<double>(_colorBuffer.height), 0.0, 1.0};
        }
        else
        {
            _viewPort = {0.0, 0.0, static_cast<double>(_bufferWidth), static_cast<double>(_bufferHeight), 0.0, 1.0};
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

void CubismOffscreenSurface_Metal::DestroyOffscreenSurface()
{
    if (_colorBuffer != NULL)
    {
        [_colorBuffer release];
        _colorBuffer = NULL;
    }

    if (_renderPassDescriptor != NULL)
    {
        [_renderPassDescriptor release];
        _renderPassDescriptor = NULL;
    }
}

id <MTLTexture> CubismOffscreenSurface_Metal::GetColorBuffer() const
{
    return _colorBuffer;
}

void CubismOffscreenSurface_Metal::SetClearColor(float r, float g, float b, float a)
{
    _clearColorR = r;
    _clearColorG = g;
    _clearColorB = b;
    _clearColorA = a;
}

csmUint32 CubismOffscreenSurface_Metal::GetBufferWidth() const
{
    return _bufferWidth;
}

csmUint32 CubismOffscreenSurface_Metal::GetBufferHeight() const
{
    return _bufferHeight;
}

csmBool CubismOffscreenSurface_Metal::IsValid() const
{
    return _renderPassDescriptor != NULL;
}

const MTLViewport* CubismOffscreenSurface_Metal::GetViewport() const
{
    return &_viewPort;
}

MTLRenderPassDescriptor* CubismOffscreenSurface_Metal::GetRenderPassDescriptor() const
{
    return _renderPassDescriptor;
}

void CubismOffscreenSurface_Metal::SetMTLPixelFormat(MTLPixelFormat pixelFormat)
{
    _pixelFormat = pixelFormat;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
