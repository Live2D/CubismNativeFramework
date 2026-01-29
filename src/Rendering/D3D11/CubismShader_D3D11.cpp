/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismShader_D3D11.hpp"
#include "CubismRenderer_D3D11.hpp"
#include <d3dcompiler.h>

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

void CubismShader_D3D11::ReleaseShaderProgram()
{
    // 頂点レイアウト開放
    if (_vertexFormat)
    {
        _vertexFormat->Release();
        _vertexFormat = NULL;
    }

    // 器はそのまま
    for (csmInt32 i = 0; i < ShaderNames_Max; i++)
    {
        if(_shaderSets[i].VertexShader)
        {
            if (_shaderSets[i].IsOriginalVertexData)
            {
                _shaderSets[i].VertexShader->Release();
                _shaderSets[i].IsOriginalVertexData = false;
            }
            _shaderSets[i].VertexShader = NULL;
        }

        if (_shaderSets[i].PixelShader)
        {
            _shaderSets[i].PixelShader->Release();
            _shaderSets[i].PixelShader = NULL;
        }
    }
}

CubismShader_D3D11::CubismShader_D3D11()
    : _vertexFormat(NULL)
{
    // 器作成
    _shaderSets.PrepareCapacity(ShaderNames_Max);
    for (csmInt32 i = 0; i < ShaderNames_Max; i++)
    {
        _shaderSets.PushBack(CubismShaderSet());
    }
}

CubismShader_D3D11::~CubismShader_D3D11()
{
    ReleaseShaderProgram();

    // 器の削除
    _shaderSets.Clear();
}

void CubismShader_D3D11::GenerateShaders(ID3D11Device* device)
{
    // 既に有るかどうかをこれで判断している
    if(_vertexFormat!=NULL)
    {
        return;
    }

    // 一旦開放
    ReleaseShaderProgram();

    csmBool isSuccess = false;
    do
    {
        // シェーダーソースコードが記述されているファイルを読み込み
        const csmChar* frameworkShaderPath = "FrameworkShaders/CubismEffect.fx";
        const csmChar* frameworkBlendShaderPath = "FrameworkShaders/CubismBlendMode.fx";
        csmLoadFileFunction fileLoader = CubismFramework::GetLoadFileFunction();
        csmReleaseBytesFunction bytesReleaser = CubismFramework::GetReleaseBytesFunction();

        if (!fileLoader)
        {
            CubismLogError("File loader is not set.");
            break;
        }

        if (!bytesReleaser)
        {
            CubismLogError("Byte releaser is not set.");
            break;
        }

        csmSizeInt effectShaderSize;
        csmByte* effectShaderSrc = fileLoader(frameworkShaderPath, &effectShaderSize);
        csmString shaderString = csmString(reinterpret_cast<const csmChar*>(effectShaderSrc), effectShaderSize);
        csmSizeInt blendShaderSize;
        csmByte* blendShaderSrc = fileLoader(frameworkBlendShaderPath, &blendShaderSize);
        shaderString += csmString(reinterpret_cast<const csmChar*>(blendShaderSrc), blendShaderSize);

        // ファイル読み込みで確保したバイト列を開放
        bytesReleaser(effectShaderSrc);
        bytesReleaser(blendShaderSrc);

        // コピー
        if (!LoadShaderProgram(device, false, ShaderNames_Copy, "VertCopy", shaderString))
        {
            break;
        }
        if (!LoadShaderProgram(device, true, ShaderNames_Copy, "PixelCopy", shaderString))
        {
            break;
        }

        // マスク
        if(!LoadShaderProgram(device, false, ShaderNames_SetupMask, "VertSetupMask", shaderString))
        {
            break;
        }
        if (!LoadShaderProgram(device, true, ShaderNames_SetupMask, "PixelSetupMask", shaderString))
        {
            break;
        }

        // 他描画用シェーダ
#define CSM_LOAD_SHADER_PROGRAM(VERT_NAME, SHADER_NAME, PIXEL_ENTRY_POINT_SRC) {\
        _shaderSets[SHADER_NAME].VertexShader = _shaderSets[VERT_NAME].VertexShader; \
        if (!LoadShaderProgram(device, true, SHADER_NAME, #PIXEL_ENTRY_POINT_SRC, shaderString)) \
        { \
            break; \
        } \
    }

#define CSM_SET_SHADER_52(TYPE) {\
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_Normal, ShaderNames_ ## TYPE, PixelNormal) \
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalMasked, ShaderNames_ ## TYPE ## Masked, PixelMasked) \
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalMasked, ShaderNames_ ## TYPE ## MaskedInverted, PixelMaskedInverted) \
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_Normal, ShaderNames_ ## TYPE ## PremultipliedAlpha, PixelNormalPremult) \
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalMasked, ShaderNames_ ## TYPE ## MaskedPremultipliedAlpha, PixelMaskedPremult) \
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalMasked, ShaderNames_ ## TYPE ## MaskedInvertedPremultipliedAlpha, PixelMaskedInvertedPremult) \
    }

#define CSM_SET_SHADER_53(COLOR, ALPHA) {\
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalAtop, ShaderNames_ ## COLOR ## ALPHA, PixelNormalBlend ## COLOR ## ALPHA) \
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalAtopMasked, ShaderNames_ ## COLOR ## ALPHA ## Masked, PixelMaskedBlend ## COLOR ## ALPHA) \
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalAtopMasked, ShaderNames_ ## COLOR ## ALPHA ## MaskedInverted, PixelMaskedInvertedBlend ## COLOR ## ALPHA) \
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalAtop, ShaderNames_ ## COLOR ## ALPHA ## PremultipliedAlpha, PixelNormalPremultBlend ## COLOR ## ALPHA) \
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalAtopMasked, ShaderNames_ ## COLOR ## ALPHA ## MaskedPremultipliedAlpha, PixelMaskedPremultBlend ## COLOR ## ALPHA) \
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalAtopMasked, ShaderNames_ ## COLOR ## ALPHA ## MaskedInvertedPremultipliedAlpha, PixelMaskedInvertedPremultBlend ## COLOR ## ALPHA) \
    }

#define CSM_SET_BLEND_OVERLAP_SHADER(COLOR) {\
        CSM_SET_SHADER_53(COLOR, Over) \
        CSM_SET_SHADER_53(COLOR, Atop) \
        CSM_SET_SHADER_53(COLOR, Out) \
        CSM_SET_SHADER_53(COLOR, ConjointOver) \
        CSM_SET_SHADER_53(COLOR, DisjointOver) \
    }

        // 5.2以前 copy
        if (!LoadShaderProgram(device, false, ShaderNames_Copy, "VertCopy", shaderString))
        {
            break;
        }
        if (!LoadShaderProgram(device, true, ShaderNames_Copy, "PixelCopy", shaderString))
        {
            break;
        }

        // 5.2以前 setup mask
        if (!LoadShaderProgram(device, false, ShaderNames_SetupMask, "VertSetupMask", shaderString))
        {
            break;
        }
        if (!LoadShaderProgram(device, true, ShaderNames_SetupMask, "PixelSetupMask", shaderString))
        {
            break;
    }

        // 5.2以前 normal
        if (!LoadShaderProgram(device, false, ShaderNames_Normal, "VertNormal", shaderString, true))
        {
            break;
        }
        if (!LoadShaderProgram(device, true, ShaderNames_Normal, "PixelNormal", shaderString))
        {
            break;
        }
        if (!LoadShaderProgram(device, false, ShaderNames_NormalMasked, "VertMasked", shaderString))
        {
            break;
        }
        if (!LoadShaderProgram(device, true, ShaderNames_NormalMasked, "PixelMasked", shaderString))
        {
            break;
        }
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalMasked, ShaderNames_NormalMaskedInverted, PixelMaskedInverted)
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_Normal, ShaderNames_NormalPremultipliedAlpha, PixelNormalPremult)
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalMasked, ShaderNames_NormalMaskedPremultipliedAlpha, PixelMaskedPremult)
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalMasked, ShaderNames_NormalMaskedInvertedPremultipliedAlpha, PixelMaskedInvertedPremult)
        // 5.2以前 add
        CSM_SET_SHADER_52(Add)
        // 5.2以前 mult
        CSM_SET_SHADER_52(Mult)

        // 5.3以降 normal
        if (!LoadShaderProgram(device, false, ShaderNames_NormalAtop, "VertNormalBlend", shaderString))
        {
            break;
        }
        if (!LoadShaderProgram(device, true, ShaderNames_NormalAtop, "PixelNormalBlendNormalAtop", shaderString))
        {
            break;
        }
        if (!LoadShaderProgram(device, false, ShaderNames_NormalAtopMasked, "VertMaskedBlend", shaderString))
        {
            break;
        }
        if (!LoadShaderProgram(device, true, ShaderNames_NormalAtopMasked, "PixelMaskedBlendNormalAtop", shaderString))
        {
            break;
        }
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalAtopMasked, ShaderNames_NormalAtopMaskedInverted, PixelMaskedInvertedBlendNormalAtop)
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalAtop, ShaderNames_NormalAtopPremultipliedAlpha, PixelNormalPremultBlendNormalAtop)
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalAtopMasked, ShaderNames_NormalAtopMaskedPremultipliedAlpha, PixelMaskedPremultBlendNormalAtop)
        CSM_LOAD_SHADER_PROGRAM(ShaderNames_NormalAtopMasked, ShaderNames_NormalAtopMaskedInvertedPremultipliedAlpha, PixelMaskedInvertedPremultBlendNormalAtop)
        CSM_SET_SHADER_53(Normal, Out)
        CSM_SET_SHADER_53(Normal, ConjointOver)
        CSM_SET_SHADER_53(Normal, DisjointOver)
        // 5.3以降 add
        CSM_SET_BLEND_OVERLAP_SHADER(Add)
        // 5.3以降 add(glow)
        CSM_SET_BLEND_OVERLAP_SHADER(AddGlow)
        // 5.3以降 darken
        CSM_SET_BLEND_OVERLAP_SHADER(Darken)
        // 5.3以降 multiply
        CSM_SET_BLEND_OVERLAP_SHADER(Multiply)
        // 5.3以降 color burn
        CSM_SET_BLEND_OVERLAP_SHADER(ColorBurn)
        // 5.3以降 linear burn
        CSM_SET_BLEND_OVERLAP_SHADER(LinearBurn)
        // 5.3以降 lighten
        CSM_SET_BLEND_OVERLAP_SHADER(Lighten)
        // 5.3以降 screen
        CSM_SET_BLEND_OVERLAP_SHADER(Screen)
        // 5.3以降 color dodge
        CSM_SET_BLEND_OVERLAP_SHADER(ColorDodge)
        // 5.3以降 overlay
        CSM_SET_BLEND_OVERLAP_SHADER(Overlay)
        // 5.3以降 soft light
        CSM_SET_BLEND_OVERLAP_SHADER(SoftLight)
        // 5.3以降 hard light
        CSM_SET_BLEND_OVERLAP_SHADER(HardLight)
        // 5.3以降 linear light
        CSM_SET_BLEND_OVERLAP_SHADER(LinearLight)
        // 5.3以降 hue
        CSM_SET_BLEND_OVERLAP_SHADER(Hue)
        // 5.3以降 color
        CSM_SET_BLEND_OVERLAP_SHADER(Color)

#undef CSM_SET_BLEND_OVERLAP_SHADER
#undef CSM_SET_SHADER53
#undef CSM_SET_SHADER52
#undef CSM_LOAD_SHADER_PROGRAM

        // 成功
        isSuccess = true;
    } while (0);

    if (!isSuccess)
    {
        CubismLogError("Fail Compile shader");
        CSM_ASSERT(0);
        return;
    }
}

csmBool CubismShader_D3D11::LoadShaderProgram(ID3D11Device* device, bool isPs, csmInt32 assign, const csmChar* entryPoint, const csmString& shaderSrc, csmBool isFormat)
{
    csmBool bRet = false;
    if (!device)
    {
        return false;
    }

    ID3DBlob* errorBlob = NULL;
    ID3DBlob* vertexBlob = NULL;

    ID3D11VertexShader* vertexShader = NULL;
    ID3D11PixelShader* pixelShader = NULL;

    HRESULT hr = S_OK;
    do
    {
        UINT compileFlag = 0;
#ifdef CSM_DEBUG
        // デバッグならば
        compileFlag |= D3DCOMPILE_DEBUG;
#endif

        hr = D3DCompile(
            shaderSrc.GetRawString(),           // メモリー内のシェーダーへのポインターです。
            shaderSrc.GetLength(),              // メモリー内のシェーダーのサイズです。
            NULL,                               // シェーダー コードが格納されているファイルの名前です。
            NULL,                               // マクロ定義の配列へのポインターです
            NULL,                               // インクルード ファイルを扱うインターフェイスへのポインターです
            entryPoint,                         // シェーダーの実行が開始されるシェーダー エントリポイント関数の名前です。
            isPs ? "ps_4_0" : "vs_4_0",         // シェーダー モデルを指定する文字列。
            compileFlag,                        // シェーダーコンパイルフラグ
            0,                                  // シェーダーエフェクトのフラグ
            &vertexBlob,
            &errorBlob);              // エラーが出る場合はここで
        if (FAILED(hr))
        {
            CubismLogWarning("Fail Compile Shader : %s", entryPoint == NULL ? "" : entryPoint);
            break;
        }
        hr = isPs ? device->CreatePixelShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), NULL, &pixelShader)
                  : device->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), NULL, &vertexShader);
        if (FAILED(hr))
        {
            CubismLogWarning("Fail Create Shader");
            break;
        }

        if (isFormat)
        {
            // この描画で使用する頂点フォーマット
            D3D11_INPUT_ELEMENT_DESC elems[] = {
                { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            };
            HRESULT hr = device->CreateInputLayout(elems, ARRAYSIZE(elems), vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), &_vertexFormat);

            if (FAILED(hr))
            {
                CubismLogWarning("CreateVertexDeclaration failed");
            }
        }

        if (isPs)
        {
            _shaderSets[assign].PixelShader = pixelShader;
        }
        else
        {
            _shaderSets[assign].VertexShader = vertexShader;
            _shaderSets[assign].IsOriginalVertexData = true;
        }
        // 成功
        bRet = true;
    } while (0);

    if (errorBlob)
    {
        errorBlob->Release();
    }
    if (vertexBlob)
    {
        vertexBlob->Release();
    }

    return bRet;
}

ID3D11VertexShader* CubismShader_D3D11::GetVertexShader(csmUint32 assign)
{
    if(assign<ShaderNames_Max)
    {
        return _shaderSets[assign].VertexShader;
    }

    return NULL;
}

ID3D11PixelShader* CubismShader_D3D11::GetPixelShader(csmUint32 assign)
{
    if (assign<ShaderNames_Max)
    {
        return _shaderSets[assign].PixelShader;
    }

    return NULL;
}

void CubismShader_D3D11::SetupShader(ID3D11Device* device, ID3D11DeviceContext* renderContext)
{
    // まだシェーダ・頂点宣言未作成ならば作成する
    GenerateShaders(device);

    if (!renderContext || !_vertexFormat)
    {
        return;
    }

    renderContext->IASetInputLayout(_vertexFormat);
}

}}}}

//------------ LIVE2D NAMESPACE ------------

