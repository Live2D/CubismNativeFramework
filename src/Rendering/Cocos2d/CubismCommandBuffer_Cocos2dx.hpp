﻿/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismFramework.hpp"
#include "cocos2d.h"

#ifdef CSM_TARGET_ANDROID_ES2
#include <jni.h>
#include <errno.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#ifdef CSM_TARGET_IPHONE_ES2
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#endif

#if defined(CSM_TARGET_WIN_GL) || defined(CSM_TARGET_LINUX_GL)
//#include <GL/glew.h>
//#include <GL/gl.h>
#endif

#ifdef CSM_TARGET_MAC_GL
#ifndef CSM_TARGET_COCOS
#include <GL/glew.h>
#endif
#include <OpenGL/gl.h>
#endif

namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

static inline cocos2d::Renderer* GetCocos2dRenderer()
{
    return cocos2d::Director::getInstance()->getRenderer();
}

class CubismCommandBuffer_Cocos2dx
{
public:
    class DrawCommandBuffer
    {
    public:
        class DrawCommand
        {
        public:
            DrawCommand();
            virtual ~DrawCommand();

            cocos2d::backend::BlendDescriptor* GetBlendDescriptor();
            cocos2d::PipelineDescriptor* GetPipelineDescriptor();
            cocos2d::CustomCommand* GetCommand();

        private:
            DrawCommand& operator=(const DrawCommand&);
            cocos2d::CustomCommand _command;
        };

        DrawCommandBuffer();
        virtual ~DrawCommandBuffer();


        void CreateVertexBuffer(csmSizeInt stride, csmSizeInt count);
        void CreateIndexBuffer(csmSizeInt count);
        void UpdateVertexBuffer(void* data, void* uvData, csmSizeInt count);
        void UpdateIndexBuffer(void* data, csmSizeInt count);
        void CommitVertexBuffer();

        DrawCommand* GetCommandDraw();

    private:
        DrawCommand _drawCommandDraw;
        csmSizeInt _vbStride;
        csmSizeInt _vbCount;
        csmSizeInt _ibCount;
        csmUint8* _drawBuffer;
    };

    union OperationStateDataArgument
    {
        csmBool    bl;
        csmInt32   i32;
        csmUint32  u32;
        csmFloat32 f32;
    };

    struct OperationStateData
    {
        OperationStateData()
        : Enabled(false)
        {
            Arg0.i32 = 0;
            Arg1.i32 = 0;
            Arg2.i32 = 0;
            Arg3.i32 = 0;
        }

        csmBool Enabled;
        OperationStateDataArgument Arg0;
        OperationStateDataArgument Arg1;
        OperationStateDataArgument Arg2;
        OperationStateDataArgument Arg3;
    };

    enum OperationType
    {
        OperationType_ScissorTest = 0,
        OperationType_DepthTest,
        OperationType_StencilTest,
        OperationType_Culling,
        OperationType_Winding,
        OperationType_TypeMax,
    };

    enum CullType
    {
        CullType_Back,
        CullType_Front,
    };

    enum WindingType
    {
        WindingType_ClockWise,
        WindingType_CounterClockWise,
    };

    CubismCommandBuffer_Cocos2dx();
    virtual ~CubismCommandBuffer_Cocos2dx();

    void PushCommandGroup();
    void PopCommandGroup();

    void SetOperationEnable(OperationType operationType, csmBool enabled);
    csmBool GetOperationEnabled(OperationType operationType);

    void SetCullMode(CullType cullType);
    void SetWindingMode(WindingType windingType);

    void Clear(csmFloat32 r, csmFloat32 g, csmFloat32 b, csmFloat32 a);

    void Viewport(csmFloat32 x, csmFloat32 y, csmFloat32 w, csmFloat32 h);

    void SetColorBuffer(cocos2d::Texture2D* colorBuffer);
    cocos2d::Texture2D* GetColorBuffer();

    void AddDrawCommand(DrawCommandBuffer::DrawCommand* drawCommand);

private:
    void AddCommand(const std::function<void()>& fn);

    cocos2d::Texture2D* _currentColorBuffer;
    OperationStateData _operationStateArray[OperationType_TypeMax];
    cocos2d::GroupCommand _groupCommand;
};

}}}}
