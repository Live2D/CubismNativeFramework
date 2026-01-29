/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismShader_OpenGLES2.hpp"
#include <float.h>
#include "Type/csmRectF.hpp"

#ifdef CSM_TARGET_WIN_GL
#include <Windows.h>
#endif

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/*********************************************************************************************************************
*                                       CubismShader_OpenGLES2
********************************************************************************************************************/
namespace {
    CubismShader_OpenGLES2* s_instance;
#ifdef CSM_TARGET_ANDROID_ES2
    const csmChar* ColorBlendShaderPath = "FragShaderSrcColorBlend.frag";
    const csmChar* AlphaBlendShaderPath = "FragShaderSrcAlphaBlend.frag";
#else
    const csmChar* ColorBlendShaderPath = "FrameworkShaders/FragShaderSrcColorBlend.frag";
    const csmChar* AlphaBlendShaderPath = "FrameworkShaders/FragShaderSrcAlphaBlend.frag";
#endif
    const csmFloat32 renderTargetVertexArray[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };
    const csmFloat32 renderTargetUvArray[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    };

    const csmFloat32 renderTargetReverseUvArray[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
    };
}

enum ShaderNames
{
#define CSM_CREATE_SHADER_NAMES(NAME) \
    ShaderNames_ ## NAME, \
    ShaderNames_ ## NAME ## Masked, \
    ShaderNames_ ## NAME ## MaskedInverted, \
    ShaderNames_ ## NAME ## PremultipliedAlpha, \
    ShaderNames_ ## NAME ## MaskedPremultipliedAlpha, \
    ShaderNames_ ## NAME ## MaskedInvertedPremultipliedAlpha

#define CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(NAME) \
    CSM_CREATE_SHADER_NAMES(NAME ## Over), \
    CSM_CREATE_SHADER_NAMES(NAME ## Atop), \
    CSM_CREATE_SHADER_NAMES(NAME ## Out), \
    CSM_CREATE_SHADER_NAMES(NAME ## ConjointOver), \
    CSM_CREATE_SHADER_NAMES(NAME ## DisjointOver)

    // Copy
    ShaderNames_Copy,
    // SetupMask
    ShaderNames_SetupMask,
    // Normal
    CSM_CREATE_SHADER_NAMES(Normal),
    // Add
    CSM_CREATE_SHADER_NAMES(Add),
    // Mult
    CSM_CREATE_SHADER_NAMES(Mult),
    // Normal
    CSM_CREATE_SHADER_NAMES(NormalAtop),
    CSM_CREATE_SHADER_NAMES(NormalOut),
    CSM_CREATE_SHADER_NAMES(NormalConjointOver),
    CSM_CREATE_SHADER_NAMES(NormalDisjointOver),
    // 加算
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Add),
    // 加算(発光)
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(AddGlow),
    // 比較(暗)
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Darken),
    // 乗算
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Multiply),
    // 焼き込みカラー
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(ColorBurn),
    // 焼き込み(リニア)
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(LinearBurn),
    // 比較(明)
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Lighten),
    // スクリーン
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Screen),
    // 覆い焼きカラー
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(ColorDodge),
    // オーバーレイ
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Overlay),
    // ソフトライト
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(SoftLight),
    // ハードライト
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(HardLight),
    // リニアライト
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(LinearLight),
    // 色相
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Hue),
    // カラー
    CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES(Color),

    // ブレンドモードの組み合わせ数 = 加算(5.2以前) + 乗算(5.2以前) + (通常 + 加算 + 加算(発光) + 比較(暗) + 乗算 + 焼き込みカラー + 焼き込み(リニア) + 比較(明) + スクリーン + 覆い焼きカラー + オーバーレイ + ソフトライト + ハードライト + リニアライト + 色相 + カラー) * (over + atop + out + conjoint over + disjoint over)
    // シェーダの数 = コピー用 + マスク生成用 + (通常 + 加算 + 乗算 + ブレンドモードの組み合わせ数) * (マスク無 + マスク有 + マスク有反転 + マスク無の乗算済アルファ対応版 + マスク有の乗算済アルファ対応版 + マスク有反転の乗算済アルファ対応版)
    ShaderNames_ShaderCount,

#undef CSM_CREATE_BLEND_OVERLAP_SHADER_NAMES
#undef CSM_CREATE_SHADER_NAMES
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

void CubismShader_OpenGLES2::ReleaseInvalidShaderProgram()
{
    for (csmUint32 i = 0; i < _shaderSets.GetSize(); i++)
    {
        CSM_DELETE(_shaderSets[i]);
    }
    _shaderSets.Clear();
}


void CubismShader_OpenGLES2::SetShaderSet(CubismShaderSet& shaderSets, const MaskType maskType, csmBool isBlendMode)
{
    shaderSets.AttributePositionLocation = glGetAttribLocation(shaderSets.ShaderProgram, "a_position");
    shaderSets.AttributeTexCoordLocation = glGetAttribLocation(shaderSets.ShaderProgram, "a_texCoord");
    shaderSets.SamplerTexture0Location = glGetUniformLocation(shaderSets.ShaderProgram, "s_texture0");
    if (isBlendMode)
    {
        shaderSets.SamplerBlendTextureLocation = glGetUniformLocation(shaderSets.ShaderProgram, "s_blendTexture");
    }
    shaderSets.UniformMatrixLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_matrix");
    shaderSets.UniformBaseColorLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_baseColor");
    shaderSets.UniformMultiplyColorLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_multiplyColor");
    shaderSets.UniformScreenColorLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_screenColor");

    switch (maskType)
    {
    case MaskType_None:
        break;
    case MaskType_Masked: // クリッピング
        shaderSets.SamplerTexture1Location = glGetUniformLocation(shaderSets.ShaderProgram, "s_texture1");
        shaderSets.UniformClipMatrixLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_clipMatrix");
        shaderSets.UnifromChannelFlagLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_channelFlag");
        break;
    case MaskType_MaskedInverted: // クリッピング・反転
        shaderSets.SamplerTexture1Location = glGetUniformLocation(shaderSets.ShaderProgram, "s_texture1");
        shaderSets.UniformClipMatrixLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_clipMatrix");
        shaderSets.UnifromChannelFlagLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_channelFlag");
        break;
    case MaskType_PremultipliedAlpha: // PremultipliedAlpha
        break;
    case MaskType_MaskedPremultipliedAlpha: // クリッピング、PremultipliedAlpha
        shaderSets.SamplerTexture1Location = glGetUniformLocation(shaderSets.ShaderProgram, "s_texture1");
        shaderSets.UniformClipMatrixLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_clipMatrix");
        shaderSets.UnifromChannelFlagLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_channelFlag");
        break;
    case MaskType_MaskedInvertedPremultipliedAlpha: // クリッピング・反転、PremultipliedAlpha
        shaderSets.SamplerTexture1Location = glGetUniformLocation(shaderSets.ShaderProgram, "s_texture1");
        shaderSets.UniformClipMatrixLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_clipMatrix");
        shaderSets.UnifromChannelFlagLocation = glGetUniformLocation(shaderSets.ShaderProgram, "u_channelFlag");
        break;
    }
}

csmInt32 CubismShader_OpenGLES2::GetShaderNamesBegin(const csmBlendMode blendMode)
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
void CubismShader_OpenGLES2::SetExtShaderMode(csmBool extMode, csmBool extPAMode)
{
    s_extMode = extMode;
    s_extPAMode = extPAMode;
}
#endif

void CubismShader_OpenGLES2::GenerateShaders()
{
    for (csmInt32 i = 0; i < ShaderNames_ShaderCount; i++)
    {
        _shaderSets.PushBack(CSM_NEW CubismShaderSet());
    }

#ifdef CSM_TARGET_ANDROID_ES2
    if (s_extMode)
    {
        _shaderSets[ShaderNames_Copy]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcCopy.vert", "FragShaderSrcCopyTegra.frag");
        _shaderSets[ShaderNames_SetupMask]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcSetupMask.vert", "FragShaderSrcSetupMaskTegra.frag");

        _shaderSets[ShaderNames_Normal]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrc.vert", "FragShaderSrcTegra.frag");
        _shaderSets[ShaderNames_NormalMasked]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMasked.vert", "FragShaderSrcMaskTegra.frag");
        _shaderSets[ShaderNames_NormalMaskedInverted]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMasked.vert", "FragShaderSrcMaskInvertedTegra.frag");
        _shaderSets[ShaderNames_NormalPremultipliedAlpha]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrc.vert", "FragShaderSrcPremultipliedAlphaTegra.frag");
        _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMasked.vert", "FragShaderSrcMaskPremultipliedAlphaTegra.frag");
        _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMasked.vert", "FragShaderSrcMaskInvertedPremultipliedAlphaTegra.frag");

        // ブレンドモードの組み合わせ分作成
        {
            csmUint32 offset = ShaderNames_NormalAtop;
            for (csmInt32 i = 0; i < ColorBlendMode_Count; ++i)
            {
                // Normal Overはシェーダを作る必要がないため 1 から始める
                const csmInt32 start = (i == 0 ? 1 : 0);
                for (csmInt32 j = start; j < AlphaBlendMode_Count; ++j)
                {
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcBlend.vert", "FragShaderSrcBlendTegra.frag", i, j);
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMaskedBlend.vert", "FragShaderSrcMaskBlendTegra.frag", i, j);
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMaskedBlend.vert", "FragShaderSrcMaskInvertedBlendTegra.frag", i, j);
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcBlend.vert", "FragShaderSrcPremultipliedAlphaBlendTegra.frag", i, j);
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMaskedBlend.vert", "FragShaderSrcMaskPremultipliedAlphaBlendTegra.frag", i, j);
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMaskedBlend.vert", "FragShaderSrcMaskInvertedPremultipliedAlphaBlendTegra.frag", i, j);
                }
            }
        }
    }
    else
    {
        _shaderSets[ShaderNames_Copy]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcCopy.vert", "FragShaderSrcCopy.frag");
        _shaderSets[ShaderNames_SetupMask]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcSetupMask.vert", "FragShaderSrcSetupMask.frag");

        _shaderSets[ShaderNames_Normal]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrc.vert", "FragShaderSrc.frag");
        _shaderSets[ShaderNames_NormalMasked]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMasked.vert", "FragShaderSrcMask.frag");
        _shaderSets[ShaderNames_NormalMaskedInverted]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMasked.vert", "FragShaderSrcMaskInverted.frag");
        _shaderSets[ShaderNames_NormalPremultipliedAlpha]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrc.vert", "FragShaderSrcPremultipliedAlpha.frag");
        _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMasked.vert", "FragShaderSrcMaskPremultipliedAlpha.frag");
        _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMasked.vert", "FragShaderSrcMaskInvertedPremultipliedAlpha.frag");

        // ブレンドモードの組み合わせ分作成
        {
            csmUint32 offset = ShaderNames_NormalAtop;
            for (csmInt32 i = 0; i < ColorBlendMode_Count; ++i)
            {
                // Normal Overはシェーダを作る必要がないため 1 から始める
                const csmInt32 start = (i == 0 ? 1 : 0);
                for (csmInt32 j = start; j < AlphaBlendMode_Count; ++j)
                {
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcBlend.vert", "FragShaderSrcBlend.frag", i, j);
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMaskedBlend.vert", "FragShaderSrcMaskBlend.frag", i, j);
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMaskedBlend.vert", "FragShaderSrcMaskInvertedBlend.frag", i, j);
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcBlend.vert", "FragShaderSrcPremultipliedAlphaBlend.frag", i, j);
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMaskedBlend.vert", "FragShaderSrcMaskPremultipliedAlphaBlend.frag", i, j);
                    _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("VertShaderSrcMaskedBlend.vert", "FragShaderSrcMaskInvertedPremultipliedAlphaBlend.frag", i, j);
                }
            }
        }
    }

    // 加算も通常と同じシェーダーを利用する
    _shaderSets[ShaderNames_Add]->ShaderProgram = _shaderSets[ShaderNames_Normal]->ShaderProgram;
    _shaderSets[ShaderNames_AddMasked]->ShaderProgram = _shaderSets[ShaderNames_NormalMasked]->ShaderProgram;
    _shaderSets[ShaderNames_AddMaskedInverted]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInverted]->ShaderProgram;
    _shaderSets[ShaderNames_AddPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_AddMaskedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_AddMaskedInvertedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram;

    // 乗算も通常と同じシェーダーを利用する
    _shaderSets[ShaderNames_Mult]->ShaderProgram = _shaderSets[ShaderNames_Normal]->ShaderProgram;
    _shaderSets[ShaderNames_MultMasked]->ShaderProgram = _shaderSets[ShaderNames_NormalMasked]->ShaderProgram;
    _shaderSets[ShaderNames_MultMaskedInverted]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInverted]->ShaderProgram;
    _shaderSets[ShaderNames_MultPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_MultMaskedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_MultMaskedInvertedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram;

#else

    _shaderSets[ShaderNames_Copy]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcCopy.vert", "FrameworkShaders/FragShaderSrcCopy.frag");
    _shaderSets[ShaderNames_SetupMask]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcSetupMask.vert", "FrameworkShaders/FragShaderSrcSetupMask.frag");

    // 通常
    _shaderSets[ShaderNames_Normal]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrc.vert", "FrameworkShaders/FragShaderSrc.frag");
    _shaderSets[ShaderNames_NormalMasked]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcMasked.vert", "FrameworkShaders/FragShaderSrcMask.frag");
    _shaderSets[ShaderNames_NormalMaskedInverted]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcMasked.vert", "FrameworkShaders/FragShaderSrcMaskInverted.frag");
    _shaderSets[ShaderNames_NormalPremultipliedAlpha]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrc.vert", "FrameworkShaders/FragShaderSrcPremultipliedAlpha.frag");
    _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcMasked.vert", "FrameworkShaders/FragShaderSrcMaskPremultipliedAlpha.frag");
    _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcMasked.vert", "FrameworkShaders/FragShaderSrcMaskInvertedPremultipliedAlpha.frag");

    // 加算も通常と同じシェーダーを利用する
    _shaderSets[ShaderNames_Add]->ShaderProgram = _shaderSets[ShaderNames_Normal]->ShaderProgram;
    _shaderSets[ShaderNames_AddMasked]->ShaderProgram = _shaderSets[ShaderNames_NormalMasked]->ShaderProgram;
    _shaderSets[ShaderNames_AddMaskedInverted]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInverted]->ShaderProgram;
    _shaderSets[ShaderNames_AddPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_AddMaskedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_AddMaskedInvertedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram;

    // 乗算も通常と同じシェーダーを利用する
    _shaderSets[ShaderNames_Mult]->ShaderProgram = _shaderSets[ShaderNames_Normal]->ShaderProgram;
    _shaderSets[ShaderNames_MultMasked]->ShaderProgram = _shaderSets[ShaderNames_NormalMasked]->ShaderProgram;
    _shaderSets[ShaderNames_MultMaskedInverted]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInverted]->ShaderProgram;
    _shaderSets[ShaderNames_MultPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_MultMaskedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_MultMaskedInvertedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram;

    // ブレンドモードの組み合わせ分作成
    {
        csmUint32 offset = ShaderNames_NormalAtop;
        for (csmInt32 i = 0; i < ColorBlendMode_Count; ++i)
        {
            // Normal Overはシェーダを作る必要がないため 1 から始める
            const csmInt32 start = (i == 0 ? 1 : 0);
            for (csmInt32 j = start; j < AlphaBlendMode_Count; ++j)
            {
                _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcBlend.vert", "FrameworkShaders/FragShaderSrcBlend.frag", i, j);
                _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcMaskedBlend.vert", "FrameworkShaders/FragShaderSrcMaskBlend.frag", i, j);
                _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcMaskedBlend.vert", "FrameworkShaders/FragShaderSrcMaskInvertedBlend.frag", i, j);
                _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcBlend.vert", "FrameworkShaders/FragShaderSrcPremultipliedAlphaBlend.frag", i, j);
                _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcMaskedBlend.vert", "FrameworkShaders/FragShaderSrcMaskPremultipliedAlphaBlend.frag", i, j);
                _shaderSets[offset++]->ShaderProgram = LoadShaderProgramFromFile("FrameworkShaders/VertShaderSrcMaskedBlend.vert", "FrameworkShaders/FragShaderSrcMaskInvertedPremultipliedAlphaBlend.frag", i, j);
            }
        }
    }
#endif

    // Copy
    _shaderSets[ShaderNames_Copy]->AttributePositionLocation = glGetAttribLocation(_shaderSets[ShaderNames_Copy]->ShaderProgram, "a_position");
    _shaderSets[ShaderNames_Copy]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[ShaderNames_Copy]->ShaderProgram, "a_texCoord");
    _shaderSets[ShaderNames_Copy]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[ShaderNames_Copy]->ShaderProgram, "s_texture0");
    _shaderSets[ShaderNames_Copy]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[ShaderNames_Copy]->ShaderProgram, "u_baseColor");

    // SetupMask
    _shaderSets[ShaderNames_SetupMask]->AttributePositionLocation = glGetAttribLocation(_shaderSets[ShaderNames_SetupMask]->ShaderProgram, "a_position");
    _shaderSets[ShaderNames_SetupMask]->AttributeTexCoordLocation = glGetAttribLocation(_shaderSets[ShaderNames_SetupMask]->ShaderProgram, "a_texCoord");
    _shaderSets[ShaderNames_SetupMask]->SamplerTexture0Location = glGetUniformLocation(_shaderSets[ShaderNames_SetupMask]->ShaderProgram, "s_texture0");
    _shaderSets[ShaderNames_SetupMask]->UniformClipMatrixLocation = glGetUniformLocation(_shaderSets[ShaderNames_SetupMask]->ShaderProgram, "u_clipMatrix");
    _shaderSets[ShaderNames_SetupMask]->UnifromChannelFlagLocation = glGetUniformLocation(_shaderSets[ShaderNames_SetupMask]->ShaderProgram, "u_channelFlag");
    _shaderSets[ShaderNames_SetupMask]->UniformBaseColorLocation = glGetUniformLocation(_shaderSets[ShaderNames_SetupMask]->ShaderProgram, "u_baseColor");
    _shaderSets[ShaderNames_SetupMask]->UniformMultiplyColorLocation = glGetUniformLocation(_shaderSets[ShaderNames_SetupMask]->ShaderProgram, "u_multiplyColor");
    _shaderSets[ShaderNames_SetupMask]->UniformScreenColorLocation = glGetUniformLocation(_shaderSets[ShaderNames_SetupMask]->ShaderProgram, "u_screenColor");

    // 通常
    SetShaderSet(*_shaderSets[ShaderNames_Normal], MaskType_None);
    // 通常（クリッピング）
    SetShaderSet(*_shaderSets[ShaderNames_NormalMasked], MaskType_Masked);
    // 通常（クリッピング・反転）
    SetShaderSet(*_shaderSets[ShaderNames_NormalMaskedInverted], MaskType_MaskedInverted);
    // 通常（PremultipliedAlpha）
    SetShaderSet(*_shaderSets[ShaderNames_NormalPremultipliedAlpha], MaskType_PremultipliedAlpha);
    // 通常（クリッピング、PremultipliedAlpha）
    SetShaderSet(*_shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha], MaskType_MaskedPremultipliedAlpha);
    // 通常（クリッピング・反転、PremultipliedAlpha）
    SetShaderSet(*_shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha], MaskType_MaskedInvertedPremultipliedAlpha);

    // 加算
    SetShaderSet(*_shaderSets[ShaderNames_Add], MaskType_None);
    // 加算（クリッピング）
    SetShaderSet(*_shaderSets[ShaderNames_AddMasked], MaskType_Masked);
    // 加算（クリッピング・反転）
    SetShaderSet(*_shaderSets[ShaderNames_AddMaskedInverted], MaskType_MaskedInverted);
    // 加算（PremultipliedAlpha）
    SetShaderSet(*_shaderSets[ShaderNames_AddPremultipliedAlpha], MaskType_PremultipliedAlpha);
    // 加算（クリッピング、PremultipliedAlpha）
    SetShaderSet(*_shaderSets[ShaderNames_AddMaskedPremultipliedAlpha], MaskType_MaskedPremultipliedAlpha);
    // 加算（クリッピング・反転、PremultipliedAlpha）
    SetShaderSet(*_shaderSets[ShaderNames_AddMaskedInvertedPremultipliedAlpha], MaskType_MaskedInvertedPremultipliedAlpha);

    // 乗算
    SetShaderSet(*_shaderSets[ShaderNames_Mult], MaskType_None);
    // 乗算（クリッピング）
    SetShaderSet(*_shaderSets[ShaderNames_MultMasked], MaskType_Masked);
    // 乗算（クリッピング・反転）
    SetShaderSet(*_shaderSets[ShaderNames_MultMaskedInverted], MaskType_MaskedInverted);
    // 乗算（PremultipliedAlpha）
    SetShaderSet(*_shaderSets[ShaderNames_MultPremultipliedAlpha], MaskType_PremultipliedAlpha);
    // 乗算（クリッピング、PremultipliedAlpha）
    SetShaderSet(*_shaderSets[ShaderNames_MultMaskedPremultipliedAlpha], MaskType_MaskedPremultipliedAlpha);
    // 乗算（クリッピング・反転、PremultipliedAlpha）
    SetShaderSet(*_shaderSets[ShaderNames_MultMaskedInvertedPremultipliedAlpha], MaskType_MaskedInvertedPremultipliedAlpha);

    // ブレンドモードの組み合わせ分作成
    {
        csmUint32 offset = ShaderNames_NormalAtop;
        for (csmInt32 i = 0; i < ColorBlendMode_Count; ++i)
        {
            // Normal Overはシェーダを作る必要がないため 1 から始める
            const csmInt32 start = (i == 0 ? 1 : 0);
            for (csmInt32 j = start; j < AlphaBlendMode_Count; ++j)
            {
                //
                SetShaderSet(*_shaderSets[offset++], MaskType_None, true);
                // クリッピング
                SetShaderSet(*_shaderSets[offset++], MaskType_Masked, true);
                // クリッピング・反転
                SetShaderSet(*_shaderSets[offset++], MaskType_MaskedInverted, true);
                // PremultipliedAlpha
                SetShaderSet(*_shaderSets[offset++], MaskType_PremultipliedAlpha, true);
                // クリッピング、PremultipliedAlpha
                SetShaderSet(*_shaderSets[offset++], MaskType_MaskedPremultipliedAlpha, true);
                // クリッピング・反転、PremultipliedAlpha
                SetShaderSet(*_shaderSets[offset++], MaskType_MaskedInvertedPremultipliedAlpha, true);
            }
        }
    }
}

void CubismShader_OpenGLES2::SetupShaderProgramForDrawable(CubismRenderer_OpenGLES2* renderer, const CubismModel& model, const csmInt32 index)
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

    // _shaderSets用のオフセット計算
    const csmBool masked = renderer->GetClippingContextBufferForDrawable() != NULL;  // この描画オブジェクトはマスク対象か
    const csmBool invertedMask = model.GetDrawableInvertedMask(index);
    const csmBool isPremultipliedAlpha = renderer->IsPremultipliedAlpha();
    const csmInt32 offset = (masked ? ( invertedMask ? 2 : 1 ) : 0) + (isPremultipliedAlpha ? 3 : 0);

    // シェーダーセット
    const csmInt32 shaderNameBegin = GetShaderNamesBegin(model.GetDrawableBlendModeType(index));
    CubismShaderSet* shaderSet = _shaderSets[shaderNameBegin + offset];
    csmBool isBlendMode = false;
    GLuint blendTexture = 0;

    switch (shaderNameBegin)
    {
    default:
        // 5.3以降
        isBlendMode = true;
        SRC_COLOR = GL_ONE;
        DST_COLOR = GL_ZERO;
        SRC_ALPHA = GL_ONE;
        DST_ALPHA = GL_ZERO;
        // 以前のオフスクリーンのテクスチャを取得
        // HACK: ES でCopy用の ShaderProgram に切り替わるのでここで処理を行う。
        blendTexture = renderer->GetCurrentOffscreen() != NULL ?
            renderer->CopyRenderTarget(*renderer->GetCurrentOffscreen()->GetRenderTarget())->GetColorBuffer() :
            renderer->CopyOffscreenRenderTarget()->GetColorBuffer();
        break;
    case ShaderNames_Normal:
        // 5.2以前
        SRC_COLOR = GL_ONE;
        DST_COLOR = GL_ONE_MINUS_SRC_ALPHA;
        SRC_ALPHA = GL_ONE;
        DST_ALPHA = GL_ONE_MINUS_SRC_ALPHA;
        break;
    case ShaderNames_Add:
        // 5.2以前
        SRC_COLOR = GL_ONE;
        DST_COLOR = GL_ONE;
        SRC_ALPHA = GL_ZERO;
        DST_ALPHA = GL_ONE;
        break;
    case ShaderNames_Mult:
        // 5.2以前
        SRC_COLOR = GL_DST_COLOR;
        DST_COLOR = GL_ONE_MINUS_SRC_ALPHA;
        SRC_ALPHA = GL_ZERO;
        DST_ALPHA = GL_ONE;
        break;
    }

    glUseProgram(shaderSet->ShaderProgram);

    //テクスチャ設定
    SetupTexture(renderer, model, index, shaderSet);

    // 頂点属性設定
    SetVertexAttributes(model, index, shaderSet);

    if (masked)
    {
        glActiveTexture(GL_TEXTURE1);

        // frameBufferに書かれたテクスチャ
        GLuint tex = renderer->GetDrawableMaskBuffer(renderer->GetClippingContextBufferForDrawable()->_bufferIndex)->GetColorBuffer();

        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(shaderSet->SamplerTexture1Location, 1);

        // View座標をClippingContextの座標に変換するための行列を設定
        glUniformMatrix4fv(shaderSet->UniformClipMatrixLocation, 1, 0, renderer->GetClippingContextBufferForDrawable()->_matrixForDraw.GetArray());

        // 使用するカラーチャンネルを設定
        SetColorChannelUniformVariables(shaderSet, renderer->GetClippingContextBufferForDrawable());
    }

    // ブレンド設定
    if (isBlendMode)
    {
        glActiveTexture(GL_TEXTURE2);

        glBindTexture(GL_TEXTURE_2D, blendTexture);
        glUniform1i(shaderSet->SamplerBlendTextureLocation, 2);
    }

    //座標変換
    glUniformMatrix4fv(shaderSet->UniformMatrixLocation, 1, 0, renderer->GetMvpMatrix().GetArray()); //

    // ユニフォーム変数設定
    CubismRenderer::CubismTextureColor baseColor;
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
        baseColor = renderer->GetModelColorWithOpacity(model.GetDrawableOpacity(index));
    }
    CubismRenderer::CubismTextureColor multiplyColor = model.GetMultiplyColor(index);
    CubismRenderer::CubismTextureColor screenColor = model.GetScreenColor(index);
    SetColorUniformVariables(renderer, model, index, shaderSet, baseColor, multiplyColor, screenColor);

    glBlendFuncSeparate(SRC_COLOR, DST_COLOR, SRC_ALPHA, DST_ALPHA);
}

void CubismShader_OpenGLES2::SetupShaderProgramForMask(CubismRenderer_OpenGLES2* renderer, const CubismModel& model, const csmInt32 index)
{
    if (_shaderSets.GetSize() == 0)
    {
        GenerateShaders();
    }

    // Blending
    csmInt32 SRC_COLOR = GL_ZERO;
    csmInt32 DST_COLOR = GL_ONE_MINUS_SRC_COLOR;
    csmInt32 SRC_ALPHA = GL_ZERO;
    csmInt32 DST_ALPHA = GL_ONE_MINUS_SRC_ALPHA;

    CubismShaderSet* shaderSet = _shaderSets[ShaderNames_SetupMask];
    glUseProgram(shaderSet->ShaderProgram);

    //テクスチャ設定
    SetupTexture(renderer, model, index, shaderSet);

    // 頂点属性設定
    SetVertexAttributes(model, index, shaderSet);

    // 使用するカラーチャンネルを設定
    SetColorChannelUniformVariables(shaderSet, renderer->GetClippingContextBufferForMask());

    glUniformMatrix4fv(shaderSet->UniformClipMatrixLocation, 1, GL_FALSE, renderer->GetClippingContextBufferForMask()->_matrixForMask.GetArray());

    // ユニフォーム変数設定
    csmRectF* rect = renderer->GetClippingContextBufferForMask()->_layoutBounds;
    CubismRenderer::CubismTextureColor baseColor = {rect->X * 2.0f - 1.0f, rect->Y * 2.0f - 1.0f, rect->GetRight() * 2.0f - 1.0f, rect->GetBottom() * 2.0f - 1.0f};
    CubismRenderer::CubismTextureColor multiplyColor = model.GetMultiplyColor(index);
    CubismRenderer::CubismTextureColor screenColor = model.GetScreenColor(index);
    SetColorUniformVariables(renderer, model, index, shaderSet, baseColor, multiplyColor, screenColor);

    glBlendFuncSeparate(SRC_COLOR, DST_COLOR, SRC_ALPHA, DST_ALPHA);
}

void CubismShader_OpenGLES2::SetupShaderProgramForOffscreenRenderTarget(CubismRenderer_OpenGLES2* renderer)
{
    GLuint texture = renderer->CopyOffscreenRenderTarget()->GetColorBuffer();
    // この時点のテクスチャはPMAになっているはずなので計算を行う
    CubismRenderer::CubismTextureColor baseColor = renderer->GetModelColor();
    baseColor.R *= baseColor.A;
    baseColor.G *= baseColor.A;
    baseColor.B *= baseColor.A;
    CopyTexture(texture, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, baseColor);
}

void CubismShader_OpenGLES2::CopyTexture(GLint texture, csmInt32 srcColor, csmInt32 dstColor, csmInt32 srcAlpha, csmInt32 dstAlpha, CubismRenderer::CubismTextureColor baseColor)
{
    if (_shaderSets.GetSize() == 0)
    {
        GenerateShaders();
    }

    CubismShaderSet* shaderSet = _shaderSets[ShaderNames_Copy];
    glUseProgram(shaderSet->ShaderProgram);

    // オフスクリーンの内容を設定
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(shaderSet->SamplerTexture0Location, 0);

    // 頂点位置属性の設定
    glEnableVertexAttribArray(shaderSet->AttributePositionLocation);
    glVertexAttribPointer(shaderSet->AttributePositionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(csmFloat32) * 2, renderTargetVertexArray);

    // テクスチャ座標属性の設定
    glEnableVertexAttribArray(shaderSet->AttributeTexCoordLocation);
    glVertexAttribPointer(shaderSet->AttributeTexCoordLocation, 2, GL_FLOAT, GL_FALSE, sizeof(csmFloat32) * 2, renderTargetUvArray);

    // ベースカラーの設定
    glUniform4f(shaderSet->UniformBaseColorLocation, baseColor.R, baseColor.G, baseColor.B, baseColor.A);

    glBlendFuncSeparate(srcColor, dstColor, srcAlpha, dstAlpha);
}

void CubismShader_OpenGLES2::SetupShaderProgramForOffscreen(CubismRenderer_OpenGLES2* renderer, const CubismModel& model, const CubismOffscreenRenderTarget_OpenGLES2* offscreen)
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

    csmInt32 offscreenIndex = offscreen->GetOffscreenIndex();
    // _shaderSets用のオフセット計算
    const csmBool masked = renderer->GetClippingContextBufferForOffscreen() != NULL;  // この描画オブジェクトはマスク対象か
    const csmBool invertedMask = model.GetOffscreenInvertedMask(offscreenIndex);
    // オフスクリーンはPremultipliedAlpha を利用する
    const csmInt32 usePremultipliedAlphaOffset = 3;
    const csmInt32 offset = (masked ? (invertedMask ? 2 : 1) : 0) + usePremultipliedAlphaOffset;

    // シェーダーセット
    const csmInt32 shaderNameBegin = GetShaderNamesBegin(model.GetOffscreenBlendModeType(offscreenIndex));
    CubismShaderSet* shaderSet = _shaderSets[shaderNameBegin + offset];
    csmBool isBlendMode = false;
    GLuint blendTexture = 0;

    switch (shaderNameBegin)
    {
    default:
        // 5.3以降
        isBlendMode = true;
        SRC_COLOR = GL_ONE;
        DST_COLOR = GL_ZERO;
        SRC_ALPHA = GL_ONE;
        DST_ALPHA = GL_ZERO;
        // 以前のオフスクリーンのテクスチャを取得
        // HACK: ES でCopy用の ShaderProgram に切り替わるのでここで処理を行う。
        blendTexture = offscreen->GetOldOffscreen() != NULL ?
            renderer->CopyRenderTarget(*offscreen->GetOldOffscreen()->GetRenderTarget())->GetColorBuffer() :
            renderer->CopyOffscreenRenderTarget()->GetColorBuffer();
        break;
    case ShaderNames_Normal:
        // 5.2以前
        SRC_COLOR = GL_ONE;
        DST_COLOR = GL_ONE_MINUS_SRC_ALPHA;
        SRC_ALPHA = GL_ONE;
        DST_ALPHA = GL_ONE_MINUS_SRC_ALPHA;
        break;
    case ShaderNames_Add:
        SRC_COLOR = GL_ONE;
        DST_COLOR = GL_ONE;
        SRC_ALPHA = GL_ZERO;
        DST_ALPHA = GL_ONE;
        break;
    case ShaderNames_Mult:
        SRC_COLOR = GL_DST_COLOR;
        DST_COLOR = GL_ONE_MINUS_SRC_ALPHA;
        SRC_ALPHA = GL_ZERO;
        DST_ALPHA = GL_ONE;
        break;
    }

    glUseProgram(shaderSet->ShaderProgram);

    // オフスクリーンのテクスチャ設定
    glActiveTexture(GL_TEXTURE0);
    GLuint tex = offscreen->GetRenderTarget()->GetColorBuffer();
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(shaderSet->SamplerTexture0Location, 0);

    // 頂点位置属性の設定
    glEnableVertexAttribArray(shaderSet->AttributePositionLocation);
    glVertexAttribPointer(shaderSet->AttributePositionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(csmFloat32) * 2, renderTargetVertexArray);

    // テクスチャ座標属性の設定
    glEnableVertexAttribArray(shaderSet->AttributeTexCoordLocation);
    glVertexAttribPointer(shaderSet->AttributeTexCoordLocation, 2, GL_FLOAT, GL_FALSE, sizeof(csmFloat32) * 2, renderTargetReverseUvArray);

    if (masked)
    {
        glActiveTexture(GL_TEXTURE1);

        // frameBufferに書かれたテクスチャ
        GLuint tex = renderer->GetOffscreenMaskBuffer(renderer->GetClippingContextBufferForOffscreen()->_bufferIndex)->GetColorBuffer();
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(shaderSet->SamplerTexture1Location, 1);

        // View座標をClippingContextの座標に変換するための行列を設定
        glUniformMatrix4fv(shaderSet->UniformClipMatrixLocation, 1, 0, renderer->GetClippingContextBufferForOffscreen()->_matrixForDraw.GetArray());

        // 使用するカラーチャンネルを設定
        SetColorChannelUniformVariables(shaderSet, renderer->GetClippingContextBufferForOffscreen());
    }

    // ブレンド設定
    if (isBlendMode)
    {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, blendTexture);
        glUniform1i(shaderSet->SamplerBlendTextureLocation, 2);
    }

    //座標変換
    CubismMatrix44 mvpMatrix;
    mvpMatrix.LoadIdentity();
    glUniformMatrix4fv(shaderSet->UniformMatrixLocation, 1, 0, mvpMatrix.GetArray());

    // ユニフォーム変数設定
    csmFloat32 offscreenOpacity = model.GetOffscreenOpacity(offscreenIndex);
    // PMAなのと不透明度だけを変更したいためすべてOpacityで初期化
    CubismRenderer::CubismTextureColor baseColor(offscreenOpacity, offscreenOpacity, offscreenOpacity, offscreenOpacity);
    CubismRenderer::CubismTextureColor multiplyColor = model.GetMultiplyColorOffscreen(offscreenIndex);
    CubismRenderer::CubismTextureColor screenColor = model.GetScreenColorOffscreen(offscreenIndex);
    SetColorUniformVariables(renderer, model, offscreenIndex, shaderSet, baseColor, multiplyColor, screenColor);

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

GLuint CubismShader_OpenGLES2::LoadShaderProgramFromFile(const csmChar* vertShaderPath, const csmChar* fragShaderPath, const csmInt32 colorBlendMode, const csmInt32 alphaBlendMode)
{
    csmLoadFileFunction fileLoader = Csm::CubismFramework::GetLoadFileFunction();
    csmReleaseBytesFunction bytesReleaser = Csm::CubismFramework::GetReleaseBytesFunction();

    if (!fileLoader)
    {
        CubismLogError("File loader is not set.");
        return 0;
    }

    if (!bytesReleaser)
    {
        CubismLogError("Byte releaser is not set.");
        return 0;
    }

    // ファイルからシェーダーのソースコードを読み込み
    csmSizeInt vertSrcSize, fragSrcSize, colorBlendSrcSize, alphaBlendSrcSize;

    csmByte* vertSrc = fileLoader(vertShaderPath, &vertSrcSize);
    if (vertSrc == NULL)
    {
        CubismLogError("Failed to load vertex shader");
        return 0;
    }

    csmByte* fragSrc = fileLoader(fragShaderPath, &fragSrcSize);
    if (fragSrc == NULL)
    {
        CubismLogError("Failed to load fragment shader");
        return 0;
    }

    // 読み込んだソースコードを文字列として扱うために、終端文字列を付加
    csmString vertString = csmString(reinterpret_cast<const csmChar*>(vertSrc), vertSrcSize);
    csmString fragString = csmString(reinterpret_cast<const csmChar*>(fragSrc), fragSrcSize);

    // ブレンドモードの記述の必要があれば追記
    if (ColorBlendMode_None != colorBlendMode)
    {
        csmChar buffer[64];
        csmByte* colorBlendSrc = fileLoader(ColorBlendShaderPath, &colorBlendSrcSize);
        csmByte* alphaBlendSrc = fileLoader(AlphaBlendShaderPath, &alphaBlendSrcSize);

        // ブレンド
        std::snprintf(buffer, sizeof(buffer), "\n#define CSM_COLOR_BLEND_MODE %d\n", colorBlendMode);
        fragString += buffer;
        fragString += csmString(reinterpret_cast<const csmChar*>(colorBlendSrc), colorBlendSrcSize);

        // オーバーラップ
        if (AlphaBlendMode_None != alphaBlendMode)
        {
            std::snprintf(buffer, sizeof(buffer), "\n#define CSM_ALPHA_BLEND_MODE %d\n", alphaBlendMode);
            fragString += buffer;
        }
        else
        {
            fragString += "\n#define CSM_ALPHA_BLEND_MODE 0\n";

        }
        fragString += csmString(reinterpret_cast<const csmChar*>(alphaBlendSrc), alphaBlendSrcSize);

        bytesReleaser(colorBlendSrc);
        bytesReleaser(alphaBlendSrc);
    }

    // ファイル読み込みで確保したバイト列を開放
    bytesReleaser(vertSrc);
    bytesReleaser(fragSrc);

    // シェーダーオブジェクトを作成
    return LoadShaderProgram(vertString.GetRawString(), fragString.GetRawString());
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

void CubismShader_OpenGLES2::SetVertexAttributes(const CubismModel& model, const csmInt32 index, CubismShaderSet* shaderSet)
{
    // 頂点位置属性の設定
    const csmFloat32* vertexArray = model.GetDrawableVertices(index);
    glEnableVertexAttribArray(shaderSet->AttributePositionLocation);
    glVertexAttribPointer(shaderSet->AttributePositionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(csmFloat32) * 2, vertexArray);

    // テクスチャ座標属性の設定
    const csmFloat32* uvArray = reinterpret_cast<const csmFloat32*>(model.GetDrawableVertexUvs(index));
    glEnableVertexAttribArray(shaderSet->AttributeTexCoordLocation);
    glVertexAttribPointer(shaderSet->AttributeTexCoordLocation, 2, GL_FLOAT, GL_FALSE, sizeof(csmFloat32) * 2, uvArray);
}

void CubismShader_OpenGLES2::SetupTexture(CubismRenderer_OpenGLES2* renderer, const CubismModel& model, const csmInt32 index, CubismShaderSet* shaderSet)
{
    const csmInt32 textureIndex = model.GetDrawableTextureIndex(index);
    const GLuint textureId = renderer->GetBindedTextureId(textureIndex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glUniform1i(shaderSet->SamplerTexture0Location, 0);
}

void CubismShader_OpenGLES2::SetColorUniformVariables(CubismRenderer_OpenGLES2* renderer, const CubismModel& model, const csmInt32 index, CubismShaderSet* shaderSet,
                                                      CubismRenderer::CubismTextureColor& baseColor, CubismRenderer::CubismTextureColor& multiplyColor, CubismRenderer::CubismTextureColor& screenColor)
{
    glUniform4f(shaderSet->UniformBaseColorLocation, baseColor.R, baseColor.G, baseColor.B, baseColor.A);
    glUniform4f(shaderSet->UniformMultiplyColorLocation, multiplyColor.R, multiplyColor.G, multiplyColor.B, multiplyColor.A);
    glUniform4f(shaderSet->UniformScreenColorLocation, screenColor.R, screenColor.G, screenColor.B, screenColor.A);
}

void CubismShader_OpenGLES2::SetColorChannelUniformVariables(CubismShaderSet* shaderSet, CubismClippingContext_OpenGLES2* contextBuffer)
{
    const csmInt32 channelIndex = contextBuffer->_layoutChannelIndex;
    CubismRenderer::CubismTextureColor* colorChannel = contextBuffer->GetClippingManager()->GetChannelFlagAsColor(channelIndex);
    glUniform4f(shaderSet->UnifromChannelFlagLocation, colorChannel->R, colorChannel->G, colorChannel->B, colorChannel->A);
}

}}}}

//------------ LIVE2D NAMESPACE ------------
