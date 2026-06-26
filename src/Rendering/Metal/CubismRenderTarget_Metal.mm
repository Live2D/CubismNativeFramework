/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismRenderTarget_Metal.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

void CubismRenderTarget_Metal::CopyBuffer(CubismRenderTarget_Metal &src, CubismRenderTarget_Metal &dst, id<MTLCommandBuffer> commandBuffer)
{
    id<MTLBlitCommandEncoder> blitCommandEncoder = [commandBuffer blitCommandEncoder];
    [blitCommandEncoder copyFromTexture:src.GetColorBuffer() toTexture:dst.GetColorBuffer()];
    [blitCommandEncoder endEncoding];
}

CubismRenderTarget_Metal::CubismRenderTarget_Metal()
    : _colorBuffer(nil)
    , _depthBuffer(nil)
    , _commandEncoder(nil)
    , _renderPassDescriptor(nil)
    , _bufferWidth(0)
    , _bufferHeight(0)
    , _pixelFormat(MTLPixelFormatRGBA8Unorm)
    , _clearColorR(0.0f)
    , _clearColorG(0.0f)
    , _clearColorB(0.0f)
    , _clearColorA(0.0f)
{
}

void CubismRenderTarget_Metal::BeginDraw(id<MTLCommandBuffer> commandBuffer)
{
    if (_commandEncoder == nil)
    {
        _commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:_renderPassDescriptor];
    }
}

void CubismRenderTarget_Metal::EndDraw()
{
    if (_commandEncoder != nil)
    {
      [_commandEncoder endEncoding];
      _commandEncoder = nil;
      // 次回BeginDrawを呼び出すときに以前書き込んだ内容を呼び出すようにする
      _renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionLoad;
    }
}

void CubismRenderTarget_Metal::Clear(float r, float g, float b, float a)
{
    if (_commandEncoder != nil)
    {
        // BeginDraw後に設定が変更されることを防ぐ
        return;
    }

    _clearColorR = r;
    _clearColorG = g;
    _clearColorB = b;
    _clearColorA = a;
    _renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(_clearColorR, _clearColorG, _clearColorB, _clearColorA);
    _renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
}

csmBool CubismRenderTarget_Metal::CreateRenderTarget(id<MTLDevice> device, csmUint32 displayBufferWidth, csmUint32 displayBufferHeight, id <MTLTexture> colorBuffer, id <MTLTexture> depthBuffer)
{
    // 一旦削除
    DestroyRenderTarget();

    do
    {
        _renderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];
        _renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
        _renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
        _renderPassDescriptor.depthAttachment.clearDepth = 1.0;
        _renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        _renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        _renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(_clearColorR, _clearColorG, _clearColorB, _clearColorA);
        _renderPassDescriptor.renderTargetWidth = displayBufferWidth;
        _renderPassDescriptor.renderTargetHeight = displayBufferHeight;

        if (colorBuffer == NULL)
        {
            csmBool initResult = false;

            // Set up a texture for rendering to and sampling from
            MTLTextureDescriptor *texDescriptor = [[[MTLTextureDescriptor alloc] init] autorelease];
            texDescriptor.textureType = MTLTextureType2D;
            texDescriptor.width = displayBufferWidth;
            texDescriptor.height = displayBufferHeight;
            texDescriptor.pixelFormat = _pixelFormat;
            texDescriptor.storageMode = MTLStorageModePrivate;
            texDescriptor.usage = MTLTextureUsageRenderTarget |
                                  MTLTextureUsageShaderRead;
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

        if(depthBuffer == nil)
        {
            MTLTextureDescriptor *depthTexDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:displayBufferWidth height:displayBufferHeight mipmapped:false];
            depthTexDescriptor.usage = MTLTextureUsageRenderTarget;
            depthTexDescriptor.storageMode = MTLStorageModePrivate;
            _depthBuffer = [device newTextureWithDescriptor:depthTexDescriptor];
        }
        else
        {
            _depthBuffer = depthBuffer;
        }

        if(_depthBuffer)
        {
            _renderPassDescriptor.depthAttachment.texture = _depthBuffer;
        }

        _bufferWidth = displayBufferWidth;
        _bufferHeight = displayBufferHeight;

        // 成功
        return true;

    } while (0);

    // 失敗したので削除
    DestroyRenderTarget();

    return false;
}

void CubismRenderTarget_Metal::DestroyRenderTarget()
{
    if (_colorBuffer != nil)
    {
        [_colorBuffer release];
        _colorBuffer = nil;
    }

    if(_depthBuffer != nil)
    {
        [_depthBuffer release];
        _depthBuffer = nil;
    }

    if (_renderPassDescriptor != nil)
    {
        [_renderPassDescriptor release];
        _renderPassDescriptor = nil;
    }
}

id <MTLTexture> CubismRenderTarget_Metal::GetColorBuffer() const
{
    return _colorBuffer;
}

id <MTLTexture> CubismRenderTarget_Metal::GetDepthBuffer() const
{
    return _depthBuffer;
}

id <MTLRenderCommandEncoder> CubismRenderTarget_Metal::GetCommandEncoder() const
{
    return _commandEncoder;
}

csmUint32 CubismRenderTarget_Metal::GetBufferWidth() const
{
    return _bufferWidth;
}

csmUint32 CubismRenderTarget_Metal::GetBufferHeight() const
{
    return _bufferHeight;
}

csmBool CubismRenderTarget_Metal::IsValid() const
{
    return _renderPassDescriptor != NULL;
}

const MTLViewport* CubismRenderTarget_Metal::GetViewport() const
{
    return &_viewPort;
}

MTLRenderPassDescriptor* CubismRenderTarget_Metal::GetRenderPassDescriptor() const
{
    return _renderPassDescriptor;
}

void CubismRenderTarget_Metal::SetMTLPixelFormat(MTLPixelFormat pixelFormat)
{
    _pixelFormat = pixelFormat;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
