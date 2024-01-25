﻿/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismExpressionMotion.hpp"
#include "CubismMotionQueueEntry.hpp"
#include "Id/CubismIdManager.hpp"
#include "Math/CubismMath.hpp"

namespace Live2D { namespace Cubism { namespace Framework {

const csmFloat32 CubismExpressionMotion::DefaultAdditiveValue = 0.0f;    ///<  加算適用の初期値
const csmFloat32 CubismExpressionMotion::DefaultMultiplyValue = 1.0f;    ///<  乗算適用の初期値

namespace {
// exp3.jsonのキーとデフォルト値
const csmChar* ExpressionKeyFadeIn = "FadeInTime";
const csmChar* ExpressionKeyFadeOut = "FadeOutTime";
const csmChar* ExpressionKeyParameters = "Parameters";
const csmChar* ExpressionKeyId = "Id";
const csmChar* ExpressionKeyValue = "Value";
const csmChar* ExpressionKeyBlend = "Blend";
const csmChar* BlendValueAdd = "Add";
const csmChar* BlendValueMultiply = "Multiply";
const csmChar* BlendValueOverwrite = "Overwrite";
const csmFloat32 DefaultFadeTime = 1.0f;
}


CubismExpressionMotion::CubismExpressionMotion()
    : _fadeWeight(0.0f)
{ }

CubismExpressionMotion::~CubismExpressionMotion()
{ }

CubismExpressionMotion* CubismExpressionMotion::Create(const csmByte* buffer, csmSizeInt size)
{
    CubismExpressionMotion* expression = CSM_NEW CubismExpressionMotion();
    expression->Parse(buffer, size);
    return expression;
}

void CubismExpressionMotion::DoUpdateParameters(CubismModel* model, csmFloat32 userTimeSeconds, csmFloat32 weight, CubismMotionQueueEntry* motionQueueEntry)
{
    for (csmUint32 i = 0; i < _parameters.GetSize(); ++i)
    {
        ExpressionParameter& parameter = _parameters[i];

        switch (parameter.BlendType)
        {
        case Additive: {
            model->AddParameterValue(parameter.ParameterId, parameter.Value, weight);            // 相対変化 加算
            break;
        }
        case Multiply: {
            model->MultiplyParameterValue(parameter.ParameterId, parameter.Value, weight);       // 相対変化 乗算
            break;
        }
        case Overwrite: {
            model->SetParameterValue(parameter.ParameterId, parameter.Value, weight);            // 絶対変化 上書き
            break;
        }
        default:
            // 仕様にない値を設定したときは既に加算モードになっている
            break;
        }
    }
}

void CubismExpressionMotion::CalculateExpressionParameters(CubismModel* model, csmFloat32 userTimeSeconds, CubismMotionQueueEntry* motionQueueEntry,
    csmVector<CubismExpressionMotionManager::ExpressionParameterValue>* expressionParameterValues, csmInt32 expressionIndex)
{
    if (!motionQueueEntry->IsAvailable() || motionQueueEntry->IsFinished())
    {
        return;
    }

    if (!motionQueueEntry->IsStarted())
    {
        motionQueueEntry->IsStarted(true);
        motionQueueEntry->SetStartTime(userTimeSeconds - _offsetSeconds); //モーションの開始時刻を記録
        motionQueueEntry->SetFadeInStartTime(userTimeSeconds); //フェードインの開始時刻

        const csmFloat32 duration = GetDuration();

        if (motionQueueEntry->GetEndTime() < 0.0f)
        {
            //開始していないうちに終了設定している場合がある。
            motionQueueEntry->SetEndTime((duration <= 0.0f) ? -1 : motionQueueEntry->GetStartTime() + duration);
            //duration == -1 の場合はループする
        }
    }

    _fadeWeight = UpdateFadeWeight(motionQueueEntry, userTimeSeconds);

    // モデルに適用する値を計算
    for (csmInt32 i = 0; i < expressionParameterValues->GetSize(); ++i)
    {
        CubismExpressionMotionManager::ExpressionParameterValue expressionParameterValue = expressionParameterValues->At(i);

        if (expressionParameterValue.ParameterId == NULL)
        {
            continue;
        }

        const csmFloat32 currentParameterValue = expressionParameterValue.OverwriteValue =
            model->GetParameterValue(expressionParameterValue.ParameterId);

        csmVector<ExpressionParameter> expressionParameters = GetExpressionParameters();
        csmInt32 parameterIndex = -1;
        for (csmInt32 j = 0; j < expressionParameters.GetSize(); ++j)
        {
            if (expressionParameterValue.ParameterId != expressionParameters[j].ParameterId)
            {
                continue;
            }

            parameterIndex = j;

            break;
        }

        // 再生中のExpressionが参照していないパラメータは初期値を適用
        if (parameterIndex < 0)
        {
            if (expressionIndex == 0)
            {
                expressionParameterValues->At(i).AdditiveValue = DefaultAdditiveValue;

                expressionParameterValues->At(i).MultiplyValue = DefaultMultiplyValue;

                expressionParameterValues->At(i).OverwriteValue = currentParameterValue;
            }
            else
            {
                expressionParameterValues->At(i).AdditiveValue =
                    CalculateValue(expressionParameterValue.AdditiveValue, DefaultAdditiveValue);

                expressionParameterValues->At(i).MultiplyValue =
                    CalculateValue(expressionParameterValue.MultiplyValue, DefaultMultiplyValue);

                expressionParameterValues->At(i).OverwriteValue =
                    CalculateValue(expressionParameterValue.OverwriteValue, currentParameterValue);
            }
            continue;
        }

        // 値を計算
        csmFloat32 value = expressionParameters.At(parameterIndex).Value;
        csmFloat32 newAdditiveValue, newMultiplyValue, newSetValue;
        switch (expressionParameters.At(parameterIndex).BlendType) {
        case Additive:
            newAdditiveValue = value;
            newMultiplyValue = DefaultMultiplyValue;
            newSetValue = currentParameterValue;
            break;
        case Multiply:
            newAdditiveValue = DefaultAdditiveValue;
            newMultiplyValue = value;
            newSetValue = currentParameterValue;
            break;
        case Overwrite:
            newAdditiveValue = DefaultAdditiveValue;
            newMultiplyValue = DefaultMultiplyValue;
            newSetValue = value;
            break;
        default:
            return;
        }

        if (expressionIndex == 0) {
            expressionParameterValues->At(i).AdditiveValue = newAdditiveValue;
            expressionParameterValues->At(i).MultiplyValue = newMultiplyValue;
            expressionParameterValues->At(i).OverwriteValue = newSetValue;
        }
        else {
            expressionParameterValues->At(i).AdditiveValue = (expressionParameterValue.AdditiveValue * (1.0f - _fadeWeight)) + newAdditiveValue * _fadeWeight;
            expressionParameterValues->At(i).MultiplyValue = (expressionParameterValue.MultiplyValue * (1.0f - _fadeWeight)) + newMultiplyValue * _fadeWeight;
            expressionParameterValues->At(i).OverwriteValue = (expressionParameterValue.OverwriteValue * (1.0f - _fadeWeight)) + newSetValue * _fadeWeight;
        }
    }
}

csmVector<CubismExpressionMotion::ExpressionParameter> CubismExpressionMotion::GetExpressionParameters()
{
    return _parameters;
}

csmFloat32 CubismExpressionMotion::GetFadeWeight()
{
    return _fadeWeight;
}

void CubismExpressionMotion::Parse(const csmByte* buffer, csmSizeInt size)
{
    Utils::CubismJson* json = Utils::CubismJson::Create(buffer, size);
    if (!json)
    {
        return;
    }

    Utils::Value& root = json->GetRoot();

    SetFadeInTime(root[ExpressionKeyFadeIn].ToFloat(DefaultFadeTime));   // フェードイン
    SetFadeOutTime(root[ExpressionKeyFadeOut].ToFloat(DefaultFadeTime)); // フェードアウト

    // 各パラメータについて
    const csmInt32 parameterCount = root[ExpressionKeyParameters].GetSize();
    _parameters.PrepareCapacity(parameterCount);

    for (csmInt32 i = 0; i < parameterCount; ++i)
    {
        Utils::Value& param = root[ExpressionKeyParameters][i];
        const CubismIdHandle parameterId = CubismFramework::GetIdManager()->GetId(param[ExpressionKeyId].GetRawString()); // パラメータID
        const csmFloat32 value = static_cast<csmFloat32>(param[ExpressionKeyValue].ToFloat());                            // 値

        // 計算方法の設定
        ExpressionBlendType blendType;

        if (param[ExpressionKeyBlend].IsNull() || param[ExpressionKeyBlend].GetString() == BlendValueAdd)
        {
            blendType = Additive;
        }
        else if (param[ExpressionKeyBlend].GetString() == BlendValueMultiply)
        {
            blendType = Multiply;
        }
        else if (param[ExpressionKeyBlend].GetString() == BlendValueOverwrite)
        {
            blendType = Overwrite;
        }
        else
        {
            // その他 仕様にない値を設定したときは加算モードにすることで復旧
            blendType = Additive;
        }

        // 設定オブジェクトを作成してリストに追加する
        ExpressionParameter item;

        item.ParameterId = parameterId;
        item.BlendType = blendType;
        item.Value = value;

        _parameters.PushBack(item);
    }

    Utils::CubismJson::Delete(json);// JSONデータは不要になったら削除する
}

csmFloat32 CubismExpressionMotion::CalculateValue(csmFloat32 source, csmFloat32 destination)
{
    return (source * (1.0f - _fadeWeight)) + (destination * _fadeWeight);
}


}}}
