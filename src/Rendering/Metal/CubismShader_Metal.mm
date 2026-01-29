/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismShader_Metal.hpp"
#include "CubismRenderer_Metal.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/*********************************************************************************************************************
*                                       CubismShader_Metal
********************************************************************************************************************/
csmInt32 CubismShader_Metal::GetShaderNamesBegin(const csmBlendMode blendMode)
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


//// SetupMask
static const csmChar* VertShaderSrcSetupMask = "VertShaderSrcSetupMask";

static const csmChar* FragShaderSrcSetupMask = "FragShaderSrcSetupMask";

//// Copy
static const csmChar* VertShaderSrcCopy = "VertShaderSrcCopy";

static const csmChar* FragShaderSrcCopy= "FragShaderSrcCopy";

//----- バーテックスシェーダプログラム -----
// Normal & Add & Mult 共通
static const csmChar* VertShaderSrc = "VertShaderSrc";

// Normal & Add & Mult 共通（クリッピングされたものの描画用）
static const csmChar* VertShaderSrcMasked = "VertShaderSrcMasked";

//----- フラグメントシェーダプログラム -----
// Normal & Add & Mult 共通
static const csmChar* FragShaderSrc = "FragShaderSrc";

// Normal & Add & Mult 共通 （PremultipliedAlpha）
static const csmChar* FragShaderSrcPremultipliedAlpha = "FragShaderSrcPremultipliedAlpha";

// Normal & Add & Mult 共通（クリッピングされたものの描画用）
static const csmChar* FragShaderSrcMask = "FragShaderSrcMask";

// Normal & Add & Mult 共通（クリッピングされて反転使用の描画用）
static const csmChar* FragShaderSrcMaskInverted = "FragShaderSrcMaskInverted";

// Normal & Add & Mult 共通（クリッピングされたものの描画用、PremultipliedAlphaの場合）
static const csmChar* FragShaderSrcMaskPremultipliedAlpha = "FragShaderSrcMaskPremultipliedAlpha";

// Normal & Add & Mult 共通（クリッピングされて反転使用の描画用、PremultipliedAlphaの場合）
static const csmChar* FragShaderSrcMaskInvertedPremultipliedAlpha = "FragShaderSrcMaskInvertedPremultipliedAlpha";

// シェーダーライブラリのバンドル先パス
static const csmChar* ShaderLibsDir = "FrameworkMetallibs";

CubismShader_Metal::CubismShader_Metal()
{
}

CubismShader_Metal::~CubismShader_Metal()
{
}

void CubismShader_Metal::GenerateShaders(CubismRenderer_Metal* renderer)
{
    for (csmInt32 i = 0; i < ShaderNames_ShaderCount; ++i)
    {
        _shaderSets.PushBack(CSM_NEW CubismShaderSet());
    }

    //シェーダライブラリのロード（.metal）
    id <MTLDevice> device = renderer->GetDevice();

    NSString *subDir = [NSString stringWithCString:ShaderLibsDir encoding:NSUTF8StringEncoding];
    NSURL *libraryURL = [[NSBundle mainBundle] URLForResource:@"MetalShaders" withExtension:@"metallib" subdirectory:subDir];
    id<MTLLibrary> shaderLib = [device newLibraryWithURL:libraryURL error:nil];
    if(!shaderLib)
    {
        NSLog(@" ERROR: Couldnt create a default shader library");
        // assert here because if the shader libary isn't loading, nothing good will happen
        return;
    }

    _shaderSets[ShaderNames_SetupMask]->ShaderProgram = LoadShaderProgram(VertShaderSrcSetupMask, FragShaderSrcSetupMask, shaderLib);
    _shaderSets[ShaderNames_Copy]->ShaderProgram = LoadShaderProgram(VertShaderSrcCopy, FragShaderSrcCopy, shaderLib);

    // 通常
    _shaderSets[ShaderNames_Normal]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrc, shaderLib);
    _shaderSets[ShaderNames_NormalMasked]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcMask, shaderLib);
    _shaderSets[ShaderNames_NormalMaskedInverted]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcMaskInverted, shaderLib);
    _shaderSets[ShaderNames_NormalPremultipliedAlpha]->ShaderProgram = LoadShaderProgram(VertShaderSrc, FragShaderSrcPremultipliedAlpha, shaderLib);
    _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcMaskPremultipliedAlpha, shaderLib);
    _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram = LoadShaderProgram(VertShaderSrcMasked, FragShaderSrcMaskInvertedPremultipliedAlpha, shaderLib);

    _shaderSets[ShaderNames_SetupMask]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_SetupMask]->ShaderProgram, CubismRenderer::CubismBlendMode_Mask);
    _shaderSets[ShaderNames_SetupMask]->SamplerState = MakeSamplerState(device, renderer);

    _shaderSets[ShaderNames_Copy]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_Copy]->ShaderProgram, CubismRenderer::CubismBlendMode_Normal);
    _shaderSets[ShaderNames_Copy]->SamplerState = MakeSamplerState(device, renderer);
    _shaderSets[ShaderNames_Copy]->DepthStencilState = MakeDepthStencilState(device);

    _shaderSets[ShaderNames_Normal]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_Normal]->ShaderProgram, CubismRenderer::CubismBlendMode_Normal);
    _shaderSets[ShaderNames_NormalMasked]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_NormalMasked]->ShaderProgram, CubismRenderer::CubismBlendMode_Normal);
    _shaderSets[ShaderNames_NormalMaskedInverted]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_NormalMaskedInverted]->ShaderProgram, CubismRenderer::CubismBlendMode_Normal);
    _shaderSets[ShaderNames_NormalPremultipliedAlpha]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_NormalPremultipliedAlpha]->ShaderProgram, CubismRenderer::CubismBlendMode_Normal);
    _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->ShaderProgram, CubismRenderer::CubismBlendMode_Normal);
    _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram, CubismRenderer::CubismBlendMode_Normal);

    _shaderSets[ShaderNames_Normal]->DepthStencilState = MakeDepthStencilState(device);
    _shaderSets[ShaderNames_NormalMasked]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_NormalMaskedInverted]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_NormalPremultipliedAlpha]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;

    _shaderSets[ShaderNames_Normal]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_NormalMasked]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_NormalMaskedInverted]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_NormalPremultipliedAlpha]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;

    // 加算も通常と同じシェーダーを利用する
    _shaderSets[ShaderNames_Add]->ShaderProgram = _shaderSets[ShaderNames_Normal]->ShaderProgram;
    _shaderSets[ShaderNames_AddMasked]->ShaderProgram = _shaderSets[ShaderNames_NormalMasked]->ShaderProgram;
    _shaderSets[ShaderNames_AddMaskedInverted]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInverted]->ShaderProgram;
    _shaderSets[ShaderNames_AddPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_AddMaskedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_AddMaskedInvertedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram;

    _shaderSets[ShaderNames_Add]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_Add]->ShaderProgram, CubismRenderer::CubismBlendMode_Additive);
    _shaderSets[ShaderNames_AddMasked]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_AddMasked]->ShaderProgram, CubismRenderer::CubismBlendMode_Additive);
    _shaderSets[ShaderNames_AddMaskedInverted]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_AddMaskedInverted]->ShaderProgram, CubismRenderer::CubismBlendMode_Additive);
    _shaderSets[ShaderNames_AddPremultipliedAlpha]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_AddPremultipliedAlpha]->ShaderProgram, CubismRenderer::CubismBlendMode_Additive);
    _shaderSets[ShaderNames_AddMaskedPremultipliedAlpha]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_AddMaskedPremultipliedAlpha]->ShaderProgram, CubismRenderer::CubismBlendMode_Additive);
    _shaderSets[ShaderNames_AddMaskedInvertedPremultipliedAlpha]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_AddMaskedInvertedPremultipliedAlpha]->ShaderProgram, CubismRenderer::CubismBlendMode_Additive);

    _shaderSets[ShaderNames_Add]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_AddMasked]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_AddMaskedInverted]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_AddPremultipliedAlpha]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_AddMaskedPremultipliedAlpha]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_AddMaskedInvertedPremultipliedAlpha]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;

    _shaderSets[ShaderNames_Add]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_AddMasked]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_AddMaskedInverted]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_AddPremultipliedAlpha]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_AddMaskedPremultipliedAlpha]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_AddMaskedInvertedPremultipliedAlpha]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;

    // 乗算も通常と同じシェーダーを利用する
    _shaderSets[ShaderNames_Mult]->ShaderProgram = _shaderSets[ShaderNames_Normal]->ShaderProgram;
    _shaderSets[ShaderNames_MultMasked]->ShaderProgram = _shaderSets[ShaderNames_NormalMasked]->ShaderProgram;
    _shaderSets[ShaderNames_MultMaskedInverted]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInverted]->ShaderProgram;
    _shaderSets[ShaderNames_MultPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_MultMaskedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedPremultipliedAlpha]->ShaderProgram;
    _shaderSets[ShaderNames_MultMaskedInvertedPremultipliedAlpha]->ShaderProgram = _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram;

    _shaderSets[ShaderNames_Mult]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_Mult]->ShaderProgram, CubismRenderer::CubismBlendMode_Multiplicative);
    _shaderSets[ShaderNames_MultMasked]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_MultMasked]->ShaderProgram, CubismRenderer::CubismBlendMode_Multiplicative);
    _shaderSets[ShaderNames_MultMaskedInverted]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_MultMaskedInverted]->ShaderProgram, CubismRenderer::CubismBlendMode_Multiplicative);
    _shaderSets[ShaderNames_MultPremultipliedAlpha]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_MultPremultipliedAlpha]->ShaderProgram, CubismRenderer::CubismBlendMode_Multiplicative);
    _shaderSets[ShaderNames_MultMaskedPremultipliedAlpha]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_MultMaskedPremultipliedAlpha]->ShaderProgram, CubismRenderer::CubismBlendMode_Multiplicative);
    _shaderSets[ShaderNames_MultMaskedInvertedPremultipliedAlpha]->RenderPipelineState = MakeRenderPipelineState(device, _shaderSets[ShaderNames_NormalMaskedInvertedPremultipliedAlpha]->ShaderProgram, CubismRenderer::CubismBlendMode_Multiplicative);

    _shaderSets[ShaderNames_Mult]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_MultMasked]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_MultMaskedInverted]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_MultPremultipliedAlpha]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_MultMaskedPremultipliedAlpha]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;
    _shaderSets[ShaderNames_MultMaskedInvertedPremultipliedAlpha]->DepthStencilState = _shaderSets[ShaderNames_Normal]->DepthStencilState;

    _shaderSets[ShaderNames_Mult]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_MultMasked]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_MultMaskedInverted]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_MultPremultipliedAlpha]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_MultMaskedPremultipliedAlpha]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;
    _shaderSets[ShaderNames_MultMaskedInvertedPremultipliedAlpha]->SamplerState = _shaderSets[ShaderNames_SetupMask]->SamplerState;

    // 5.3以降
    // ブレンドモードの組み合わせ分作成
    {
        csmInt32 offset = ShaderNames_NormalAtop;
        for(csmInt32 i = 0; i <= Core::csmColorBlendType_Color; i++)
        {
            if(i == Core::csmColorBlendType_AddCompatible || i == Core::csmColorBlendType_MultiplyCompatible)
            {
                continue;
            }

            // Normal Overはシェーダを作る必要がないため 1 から始める
            const csmInt32 start = (i == 0 ? 1 : 0);

            for (csmInt32 j = start; j <= Core::csmAlphaBlendType_DisjointOver; j++)
            {
                csmString colorBlendModeName = csmBlendMode::ColorBlendModeToString(i);
                csmString alphaBlendModeName = csmBlendMode::AlphaBlendModeToString(j);

                csmString baseFragShaderFileName = csmString("FragShaderSrcBlend");

                csmString vertShaderFileName = csmString("VertShaderSrcBlend");
                csmString fragShaderFileName = baseFragShaderFileName + colorBlendModeName + alphaBlendModeName;
                GenerateBlendShader(_shaderSets[offset++],
                                    vertShaderFileName,
                                    fragShaderFileName,
                                    vertShaderFileName,
                                    baseFragShaderFileName,
                                    device,
                                    renderer);

                vertShaderFileName = csmString("VertShaderSrcMaskedBlend");
                baseFragShaderFileName = csmString("FragShaderSrcMaskBlend");
                fragShaderFileName = baseFragShaderFileName + colorBlendModeName + alphaBlendModeName;
                GenerateBlendShader(_shaderSets[offset++],
                                    vertShaderFileName,
                                    fragShaderFileName,
                                    vertShaderFileName,
                                    baseFragShaderFileName,
                                    device,
                                    renderer);

                vertShaderFileName = csmString("VertShaderSrcMaskedBlend");
                baseFragShaderFileName = csmString("FragShaderSrcMaskInvertedBlend");
                fragShaderFileName = baseFragShaderFileName + colorBlendModeName + alphaBlendModeName;
                GenerateBlendShader(_shaderSets[offset++],
                                    vertShaderFileName,
                                    fragShaderFileName,
                                    vertShaderFileName,
                                    baseFragShaderFileName,
                                    device,
                                    renderer);

                vertShaderFileName = csmString("VertShaderSrcBlend");
                baseFragShaderFileName = csmString("FragShaderSrcPremultipliedAlphaBlend");
                fragShaderFileName = baseFragShaderFileName + colorBlendModeName + alphaBlendModeName;
                GenerateBlendShader(_shaderSets[offset++],
                                    vertShaderFileName,
                                    fragShaderFileName,
                                    vertShaderFileName,
                                    baseFragShaderFileName,
                                    device,
                                    renderer);

                vertShaderFileName = csmString("VertShaderSrcMaskedBlend");
                baseFragShaderFileName = csmString("FragShaderSrcMaskPremultipliedAlphaBlend");
                fragShaderFileName = baseFragShaderFileName + colorBlendModeName + alphaBlendModeName;
                GenerateBlendShader(_shaderSets[offset++],
                                    vertShaderFileName,
                                    fragShaderFileName,
                                    vertShaderFileName,
                                    baseFragShaderFileName,
                                    device,
                                    renderer);

                vertShaderFileName = csmString("VertShaderSrcMaskedBlend");
                baseFragShaderFileName = csmString("FragShaderSrcMaskInvertedPremultipliedAlphaBlend");
                fragShaderFileName = baseFragShaderFileName + colorBlendModeName + alphaBlendModeName;
                GenerateBlendShader(_shaderSets[offset++],
                                    vertShaderFileName,
                                    fragShaderFileName,
                                    vertShaderFileName,
                                    baseFragShaderFileName,
                                    device,
                                    renderer);
            }
        }
    }
}

void CubismShader_Metal::GenerateBlendShader(CubismShaderSet* shaderSet, const csmString& vertShaderFileName, const csmString& fragShaderFileName, const csmString& vertShaderSrc, const csmString& fragShaderSrc, id<MTLDevice> device, CubismRenderer_Metal* renderer)
{
    shaderSet->ShaderProgram = LoadShaderProgramFromFile(vertShaderFileName,
                                                         fragShaderFileName,
                                                         vertShaderFileName,
                                                         fragShaderSrc,
                                                         device);
    shaderSet->RenderPipelineState = MakeRenderPipelineState(device, shaderSet->ShaderProgram, 10);

    shaderSet->SamplerState = MakeSamplerState(device, renderer);
    shaderSet->DepthStencilState = MakeDepthStencilState(device);
}

simd::float4x4 CubismShader_Metal::ConvertCubismMatrix44IntoSimdFloat4x4(CubismMatrix44& matrix)
{
    csmFloat32* srcArray = matrix.GetArray();
    return simd::float4x4(simd::float4 {srcArray[0], srcArray[1], srcArray[2], srcArray[3]},
                          simd::float4 {srcArray[4], srcArray[5], srcArray[6], srcArray[7]},
                          simd::float4 {srcArray[8], srcArray[9], srcArray[10], srcArray[11]},
                          simd::float4 {srcArray[12], srcArray[13], srcArray[14], srcArray[15]});
}

void CubismShader_Metal::SetColorChannel(CubismShaderUniforms& shaderUniforms, CubismClippingContext_Metal* contextBuffer)
{
    const csmInt32 channelIndex = contextBuffer->_layoutChannelIndex;
    CubismRenderer::CubismTextureColor* colorChannel = contextBuffer->GetClippingManager()->GetChannelFlagAsColor(channelIndex);
    shaderUniforms.channelFlag = (vector_float4){ colorChannel->R, colorChannel->G, colorChannel->B, colorChannel->A };
}

void CubismShader_Metal::SetFragmentModelTexture(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                                          , CubismRenderer_Metal* renderer, const CubismModel& model, const csmInt32 index)
{
    const id <MTLTexture> texture = renderer->GetBindedTextureId(model.GetDrawableTextureIndex(index));
    [renderEncoder setFragmentTexture:texture atIndex:0];
}

void CubismShader_Metal::SetVertexBufferForVerticesAndUvs(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder)
{
    [renderEncoder setVertexBuffer:(drawCommandBuffer->GetVertexBuffer()) offset:0 atIndex:MetalVertexInputIndexVertices];
    [renderEncoder setVertexBuffer:(drawCommandBuffer->GetUvBuffer()) offset:0 atIndex:MetalVertexInputUVs];
}

void CubismShader_Metal::SetupShaderProgramForDrawable(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                                                  , CubismRenderer_Metal* renderer, const CubismModel& model, const csmInt32 index, id<MTLTexture> blendTexture)
{
    // シェーダー生成
    if (_shaderSets.GetSize() == 0)
    {
        GenerateShaders(renderer);
    }

    // シェーダーセットの設定
    const csmBool masked = renderer->GetClippingContextBufferForDrawable() != NULL;
    const csmBool invertedMask = model.GetDrawableInvertedMask(index);
    const csmBool isPremultipliedAlpha = renderer->IsPremultipliedAlpha();
    const csmInt32 offset = (masked ? ( invertedMask ? 2 : 1 ) : 0) + (isPremultipliedAlpha ? 3 : 0);

    // シェーダーセット
    const csmInt32 shaderNameBegin = GetShaderNamesBegin(model.GetDrawableBlendModeType(index));
    CubismShaderSet* shaderSet = _shaderSets[shaderNameBegin + offset];
    csmBool isBlendMode = false;

    //_shaderSetsにshaderが入っていて、それをブレンドモードごとに切り替えている
    switch (shaderNameBegin)
    {
    default:
        isBlendMode = true;
        break;
    case ShaderNames_Normal:
    case ShaderNames_Add:
    case ShaderNames_Mult:
        break;
    }

    //テクスチャ設定
    SetFragmentModelTexture(drawCommandBuffer, renderEncoder, renderer, model, index);

    // 頂点・テクスチャバッファの設定
    SetVertexBufferForVerticesAndUvs(drawCommandBuffer, renderEncoder);

    CubismShaderUniforms shaderUniforms;
    if (masked)
    {
        // frameBufferに書かれたテクスチャ
        id <MTLTexture> tex = renderer->GetDrawableMaskBuffer(renderer->GetClippingContextBufferForDrawable()->_bufferIndex)->GetColorBuffer();

        //テクスチャ設定
        [renderEncoder setFragmentTexture:tex atIndex:1];

        // 使用するカラーチャンネルを設定
        SetColorChannel(shaderUniforms, renderer->GetClippingContextBufferForDrawable());

        // クリッピング変換行列設定
        shaderUniforms.clipMatrix = ConvertCubismMatrix44IntoSimdFloat4x4(renderer->GetClippingContextBufferForDrawable()->_matrixForDraw);
    }

    if(isBlendMode)
    {
        csmInt32 texIndex = masked ? 2 : 1;
        [renderEncoder setFragmentTexture:blendTexture atIndex:texIndex];
    }

    // MVP行列設定
    CubismMatrix44 mvp = renderer->GetMvpMatrix();
    shaderUniforms.matrix = ConvertCubismMatrix44IntoSimdFloat4x4(mvp);

    // 色定数バッファの設定
    CubismRenderer::CubismTextureColor baseColor;
    if (model.IsBlendModeEnabled())
    {
        // ブレンドモードではモデルカラーを最後に処理するため不透明度のみ対応させる
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
        // ブレンドモードを使用しない場合はDrawable単位でモデルカラーを処理する
        baseColor = renderer->GetModelColorWithOpacity(model.GetDrawableOpacity(index));
    }
    CubismRenderer::CubismTextureColor multiplyColor = model.GetMultiplyColor(index);
    CubismRenderer::CubismTextureColor screenColor = model.GetScreenColor(index);

    shaderUniforms.baseColor = (vector_float4){ baseColor.R, baseColor.G, baseColor.B, baseColor.A };
    shaderUniforms.multiplyColor = (vector_float4){ multiplyColor.R, multiplyColor.G, multiplyColor.B, multiplyColor.A };
    shaderUniforms.screenColor = (vector_float4){ screenColor.R, screenColor.G, screenColor.B, screenColor.A };

    // 転送
    [renderEncoder setVertexBytes:&shaderUniforms length:sizeof(CubismShaderUniforms) atIndex:MetalVertexInputIndexUniforms];
    [renderEncoder setFragmentBytes:&shaderUniforms length:sizeof(CubismShaderUniforms) atIndex:MetalVertexInputIndexUniforms];
    [renderEncoder setFragmentSamplerState:shaderSet->SamplerState atIndex:0];
    [renderEncoder setDepthStencilState:shaderSet->DepthStencilState];

    drawCommandBuffer->GetCommandDraw()->SetRenderPipelineState(shaderSet->RenderPipelineState);
}

void CubismShader_Metal::SetupShaderProgramForMask(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                                                , CubismRenderer_Metal* renderer, const CubismModel& model, const csmInt32 index)
{
    // シェーダー生成
    if (_shaderSets.GetSize() == 0)
    {
        GenerateShaders(renderer);
    }

    // シェーダーセットの設定
    CubismShaderSet* shaderSet = _shaderSets[ShaderNames_SetupMask];

    // テクスチャ設定
    SetFragmentModelTexture(drawCommandBuffer, renderEncoder, renderer, model, index);

    // 頂点・テクスチャバッファの設定
    SetVertexBufferForVerticesAndUvs(drawCommandBuffer, renderEncoder);

    CubismShaderUniforms shaderUniforms;

    // 使用するカラーチャンネルを設定
    SetColorChannel(shaderUniforms, renderer->GetClippingContextBufferForMask());

    // クリッピング変換行列設定
    shaderUniforms.clipMatrix = ConvertCubismMatrix44IntoSimdFloat4x4(renderer->GetClippingContextBufferForMask()->_matrixForMask);

    // 色定数バッファの設定
    csmRectF* rect = renderer->GetClippingContextBufferForMask()->_layoutBounds;
    shaderUniforms.baseColor = (vector_float4){ rect->X * 2.0f - 1.0f,
                                rect->Y * 2.0f - 1.0f,
                                rect->GetRight() * 2.0f - 1.0f,
                                rect->GetBottom() * 2.0f - 1.0f };

    // 転送
    [renderEncoder setVertexBytes:&shaderUniforms length:sizeof(CubismShaderUniforms) atIndex:MetalVertexInputIndexUniforms];
    [renderEncoder setFragmentBytes:&shaderUniforms length:sizeof(CubismShaderUniforms) atIndex:MetalVertexInputIndexUniforms];
    [renderEncoder setFragmentSamplerState:shaderSet->SamplerState atIndex:0];

    drawCommandBuffer->GetCommandDraw()->SetRenderPipelineState(shaderSet->RenderPipelineState);
}

void CubismShader_Metal::SetupShaderProgramForRenderTarget(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                                , CubismRenderer_Metal* renderer)
{
    const id<MTLTexture> tex = renderer->GetModelRenderTarget(0)->GetColorBuffer();

    CubismShaderUniforms shaderUniforms;

    CubismRenderer::CubismTextureColor baseColor = renderer->GetModelColor();
    baseColor.R *= baseColor.A;
    baseColor.G *= baseColor.A;
    baseColor.B *= baseColor.A;
    shaderUniforms.baseColor = (vector_float4){ baseColor.R, baseColor.G, baseColor.B, baseColor.A };
    [renderEncoder setFragmentBytes:&shaderUniforms length:sizeof(CubismShaderUniforms) atIndex:MetalVertexInputIndexUniforms];
    CopyTexture(tex, drawCommandBuffer, renderEncoder, renderer);
}

void CubismShader_Metal::CopyTexture(id<MTLTexture> texture, CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                                , CubismRenderer_Metal* renderer)
{
    if (_shaderSets.GetSize() == 0)
    {
        GenerateShaders(renderer);
    }

    CubismShaderSet* shaderSet = _shaderSets[ShaderNames_Copy];

    //テクスチャ設定
    [renderEncoder setFragmentTexture:texture atIndex:0];

    // 頂点・テクスチャバッファの設定
    SetVertexBufferForVerticesAndUvs(drawCommandBuffer, renderEncoder);

    // 転送
    [renderEncoder setFragmentSamplerState:shaderSet->SamplerState atIndex:0];
    [renderEncoder setDepthStencilState:shaderSet->DepthStencilState];

    drawCommandBuffer->GetCommandDraw()->SetRenderPipelineState(shaderSet->RenderPipelineState);
}

void CubismShader_Metal::SetupShaderProgramForOffscreen(CubismCommandBuffer_Metal::DrawCommandBuffer* drawCommandBuffer, id <MTLRenderCommandEncoder> renderEncoder
                                , CubismRenderer_Metal* renderer, const CubismModel& model, const CubismOffscreenRenderTarget_Metal* offscreen, id<MTLTexture> blendTexture)
{
    // シェーダー生成
    if (_shaderSets.GetSize() == 0)
    {
        GenerateShaders(renderer);
    }

    csmInt32 offscreenIndex = offscreen->GetOffscreenIndex();

    // シェーダーセットの設定
    const csmBool masked = renderer->GetClippingContextBufferForOffscreen() != NULL;
    const csmBool invertedMask = model.GetOffscreenInvertedMask(offscreenIndex);
    // オフスクリーンはPremultipliedAlpha を利用する
    const csmInt32 usePremultipliedAlphaOffset = 3;
    const csmInt32 offset = (masked ? ( invertedMask ? 2 : 1 ) : 0) + usePremultipliedAlphaOffset;

    // シェーダーセット
    const csmInt32 shaderNameBegin = GetShaderNamesBegin(model.GetOffscreenBlendModeType(offscreenIndex));
    CubismShaderSet* shaderSet = _shaderSets[shaderNameBegin + offset];
    csmBool isBlendMode = false;

    //_shaderSetsにshaderが入っていて、それをブレンドモードごとに切り替えている
    switch (shaderNameBegin)
    {
    default:
        isBlendMode = true;
        break;
    case ShaderNames_Normal:
    case ShaderNames_Add:
    case ShaderNames_Mult:
        break;
    }

    //テクスチャ設定
    [renderEncoder setFragmentTexture:offscreen->GetRenderTarget()->GetColorBuffer() atIndex:0];

    // 頂点・テクスチャバッファの設定は呼び出し元(DrawOffscreenMetal)で行う

    CubismShaderUniforms shaderUniforms;
    if (masked)
    {
        // frameBufferに書かれたテクスチャ
        id <MTLTexture> tex = renderer->GetOffscreenMaskBuffer(renderer->GetClippingContextBufferForOffscreen()->_bufferIndex)->GetColorBuffer();

        //テクスチャ設定
        [renderEncoder setFragmentTexture:tex atIndex:1];

        // 使用するカラーチャンネルを設定
        SetColorChannel(shaderUniforms, renderer->GetClippingContextBufferForOffscreen());

        // クリッピング変換行列設定
        CubismMatrix44 mat = renderer->GetClippingContextBufferForOffscreen()->_matrixForDraw;
        shaderUniforms.clipMatrix = ConvertCubismMatrix44IntoSimdFloat4x4(mat);
    }

    if(isBlendMode)
    {
        // 親の描画結果がコピーされているテクスチャを設定
        csmInt32 texIndex = masked ? 2 : 1;
        [renderEncoder setFragmentTexture:blendTexture atIndex:texIndex];
    }

    // MVP行列設定
    CubismMatrix44 mvp;
    mvp.LoadIdentity();
    shaderUniforms.matrix = ConvertCubismMatrix44IntoSimdFloat4x4(mvp);

    // 色定数バッファの設定
    // オフスクリーンはPMA前提
    csmFloat32 offscreenOpacity = model.GetOffscreenOpacity(offscreenIndex);
    CubismRenderer::CubismTextureColor baseColor(offscreenOpacity, offscreenOpacity, offscreenOpacity, offscreenOpacity);
    CubismRenderer::CubismTextureColor multiplyColor = model.GetMultiplyColorOffscreen(offscreenIndex);
    CubismRenderer::CubismTextureColor screenColor = model.GetScreenColorOffscreen(offscreenIndex);

    shaderUniforms.baseColor = (vector_float4){ baseColor.R, baseColor.G, baseColor.B, baseColor.A };
    shaderUniforms.multiplyColor = (vector_float4){ multiplyColor.R, multiplyColor.G, multiplyColor.B, multiplyColor.A };
    shaderUniforms.screenColor = (vector_float4){ screenColor.R, screenColor.G, screenColor.B, screenColor.A };

    // 転送
    [renderEncoder setVertexBytes:&shaderUniforms length:sizeof(CubismShaderUniforms) atIndex:MetalVertexInputIndexUniforms];
    [renderEncoder setFragmentBytes:&shaderUniforms length:sizeof(CubismShaderUniforms) atIndex:MetalVertexInputIndexUniforms];
    [renderEncoder setFragmentSamplerState:shaderSet->SamplerState atIndex:0];
    [renderEncoder setDepthStencilState:shaderSet->DepthStencilState];

    drawCommandBuffer->GetCommandDraw()->SetRenderPipelineState(shaderSet->RenderPipelineState);
}

CubismShader_Metal::ShaderProgram* CubismShader_Metal::LoadShaderProgram(const csmChar* vertShaderSrc, const csmChar* fragShaderSrc, id<MTLLibrary> shaderLib)
{
    // Create shader program.
    ShaderProgram *shaderProgram = new ShaderProgram;

    //頂点シェーダの取得
    NSString *vertShaderStr = [NSString stringWithCString: vertShaderSrc encoding:NSUTF8StringEncoding];

    shaderProgram->vertexFunction = [shaderLib newFunctionWithName:vertShaderStr];
    if(!shaderProgram->vertexFunction)
    {
        delete shaderProgram;
        NSLog(@">> ERROR: Couldn't load vertex function from default library");
        return nil;
    }

    //フラグメントシェーダの取得
    NSString *fragShaderStr = [NSString stringWithCString: fragShaderSrc encoding:NSUTF8StringEncoding];
    shaderProgram->fragmentFunction = [shaderLib newFunctionWithName:fragShaderStr];
    if(!shaderProgram->fragmentFunction)
    {
        delete shaderProgram;
        NSLog(@" ERROR: Couldn't load fragment function from default library");
        return nil;
    }

    return shaderProgram;
}

CubismShader_Metal::ShaderProgram* CubismShader_Metal::LoadShaderProgram(const csmString& vertShaderSrc, const csmString& fragShaderSrc, id<MTLLibrary> vertShaderLib, id<MTLLibrary> fragShaderLib)
{
    // Create shader program.
    ShaderProgram *shaderProgram = new ShaderProgram;

    // 頂点シェーダの取得
    NSString *vertShaderStr = [NSString stringWithCString: vertShaderSrc.GetRawString() encoding:NSUTF8StringEncoding];

    shaderProgram->vertexFunction = [vertShaderLib newFunctionWithName:vertShaderStr];
    if(!shaderProgram->vertexFunction)
    {
        delete shaderProgram;
        NSLog(@">> ERROR: Couldn't load vertex function from vertex shader library");
        return nil;
    }

    // フラグメントシェーダの取得
    NSString *fragShaderStr = [NSString stringWithCString: fragShaderSrc.GetRawString() encoding:NSUTF8StringEncoding];

    shaderProgram->fragmentFunction = [fragShaderLib newFunctionWithName:fragShaderStr];
    if(!shaderProgram->fragmentFunction)
    {
        delete shaderProgram;
        NSLog(@" ERROR: Couldn't load fragment function from fragment shader library");
        return nil;
    }

    return shaderProgram;
}

CubismShader_Metal::ShaderProgram* CubismShader_Metal::LoadShaderProgramFromFile(const csmString& vertShaderFileName, const csmString& fragShaderFileName, const csmString& vertShaderSrc, const csmString& fragShaderSrc, id<MTLDevice> device)
{
    NSString *subDir = [NSString stringWithCString:ShaderLibsDir encoding:NSUTF8StringEncoding];
    NSString *vertShaderFileNameStr = [NSString stringWithCString:vertShaderFileName.GetRawString() encoding:NSUTF8StringEncoding];
    NSURL *vertShaderFileLibURL = [[NSBundle mainBundle] URLForResource:vertShaderFileNameStr withExtension:@"metallib" subdirectory:subDir];

    NSString *fragShaderFileNameStr = [NSString stringWithCString:fragShaderFileName.GetRawString() encoding:NSUTF8StringEncoding];
    NSURL *fragShaderFileLibURL = [[NSBundle mainBundle] URLForResource:fragShaderFileNameStr withExtension:@"metallib" subdirectory:subDir];

    // 頂点シェーダの取得/
    NSData *vertShaderData = [NSData dataWithContentsOfURL:vertShaderFileLibURL];
   if(vertShaderData == nil)
   {
       NSLog(@"ERROR: File load failed : %@", [vertShaderFileLibURL absoluteString]);
       return nil;
   }
   if([vertShaderData length] == 0)
   {
       NSLog(@"ERROR: File is loaded but file size is zero : %@", [vertShaderFileLibURL absoluteString]);
   }
    NSUInteger vertShaderByteLen = [vertShaderData length];
    Byte* vertShaderByteData = (Byte*)malloc(vertShaderByteLen);
    memcpy(vertShaderByteData, [vertShaderData bytes], vertShaderByteLen);
    dispatch_data_t vertShaderDispatchData = dispatch_data_create(vertShaderByteData, vertShaderByteLen, nil, DISPATCH_DATA_DESTRUCTOR_FREE);
    id<MTLLibrary> vertShaderLib = [device newLibraryWithData:vertShaderDispatchData error:nil];
    if(!vertShaderLib)
    {
        NSLog(@" ERROR: Couldnt create a vertex shader library");
        return;
    }

    // フラグメントシェーダの取得
   NSData *fragShaderData = [NSData dataWithContentsOfURL:fragShaderFileLibURL];
   if(fragShaderData == nil)
   {
       NSLog(@"ERROR: File load failed : %@", [fragShaderFileLibURL absoluteString]);
       return nil;
   }
   if([fragShaderData length] == 0)
   {
       NSLog(@"ERROR: File is loaded but file size is zero : %@", [fragShaderFileLibURL absoluteString]);
   }
    NSUInteger fragShaderByteLen = [fragShaderData length];
    Byte* fragShaderByteData = (Byte*)malloc(fragShaderByteLen);
    memcpy(fragShaderByteData, [fragShaderData bytes], fragShaderByteLen);
    dispatch_data_t fragShaderDispatchData = dispatch_data_create(fragShaderByteData, fragShaderByteLen, nil, DISPATCH_DATA_DESTRUCTOR_FREE);
    id<MTLLibrary> fragShaderLib = [device newLibraryWithData:fragShaderDispatchData error:nil];
    if(!fragShaderLib)
    {
        NSLog(@" ERROR: Couldnt create a frag shader library");
        return;
    }
  ShaderProgram* shaderProgram = LoadShaderProgram(vertShaderSrc, fragShaderSrc, vertShaderLib, fragShaderLib);
  return shaderProgram;
}

id<MTLRenderPipelineState> CubismShader_Metal::MakeRenderPipelineState(id<MTLDevice> device, CubismShader_Metal::ShaderProgram* shaderProgram, int blendMode)
{
    MTLRenderPipelineDescriptor* renderPipelineDescriptor = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
    NSError *error;

    renderPipelineDescriptor.vertexFunction = shaderProgram->vertexFunction;
    renderPipelineDescriptor.fragmentFunction = shaderProgram->fragmentFunction;
    renderPipelineDescriptor.colorAttachments[0].blendingEnabled = true;
    renderPipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    renderPipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    renderPipelineDescriptor.colorAttachments[0].writeMask = MTLColorWriteMaskAll;
    renderPipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    renderPipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

    switch (blendMode)
    {
    default:
            // 5.3以降
            renderPipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
            renderPipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorZero;
            renderPipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
            renderPipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
        break;

    case CubismRenderer::CubismBlendMode_Mask:
            // Setup masking
            renderPipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorZero;
            renderPipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceColor;
            renderPipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
            renderPipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        break;

    case CubismRenderer::CubismBlendMode_Normal:
            renderPipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
            renderPipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            renderPipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
            renderPipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        break;

    case CubismRenderer::CubismBlendMode_Additive:
            renderPipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
            renderPipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
            renderPipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
            renderPipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
        break;

    case CubismRenderer::CubismBlendMode_Multiplicative:
            renderPipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorDestinationColor;
            renderPipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            renderPipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
            renderPipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
        break;
    }

    return [device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor error:&error];
}

id<MTLDepthStencilState> CubismShader_Metal::MakeDepthStencilState(id<MTLDevice> device)
{
    MTLDepthStencilDescriptor* depthStencilDescriptor = [[[MTLDepthStencilDescriptor alloc] init] autorelease];

    depthStencilDescriptor.depthCompareFunction = MTLCompareFunctionAlways;
    depthStencilDescriptor.depthWriteEnabled = YES;

    return [device newDepthStencilStateWithDescriptor:depthStencilDescriptor];
}

id<MTLSamplerState> CubismShader_Metal::MakeSamplerState(id<MTLDevice> device, CubismRenderer_Metal* renderer)
{
    MTLSamplerDescriptor* samplerDescriptor = [[[MTLSamplerDescriptor alloc] init] autorelease];

    samplerDescriptor.rAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterLinear;

    //異方性フィルタリング
    if (renderer->GetAnisotropy() >= 1.0f)
    {
        samplerDescriptor.maxAnisotropy = renderer->GetAnisotropy();
    }

    return [device newSamplerStateWithDescriptor:samplerDescriptor];
}

}}}}
//------------ LIVE2D NAMESPACE ------------
