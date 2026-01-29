/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismRenderer_OpenGLES2.hpp"
#include "CubismOffscreenManager_OpenGLES2.hpp"
#include "Math/CubismMatrix44.hpp"
#include "Type/csmVector.hpp"
#include "Type/csmVectorSort.hpp"
#include "Model/CubismModel.hpp"

#ifdef CSM_TARGET_WIN_GL
#include <Windows.h>
#endif

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

namespace {
    /**
     * コピーする際に使用する
     */
    const csmUint16 ModelRenderTargetIndexArray[] = {
        0, 1, 2,
        2, 1, 3,
    };
}

/*********************************************************************************************************************
*                                      CubismClippingManager_OpenGLES2
********************************************************************************************************************/
void CubismClippingManager_OpenGLES2::SetupClippingContext(CubismModel& model, CubismRenderer_OpenGLES2* renderer, GLint lastFBO, GLint lastViewport[4], CubismRenderer::DrawableObjectType drawableObjectType)
{
    // 全てのクリッピングを用意する
    // 同じクリップ（複数の場合はまとめて１つのクリップ）を使う場合は１度だけ設定する
    csmInt32 usingClipCount = 0;
    for (csmUint32 clipIndex = 0; clipIndex < _clippingContextListForMask.GetSize(); clipIndex++)
    {
        // １つのクリッピングマスクに関して
        CubismClippingContext_OpenGLES2* cc = _clippingContextListForMask[clipIndex];

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
    // 生成したRenderTargetと同じサイズでビューポートを設定
    glViewport(0, 0, _clippingMaskBufferSize.X, _clippingMaskBufferSize.Y);

    // 後の計算のためにインデックスの最初をセット
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
    // ----- マスク描画処理 -----
    _currentMaskBuffer->BeginDraw(lastFBO);

    renderer->PreDraw(); // バッファをクリアする

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
        CubismClippingContext_OpenGLES2* clipContext = _clippingContextListForMask[clipIndex];
        csmRectF* allClippedDrawRect = clipContext->_allClippedDrawRect; //このマスクを使う、全ての描画オブジェクトの論理座標上の囲み矩形
        csmRectF* layoutBoundsOnTex01 = clipContext->_layoutBounds; //この中にマスクを収める
        const csmFloat32 MARGIN = 0.05f;

        // clipContextに設定したレンダーターゲットをインデックスで取得
        CubismRenderTarget_OpenGLES2* maskBuffer = NULL;
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

        // 現在のレンダーターゲットがclipContextのものと異なる場合
        if (_currentMaskBuffer != maskBuffer)
        {
            _currentMaskBuffer->EndDraw();
            _currentMaskBuffer = maskBuffer;
            // マスク用RenderTextureをactiveにセット
            _currentMaskBuffer->BeginDraw(lastFBO);

            // バッファをクリアする。
            renderer->PreDraw();
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

        if (drawableObjectType == CubismRenderer::DrawableObjectType_Offscreen)
        {
            // clipContext * mvp^-1
            CubismMatrix44 invertMvp = renderer->GetMvpMatrix().GetInvert();
            clipContext->_matrixForDraw.MultiplyByMatrix(&invertMvp);
        }

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

            // マスクがクリアされていないなら処理する
            if (!_clearedMaskBufferFlags[clipContext->_bufferIndex])
            {
                // マスクをクリアする
                // 1が無効（描かれない）領域、0が有効（描かれる）領域。（シェーダーCd*Csで0に近い値をかけてマスクを作る。1をかけると何も起こらない）
                glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                _clearedMaskBufferFlags[clipContext->_bufferIndex] = true;
            }

            // 今回専用の変換を適用して描く
            // チャンネルも切り替える必要がある(A,R,G,B)
            renderer->SetClippingContextBufferForMask(clipContext);

            renderer->DrawMeshOpenGL(model, clipDrawIndex);
        }
    }

    // --- 後処理 ---
    _currentMaskBuffer->EndDraw();
    renderer->SetClippingContextBufferForMask(NULL);
    glViewport(lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3]);
}

/*********************************************************************************************************************
*                                      CubismClippingContext_OpenGLES2
********************************************************************************************************************/
CubismClippingContext_OpenGLES2::CubismClippingContext_OpenGLES2(CubismClippingManager<CubismClippingContext_OpenGLES2, CubismRenderTarget_OpenGLES2>* manager, CubismModel& model, const csmInt32* clippingDrawableIndices, csmInt32 clipCount)
    : CubismClippingContext(clippingDrawableIndices, clipCount)
{
    _owner = manager;
}

CubismClippingContext_OpenGLES2::~CubismClippingContext_OpenGLES2()
{
}

CubismClippingManager<CubismClippingContext_OpenGLES2, CubismRenderTarget_OpenGLES2>* CubismClippingContext_OpenGLES2::GetClippingManager()
{
    return _owner;
}

/*********************************************************************************************************************
*                                      CubismDrawProfile_OpenGL
********************************************************************************************************************/
void CubismRendererProfile_OpenGLES2::SetGlEnable(GLenum index, GLboolean enabled)
{
    if (enabled == GL_TRUE)
    {
        glEnable(index);
    }
    else
    {
        glDisable(index);
    }
}

void CubismRendererProfile_OpenGLES2::SetGlEnableVertexAttribArray(GLuint index, GLint enabled)
{
    if (enabled)
    {
        glEnableVertexAttribArray(index);
    }
    else
    {
        glDisableVertexAttribArray(index);
    }
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
PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbuffer;
PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffers;
PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffers;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2D;

PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorage;
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbuffer;

PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatus;

PFNGLGETVERTEXATTRIBIVPROC glGetVertexAttribiv;

PFNGLTEXTUREBARRIERPROC glTextureBarrier;

csmBool s_isInitializeGlFunctionsSuccess = false;     ///< 初期化が完了したかどうか。trueなら初期化完了
csmBool s_isFirstInitializeGlFunctions = true;        ///< 最初の初期化実行かどうか。trueなら最初の初期化実行
}

void CubismRenderer_OpenGLES2::InitializeGlFunctions()
{
    if (!s_isFirstInitializeGlFunctions)
    {
        return;
    }

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
    glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)WinGlGetProcAddress("glBlitFramebuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)WinGlGetProcAddress("glFramebufferTexture2D");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSEXTPROC)WinGlGetProcAddress("glDeleteFramebuffers");
    glBindRenderbuffer = (PFNGLBINDRENDERBUFFEREXTPROC)WinGlGetProcAddress("glBindRenderbuffer");
    glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSEXTPROC)WinGlGetProcAddress("glDeleteRenderbuffers");

    glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEEXTPROC)WinGlGetProcAddress("glRenderbufferStorage");
    glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)WinGlGetProcAddress("glFramebufferRenderbuffer");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)WinGlGetProcAddress("glCheckFramebufferStatus");
    glGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)WinGlGetProcAddress("glGetVertexAttribiv");

    if (CanUseTextureBarrier())
    {
        glTextureBarrier = (PFNGLTEXTUREBARRIERPROC)WinGlGetProcAddress("glTextureBarrier");
    }
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

CubismRenderer* CubismRenderer::Create(csmUint32 width, csmUint32 height)
{
    return CSM_NEW CubismRenderer_OpenGLES2(width, height);
}

void CubismRenderer::StaticRelease()
{
    CubismRenderer_OpenGLES2::DoStaticRelease();
}

CubismRenderer_OpenGLES2::CubismRenderer_OpenGLES2(csmUint32 width, csmUint32 height)
    : CubismRenderer(width, height)
    , _drawableClippingManager(NULL)
    , _offscreenClippingManager(NULL)
    , _clippingContextBufferForMask(NULL)
    , _clippingContextBufferForDrawable(NULL)
    , _clippingContextBufferForOffscreen(NULL)
{
    // テクスチャ対応マップの容量を確保しておく.
    _textures.PrepareCapacity(32, true);
}

CubismRenderer_OpenGLES2::~CubismRenderer_OpenGLES2()
{
    CSM_DELETE_SELF(CubismClippingManager_OpenGLES2, _drawableClippingManager);
    CSM_DELETE_SELF(CubismClippingManager_OpenGLES2, _offscreenClippingManager);

    for (csmInt32 i = 0; i < _modelRenderTargets.GetSize(); ++i)
    {
        if (_modelRenderTargets[i].IsValid())
        {
            _modelRenderTargets[i].DestroyRenderTarget();
        }
    }
    _modelRenderTargets.Clear();

    for (csmInt32 i = 0; i < _drawableMasks.GetSize(); ++i)
    {
        if (_drawableMasks[i].IsValid())
        {
            _drawableMasks[i].DestroyRenderTarget();
        }
    }
    _drawableMasks.Clear();

    for (csmInt32 i = 0; i < _offscreenMasks.GetSize(); ++i)
    {
        if (_offscreenMasks[i].IsValid())
        {
            _offscreenMasks[i].DestroyRenderTarget();
        }
    }
    _offscreenMasks.Clear();
}

void CubismRenderer_OpenGLES2::DoStaticRelease()
{
#ifdef CSM_TARGET_WINGL
    s_isInitializeGlFunctionsSuccess = false;     ///< 初期化が完了したかどうか。trueなら初期化完了
    s_isFirstInitializeGlFunctions = true;        ///< 最初の初期化実行かどうか。trueなら最初の初期化実行
#endif
    CubismShader_OpenGLES2::DeleteInstance();
}

csmBool CubismRenderer_OpenGLES2::CanUseTextureBarrier()
{
#if defined(GLEW_ARB_texture_barrier)
    if (GLEW_ARB_texture_barrier)
    {
        return true;
    }
#elif defined(GLEW_NV_texture_barrier)
    if (GLEW_NV_texture_barrier)
    {
        return true;
    }
#endif
    return false;
}

void CubismRenderer_OpenGLES2::Initialize(CubismModel* model)
{
    Initialize(model, 1);
}

void CubismRenderer_OpenGLES2::Initialize(CubismModel* model, csmInt32 maskBufferCount)
{
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
        // TextureBarrierが使えない環境では2枚作成する
        // 添え字 0 は描画先となる
        // 添え字 1 はTextureBarrierの代替用
        csmInt32 createSize = CanUseTextureBarrier() ? 1 : 2;
        for (csmInt32 i = 0; i < createSize; ++i)
        {
            CubismRenderTarget_OpenGLES2 renderTarget;
            renderTarget.CreateRenderTarget(_modelRenderTargetWidth, _modelRenderTargetHeight, 0);
            _modelRenderTargets.PushBack(renderTarget);
        }
    }

    if (model->IsUsingMasking())
    {
        _drawableClippingManager = CSM_NEW CubismClippingManager_OpenGLES2();  //クリッピングマスク・バッファ前処理方式を初期化
        _drawableClippingManager->Initialize(
            *model,
            maskBufferCount,
            CubismRenderer::DrawableObjectType_Drawable
        );

        _drawableMasks.Clear();
        for (csmInt32 i = 0; i < maskBufferCount; ++i)
        {
            CubismRenderTarget_OpenGLES2 masks;
            masks.CreateRenderTarget(_drawableClippingManager->GetClippingMaskBufferSize().X, _drawableClippingManager->GetClippingMaskBufferSize().Y);
            _drawableMasks.PushBack(masks);
        }
    }

    if (model->IsUsingMaskingForOffscreen())
    {
        _offscreenClippingManager = CSM_NEW CubismClippingManager_OpenGLES2();  //クリッピングマスク・バッファ前処理方式を初期化
        _offscreenClippingManager->Initialize(
            *model,
            maskBufferCount,
            CubismRenderer::DrawableObjectType_Offscreen
        );

        _offscreenMasks.Clear();
        for (csmInt32 i = 0; i < maskBufferCount; ++i)
        {
            CubismRenderTarget_OpenGLES2 offscreenMask;
            offscreenMask.CreateRenderTarget(_offscreenClippingManager->GetClippingMaskBufferSize().X, _offscreenClippingManager->GetClippingMaskBufferSize().Y);
            _offscreenMasks.PushBack(offscreenMask);
        }
    }

    _sortedObjectsIndexList.Resize(model->GetDrawableCount() + model->GetOffscreenCount(), 0);
    _sortedObjectsTypeList.Resize(model->GetDrawableCount() + model->GetOffscreenCount(), DrawableObjectType_Drawable);

    const csmInt32 offscreenCount = model->GetOffscreenCount();

    // オフスクリーンの数が0の場合は何もしない
    if (offscreenCount > 0)
    {
        _offscreenList = csmVector<CubismOffscreenRenderTarget_OpenGLES2>(offscreenCount);
        for (csmInt32 offscreenIndex = 0; offscreenIndex < offscreenCount; ++offscreenIndex)
        {
            CubismOffscreenRenderTarget_OpenGLES2 renderTarget;
            renderTarget.SetOffscreenIndex(offscreenIndex);
            _offscreenList.PushBack(renderTarget);
        }

        // 全てのオフスクリーンを登録し終わってから行う
        SetupParentOffscreens(model, offscreenCount);
    }

    CubismRenderer::Initialize(model, maskBufferCount);  //親クラスの処理を呼ぶ
}

void CubismRenderer_OpenGLES2::SetupParentOffscreens(const CubismModel* model, csmInt32 offscreenCount)
{
    CubismOffscreenRenderTarget_OpenGLES2* parentOffscreen;
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

void CubismRenderer_OpenGLES2::PreDraw()
{
#ifdef CSM_TARGET_WIN_GL
    if (s_isFirstInitializeGlFunctions)
    {
        InitializeGlFunctions();
    }

    if (!s_isInitializeGlFunctionsSuccess)
    {
        return;
    }
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
    if (GetAnisotropy() >= 1.0f)
    {
        for (csmInt32 i = 0; i < _textures.GetSize(); i++)
        {
            glBindTexture(GL_TEXTURE_2D, _textures[i]);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GetAnisotropy());
        }
    }
}

void CubismRenderer_OpenGLES2::DoDrawModel()
{
    GLint lastFBO;
    GLint lastViewport[4];

    BeforeDrawModelRenderTarget();
    // モデル描画直前のFBOとビューポートを保存
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &lastFBO);
    glGetIntegerv(GL_VIEWPORT, lastViewport);

    //------------ クリッピングマスク・バッファ前処理方式の場合 ------------
    if (_drawableClippingManager != NULL)
    {
        PreDraw();

        // サイズが違う場合はここで作成しなおし
        for (csmInt32 i = 0; i < _drawableClippingManager->GetRenderTextureCount(); ++i)
        {
            if (_drawableMasks[i].GetBufferWidth() != static_cast<csmUint32>(_drawableClippingManager->GetClippingMaskBufferSize().X) ||
                _drawableMasks[i].GetBufferHeight() != static_cast<csmUint32>(_drawableClippingManager->GetClippingMaskBufferSize().Y))
            {
                _drawableMasks[i].CreateRenderTarget(
                    static_cast<csmUint32>(_drawableClippingManager->GetClippingMaskBufferSize().X), static_cast<csmUint32>(_drawableClippingManager->GetClippingMaskBufferSize().Y));
            }
        }

        if (IsUsingHighPrecisionMask())
        {
            _drawableClippingManager->SetupMatrixForHighPrecision(*GetModel(), false, DrawableObjectType_Drawable);
        }
        else
        {
            _drawableClippingManager->SetupClippingContext(*GetModel(), this, lastFBO, lastViewport, DrawableObjectType_Drawable);
        }
    }

    if (_offscreenClippingManager != NULL)
    {
        PreDraw();

        // サイズが違う場合はここで作成しなおし
        for (csmInt32 i = 0; i < _offscreenClippingManager->GetRenderTextureCount(); ++i)
        {
            if (_offscreenMasks[i].GetBufferWidth() != static_cast<csmUint32>(_offscreenClippingManager->GetClippingMaskBufferSize().X) ||
                _offscreenMasks[i].GetBufferHeight() != static_cast<csmUint32>(_offscreenClippingManager->GetClippingMaskBufferSize().Y))
            {
                _offscreenMasks[i].CreateRenderTarget(
                    static_cast<csmUint32>(_offscreenClippingManager->GetClippingMaskBufferSize().X), static_cast<csmUint32>(_offscreenClippingManager->GetClippingMaskBufferSize().Y));
            }
        }

        if (IsUsingHighPrecisionMask())
        {
            _offscreenClippingManager->SetupMatrixForHighPrecision(*GetModel(), false, DrawableObjectType_Offscreen, GetMvpMatrix());
        }
        else
        {
            _offscreenClippingManager->SetupClippingContext(*GetModel(), this, lastFBO, lastViewport, DrawableObjectType_Offscreen);
        }
    }

    // 上記クリッピング処理内でも一度PreDrawを呼ぶので注意!!
    PreDraw();

    // モデルの描画順に従って描画する
    DrawObjectLoop(lastFBO, lastViewport);

    PostDraw();

    AfterDrawModelRenderTarget();
}

void CubismRenderer_OpenGLES2::DrawObjectLoop(GLint lastFBO, GLint lastViewport[4])
{
    const csmInt32 drawableCount = GetModel()->GetDrawableCount();
    const csmInt32 offscreenCount = GetModel()->GetOffscreenCount();
    const csmInt32 totalCount = drawableCount + offscreenCount;
    const csmInt32* renderOrder = GetModel()->GetRenderOrders();

    _currentOffscreen = NULL;
    _currentFBO = lastFBO;
    _modelRootFBO = lastFBO;

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

    while (_currentOffscreen != NULL)
    {
        // オフスクリーンが残っている場合は親オフスクリーンへの伝搬を行う
        SubmitDrawToParentOffscreen(_currentOffscreen->GetOffscreenIndex(), DrawableObjectType_Offscreen);
    }
}

void CubismRenderer_OpenGLES2::RenderObject(const csmInt32 objectIndex, const csmInt32 objectType)
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

void CubismRenderer_OpenGLES2::DrawDrawable(csmInt32 drawableIndex)
{
    // Drawableが表示状態でなければ処理をパスする
    if (!GetModel()->GetDrawableDynamicFlagIsVisible(drawableIndex))
    {
        return;
    }

    SubmitDrawToParentOffscreen(drawableIndex, DrawableObjectType_Drawable);

    // クリッピングマスク
    CubismClippingContext_OpenGLES2* clipContext = (_drawableClippingManager != NULL) ?
        (*_drawableClippingManager->GetClippingContextListForDraw())[drawableIndex] :
        NULL;

    if (clipContext != NULL && IsUsingHighPrecisionMask()) // マスクを書く必要がある
    {
        if (clipContext->_isUsing) // 書くことになっていた
        {
            // 生成したRenderTargetと同じサイズでビューポートを設定
            glViewport(0, 0, _drawableClippingManager->GetClippingMaskBufferSize().X, _drawableClippingManager->GetClippingMaskBufferSize().Y);

            PreDraw(); // バッファをクリアする

            // ---------- マスク描画処理 ----------
            // マスク用RenderTextureをactiveにセット
            GetDrawableMaskBuffer(clipContext->_bufferIndex)->BeginDraw(_currentFBO);

            // マスクをクリアする
            // 1が無効（描かれない）領域、0が有効（描かれる）領域。（シェーダで Cd*Csで0に近い値をかけてマスクを作る。1をかけると何も起こらない）
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        {
            const csmInt32 clipDrawCount = clipContext->_clippingIdCount;
            for (csmInt32 index = 0; index < clipDrawCount; ++index)
            {
                const csmInt32 clipDrawIndex = clipContext->_clippingIdList[index];

                // 頂点情報が更新されておらず、信頼性がない場合は描画をパスする
                if (!GetModel()->GetDrawableDynamicFlagVertexPositionsDidChange(clipDrawIndex))
                {
                    continue;
                }

                IsCulling(GetModel()->GetDrawableCulling(clipDrawIndex) != 0);

                // 今回専用の変換を適用して描く
                // チャンネルも切り替える必要がある(A,R,G,B)
                SetClippingContextBufferForMask(clipContext);

                DrawMeshOpenGL(*GetModel(), clipDrawIndex);
            }
        }

        {
            // --- 後処理 ---
            GetDrawableMaskBuffer(clipContext->_bufferIndex)->EndDraw();
            SetClippingContextBufferForMask(NULL);
            glViewport(0, 0, _modelRenderTargetWidth, _modelRenderTargetHeight);

            PreDraw(); // バッファをクリアする
        }
    }

    // クリッピングマスクをセットする
    SetClippingContextBufferForDrawable(clipContext);

    IsCulling(GetModel()->GetDrawableCulling(drawableIndex) != 0);

    DrawMeshOpenGL(*GetModel(), drawableIndex);
}

void CubismRenderer_OpenGLES2::SubmitDrawToParentOffscreen(const csmInt32 objectIndex, const DrawableObjectType objectType)
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
    DrawOffscreen(_currentOffscreen);

    // さらに親のオフスクリーンに伝搬可能なら伝搬する。
    SubmitDrawToParentOffscreen(objectIndex, objectType);
}

void CubismRenderer_OpenGLES2::AddOffscreen(csmInt32 offscreenIndex)
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

    CubismOffscreenRenderTarget_OpenGLES2* offscreen = &_offscreenList.At(offscreenIndex);
    offscreen->SetOffscreenRenderTarget(_modelRenderTargetWidth, _modelRenderTargetHeight);

    // 以前のオフスクリーンレンダリングターゲットを取得
    CubismOffscreenRenderTarget_OpenGLES2* oldOffscreen = offscreen->GetParentPartOffscreen();

    offscreen->SetOldOffscreen(oldOffscreen);

    GLint oldFBO = 0;
    if (oldOffscreen != NULL)
    {
        oldFBO = oldOffscreen->GetRenderTarget()->GetRenderTexture();
    }

    if (oldFBO == 0)
    {
        oldFBO = _modelRootFBO; // ルートのFBOを使用する
    }

    // 別バッファに描画を開始
    offscreen->GetRenderTarget()->BeginDraw(oldFBO);
    glViewport(0, 0, _modelRenderTargetWidth, _modelRenderTargetHeight);
    offscreen->GetRenderTarget()->Clear(0.0f, 0.0f, 0.0f, 0.0f);

    // 現在のオフスクリーンレンダリングターゲットを設定
    _currentOffscreen = offscreen;
    _currentFBO = offscreen->GetRenderTarget()->GetRenderTexture();
}

void CubismRenderer_OpenGLES2::DrawOffscreen(CubismOffscreenRenderTarget_OpenGLES2* currentOffscreen)
{
    csmInt32 offscreenIndex = currentOffscreen->GetOffscreenIndex();
    // クリッピングマスク
    CubismClippingContext_OpenGLES2* clipContext = (_offscreenClippingManager != NULL) ?
        (*_offscreenClippingManager->GetClippingContextListForOffscreen())[offscreenIndex] :
        NULL;

    if (clipContext != NULL && IsUsingHighPrecisionMask()) // マスクを書く必要がある
    {
        if (clipContext->_isUsing) // 書くことになっていた
        {
            // 生成したRenderTargetと同じサイズでビューポートを設定
            glViewport(0, 0, _offscreenClippingManager->GetClippingMaskBufferSize().X, _offscreenClippingManager->GetClippingMaskBufferSize().Y);

            PreDraw(); // バッファをクリアする

            // ---------- マスク描画処理 ----------
            // マスク用RenderTextureをactiveにセット
            GetOffscreenMaskBuffer(clipContext->_bufferIndex)->BeginDraw(_currentFBO);

            // マスクをクリアする
            // 1が無効（描かれない）領域、0が有効（描かれる）領域。（シェーダで Cd*Csで0に近い値をかけてマスクを作る。1をかけると何も起こらない）
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
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

                // 今回専用の変換を適用して描く
                // チャンネルも切り替える必要がある(A,R,G,B)
                SetClippingContextBufferForMask(clipContext);

                DrawMeshOpenGL(*GetModel(), clipDrawIndex);
            }
        }

        {
            // --- 後処理 ---
            GetOffscreenMaskBuffer(clipContext->_bufferIndex)->EndDraw();
            SetClippingContextBufferForMask(NULL);
            glViewport(0, 0, _modelRenderTargetWidth, _modelRenderTargetHeight);

            PreDraw(); // バッファをクリアする
        }
    }

    // クリッピングマスクをセットする
    SetClippingContextBufferForOffscreen(clipContext);

    IsCulling(GetModel()->GetOffscreenCulling(offscreenIndex) != 0);

    DrawOffscreenOpenGL(*GetModel(), currentOffscreen);
}

void CubismRenderer_OpenGLES2::DrawMeshOpenGL(const CubismModel& model, const csmInt32 index)
{
#ifdef CSM_TARGET_WIN_GL
    if (s_isFirstInitializeGlFunctions)
    {
        return;  // WindowsプラットフォームではGL命令のバインドを済ませておく必要がある
    }
#endif

#ifndef CSM_DEBUG
    if (_textures[model.GetDrawableTextureIndex(index)] == 0)
    {
        return; // モデルが参照するテクスチャがバインドされていない場合は描画をスキップする
    }
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

    glFrontFace(GL_CCW);    // Cubism SDK OpenGLはマスク・アートメッシュ共にCCWが表面

    if (IsGeneratingMask())  // マスク生成時
    {
        CubismShader_OpenGLES2::GetInstance()->SetupShaderProgramForMask(this, model, index);
    }
    else{
        CubismShader_OpenGLES2::GetInstance()->SetupShaderProgramForDrawable(this, model, index);
    }

    // ポリゴンメッシュを描画する
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    if(currentProgram != 0)
    {
        csmInt32 indexCount = model.GetDrawableVertexIndexCount(index);
        csmUint16* indexArray = const_cast<csmUint16*>(model.GetDrawableVertexIndices(index));
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, indexArray);
    }

    // 後処理
    glUseProgram(0);
    SetClippingContextBufferForDrawable(NULL);
    SetClippingContextBufferForMask(NULL);
}

void CubismRenderer_OpenGLES2::DrawOffscreenOpenGL(const CubismModel& model, CubismOffscreenRenderTarget_OpenGLES2* offscreen)
{

#ifdef CSM_TARGET_WIN_GL
    if (s_isFirstInitializeGlFunctions)
    {
        return;  // WindowsプラットフォームではGL命令のバインドを済ませておく必要がある
    }
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

    glFrontFace(GL_CCW);    // Cubism SDK OpenGLはマスク・アートメッシュ共にCCWが表面

    offscreen->GetRenderTarget()->EndDraw();
    _currentOffscreen = _currentOffscreen->GetOldOffscreen();
    _currentFBO = offscreen->GetRenderTarget()->GetOldFBO();

    CubismShader_OpenGLES2::GetInstance()->SetupShaderProgramForOffscreen(this, model, offscreen);

    // ポリゴンメッシュを描画する
    glDrawElements(GL_TRIANGLES, sizeof(ModelRenderTargetIndexArray) / sizeof(csmUint16), GL_UNSIGNED_SHORT, ModelRenderTargetIndexArray);

    // 後処理
    offscreen->StopUsingRenderTexture();
    glUseProgram(0);
    SetClippingContextBufferForOffscreen(NULL);
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

void CubismRenderer_OpenGLES2::BeforeDrawModelRenderTarget()
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
            _modelRenderTargets[i].CreateRenderTarget(_modelRenderTargetWidth, _modelRenderTargetHeight, 0);
        }
    }

    // 別バッファに描画を開始
    _modelRenderTargets[0].BeginDraw();
    _modelRenderTargets[0].Clear(0.0f, 0.0f, 0.0f, 0.0f);
}

void CubismRenderer_OpenGLES2::AfterDrawModelRenderTarget()
{
    if (_modelRenderTargets.GetSize() == 0)
    {
        return;
    }

    // 元のバッファに描画する
    _modelRenderTargets[0].EndDraw();

    CubismShader_OpenGLES2::GetInstance()->SetupShaderProgramForOffscreenRenderTarget(this);

    glDrawElements(GL_TRIANGLES, sizeof(ModelRenderTargetIndexArray) / sizeof(csmUint16), GL_UNSIGNED_SHORT, ModelRenderTargetIndexArray);

    glUseProgram(0);
}

void CubismRenderer_OpenGLES2::BindTexture(csmUint32 modelTextureIndex, GLuint glTextureIndex)
{
    _textures[modelTextureIndex] = glTextureIndex;
}

const csmMap<csmInt32, GLuint>& CubismRenderer_OpenGLES2::GetBindedTextures() const
{
    return _textures;
}

void CubismRenderer_OpenGLES2::SetDrawableClippingMaskBufferSize(csmFloat32 width, csmFloat32 height)
{
    if (_drawableClippingManager == NULL)
    {
        return;
    }

    // インスタンス破棄前にレンダーテクスチャの数を保存
    const csmInt32 renderTextureCount = _drawableClippingManager->GetRenderTextureCount();

    // RenderTargetのサイズを変更するためにインスタンスを破棄・再作成する
    CSM_DELETE_SELF(CubismClippingManager_OpenGLES2, _drawableClippingManager);

    _drawableClippingManager = CSM_NEW CubismClippingManager_OpenGLES2();

    _drawableClippingManager->SetClippingMaskBufferSize(width, height);

    _drawableClippingManager->Initialize(
        *GetModel(),
        renderTextureCount,
        CubismRenderer::DrawableObjectType_Drawable
    );
}

void CubismRenderer_OpenGLES2::SetOffscreenClippingMaskBufferSize(csmFloat32 width, csmFloat32 height)
{
    if (_offscreenClippingManager == NULL)
    {
        return;
    }

    // インスタンス破棄前にレンダーテクスチャの数を保存
    const csmInt32 renderTextureCount = _offscreenClippingManager->GetRenderTextureCount();

    // RenderTargetのサイズを変更するためにインスタンスを破棄・再作成する
    CSM_DELETE_SELF(CubismClippingManager_OpenGLES2, _offscreenClippingManager);

    _offscreenClippingManager = CSM_NEW CubismClippingManager_OpenGLES2();

    _offscreenClippingManager->SetClippingMaskBufferSize(width, height);

    _offscreenClippingManager->Initialize(
        *GetModel(),
        renderTextureCount,
        CubismRenderer::DrawableObjectType_Offscreen
    );
}

csmInt32 CubismRenderer_OpenGLES2::GetDrawableRenderTextureCount() const
{
    return _drawableClippingManager->GetRenderTextureCount();
}

csmInt32 CubismRenderer_OpenGLES2::GetOffscreenRenderTextureCount() const
{
    return _offscreenClippingManager->GetRenderTextureCount();
}

CubismVector2 CubismRenderer_OpenGLES2::GetDrawableClippingMaskBufferSize() const
{
    return _drawableClippingManager->GetClippingMaskBufferSize();
}

CubismVector2 CubismRenderer_OpenGLES2::GetOffscreenClippingMaskBufferSize() const
{
    return _offscreenClippingManager->GetClippingMaskBufferSize();
}

const CubismRenderTarget_OpenGLES2* CubismRenderer_OpenGLES2::CopyOffscreenRenderTarget()
{
    return CopyRenderTarget(_modelRenderTargets[0]);
}

const CubismRenderTarget_OpenGLES2* CubismRenderer_OpenGLES2::CopyRenderTarget(const CubismRenderTarget_OpenGLES2& srcBuffer)
{
#if defined(GLEW_ARB_texture_barrier)
    if (GLEW_ARB_texture_barrier)
    {
        glTextureBarrier();
        return &srcBuffer;
    }
#elif defined(GLEW_NV_texture_barrier)
    if (GLEW_NV_texture_barrier)
    {
        glTextureBarrier();
        return &srcBuffer;
    }
#endif

    // textureBarrierが無効な場合は、オフスクリーンの内容をコピーしてから描画する
#if defined(CSM_TARGET_ANDROID_ES2) || defined(CSM_TARGET_IPHONE_ES2)
    _modelRenderTargets[1].BeginDraw();

    CubismShader_OpenGLES2::GetInstance()->CopyTexture(srcBuffer.GetColorBuffer());
    glDrawElements(GL_TRIANGLES, sizeof(ModelRenderTargetIndexArray) / sizeof(csmUint16), GL_UNSIGNED_SHORT, ModelRenderTargetIndexArray);

    _modelRenderTargets[1].EndDraw();

    return &_modelRenderTargets[1];
#else
    CubismRenderTarget_OpenGLES2::CopyBuffer(srcBuffer, _modelRenderTargets[1]);

    return &_modelRenderTargets[1];
#endif
}

CubismRenderTarget_OpenGLES2* CubismRenderer_OpenGLES2::GetDrawableMaskBuffer(csmInt32 index)
{
    return &_drawableMasks[index];
}

CubismRenderTarget_OpenGLES2* CubismRenderer_OpenGLES2::GetOffscreenMaskBuffer(csmInt32 index)
{
    return &_offscreenMasks[index];
}

CubismOffscreenRenderTarget_OpenGLES2* CubismRenderer_OpenGLES2::GetCurrentOffscreen() const
{
    return _currentOffscreen;
}

void CubismRenderer_OpenGLES2::SetClippingContextBufferForMask(CubismClippingContext_OpenGLES2* clip)
{
    _clippingContextBufferForMask = clip;
}

CubismClippingContext_OpenGLES2* CubismRenderer_OpenGLES2::GetClippingContextBufferForMask() const
{
    return _clippingContextBufferForMask;
}

void CubismRenderer_OpenGLES2::SetClippingContextBufferForDrawable(CubismClippingContext_OpenGLES2* clip)
{
    _clippingContextBufferForDrawable = clip;
}

CubismClippingContext_OpenGLES2* CubismRenderer_OpenGLES2::GetClippingContextBufferForDrawable() const
{
    return _clippingContextBufferForDrawable;
}

void CubismRenderer_OpenGLES2::SetClippingContextBufferForOffscreen(CubismClippingContext_OpenGLES2* clip)
{
    _clippingContextBufferForOffscreen = clip;
}

CubismClippingContext_OpenGLES2* CubismRenderer_OpenGLES2::GetClippingContextBufferForOffscreen() const
{
    return _clippingContextBufferForOffscreen;
}

const csmBool inline CubismRenderer_OpenGLES2::IsGeneratingMask() const
{
    return (GetClippingContextBufferForMask() != NULL);
}

GLuint CubismRenderer_OpenGLES2::GetBindedTextureId(csmInt32 textureId)
{
    return (_textures[textureId] != 0) ? _textures[textureId] : -1;
}

}}}}

//------------ LIVE2D NAMESPACE ------------
