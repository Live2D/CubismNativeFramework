/*
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at http://live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismModelSettingJson.hpp"
#include "CubismFramework.hpp"
#include "Type/csmMap.hpp"
#include "Id/CubismId.hpp"
#include "Id/CubismIdManager.hpp"

namespace Live2D { namespace Cubism { namespace Framework {

/**
 * @brief   Model3Jsonのキー文字列
 *
 */
namespace {
// JSON keys
const csmChar* Version = "Version";
const csmChar* FileReferences = "FileReferences";
const csmChar* Groups = "Groups";
const csmChar* Layout = "Layout";
const csmChar* HitAreas = "HitAreas";

const csmChar* Moc = "Moc";
const csmChar* Textures = "Textures";
const csmChar* Physics = "Physics";
const csmChar* Pose = "Pose";
const csmChar* Expressions = "Expressions";
const csmChar* Motions = "Motions";

const csmChar* UserData = "UserData";
const csmChar* Name = "Name";
const csmChar* FilePath = "File";
const csmChar* Id = "Id";
const csmChar* Ids = "Ids";
const csmChar* Target = "Target";

// Motions
const csmChar* Idle = "Idle";
const csmChar* TapBody = "TapBody";
const csmChar* PinchIn = "PinchIn";
const csmChar* PinchOut = "PinchOut";
const csmChar* Shake = "Shake";
const csmChar* FlickHead = "FlickHead";
const csmChar* Parameter = "Parameter";

const csmChar* SoundPath = "Sound";
const csmChar* FadeInTime = "FadeInTime";
const csmChar* FadeOutTime = "FadeOutTime";

// Layout
const csmChar* CenterX = "CenterX";
const csmChar* CenterY = "CenterY";
const csmChar* X = "X";
const csmChar* Y = "Y";
const csmChar* Width = "Width";
const csmChar* Height = "Height";

const csmChar* LipSync = "LipSync";
const csmChar* EyeBlink = "EyeBlink";

const csmChar* InitParameter = "init_param";
const csmChar* InitPartsVisible = "init_parts_visible";
const csmChar* Val = "val";
}

// キーが存在するかどうかのチェック
csmBool CubismModelSettingJson::IsExistModelFile() const { return !_json->GetRoot()[FileReferences][Moc].IsNull(); }
csmBool CubismModelSettingJson::IsExistTextureFiles() const { return !_json->GetRoot()[FileReferences][Textures].IsNull(); }
csmBool CubismModelSettingJson::IsExistHitAreas() const { return !_json->GetRoot()[HitAreas].IsNull(); }
csmBool CubismModelSettingJson::IsExistPhysicsFile() const { return !_json->GetRoot()[FileReferences][Physics].IsNull(); }
csmBool CubismModelSettingJson::IsExistPoseFile() const { return !_json->GetRoot()[FileReferences][Pose].IsNull(); }
csmBool CubismModelSettingJson::IsExistExpressionFile() const { return !_json->GetRoot()[FileReferences][Expressions].IsNull(); }
csmBool CubismModelSettingJson::IsExistMotionGroups() const { return !_json->GetRoot()[FileReferences][Motions].IsNull(); }
csmBool CubismModelSettingJson::IsExistMotionGroupName(const csmChar* groupName) const { return !_json->GetRoot()[FileReferences][Motions][groupName].IsNull(); }
csmBool CubismModelSettingJson::IsExistMotionSoundFile(const csmChar* groupName, csmInt32 index) const { return !_json->GetRoot()[FileReferences][Motions][groupName][index][SoundPath].IsNull(); }
csmBool CubismModelSettingJson::IsExistMotionFadeIn(const csmChar* groupName, csmInt32 index) const { return !_json->GetRoot()[FileReferences][Motions][groupName][index][FadeInTime].IsNull(); }
csmBool CubismModelSettingJson::IsExistMotionFadeOut(const csmChar* groupName, csmInt32 index) const { return !_json->GetRoot()[FileReferences][Motions][groupName][index][FadeOutTime].IsNull(); }
csmBool CubismModelSettingJson::IsExistUserDataFile() const { return !_json->GetRoot()[FileReferences][UserData].IsNull(); }


csmBool CubismModelSettingJson::IsExistEyeBlinkParameters() const
{
    for (csmInt32 i = 0; i < _json->GetRoot()[Groups].GetSize(); ++i)
    {
        if (strcmp(_json->GetRoot()[Groups][i][Name].GetRawString(), EyeBlink) == 0)
        {
            return true;
        }
    }
    return false;
}

csmBool CubismModelSettingJson::IsExistLipSyncParameters() const
{
    for (csmInt32 i = 0; i < _json->GetRoot()[Groups].GetSize(); ++i)
    {
        if (strcmp(_json->GetRoot()[Groups][i][Name].GetRawString(), LipSync) == 0)
        {
            return true;
        }
    }
    return false;
}

CubismModelSettingJson::CubismModelSettingJson(const csmByte* buffer, csmSizeInt size)
{
    _json = Utils::CubismJson::Create(buffer, size);
}

CubismModelSettingJson::~CubismModelSettingJson()
{
    Utils::CubismJson::Delete( _json);
}

Utils::CubismJson* CubismModelSettingJson::GetJsonPointer() const
{
    return _json;
}

const csmChar* CubismModelSettingJson::GetModelFileName()
{
    if (!IsExistModelFile())return "";
    return _json->GetRoot()[FileReferences][Moc].GetRawString();
}

// テクスチャについて
csmInt32 CubismModelSettingJson::GetTextureCount()
{
    if (!IsExistTextureFiles())return 0;
    return _json->GetRoot()[FileReferences][Textures].GetSize();
}

const csmChar* CubismModelSettingJson::GetTextureDirectory()
{
    return _json->GetRoot()[FileReferences][Textures].GetRawString();
}

const csmChar* CubismModelSettingJson::GetTextureFileName(csmInt32 index)
{
    return _json->GetRoot()[FileReferences][Textures][index].GetRawString();
}

// あたり判定について
csmInt32 CubismModelSettingJson::GetHitAreasCount()
{
    if (!IsExistHitAreas())return 0;
    return _json->GetRoot()[HitAreas].GetSize();
}

CubismIdHandle CubismModelSettingJson::GetHitAreaId(csmInt32 index)
{
    return CubismFramework::GetIdManager()->GetId(_json->GetRoot()[HitAreas][index][Id].GetRawString());
}

const csmChar* CubismModelSettingJson::GetHitAreaName(csmInt32 index)
{
    return _json->GetRoot()[HitAreas][index][Name].GetRawString();
}

// 物理演算、パーツ切り替え、表情ファイルについて
const csmChar* CubismModelSettingJson::GetPhysicsFileName()
{
    if (!IsExistPhysicsFile())return "";
    return _json->GetRoot()[FileReferences][Physics].GetRawString();
}

const csmChar* CubismModelSettingJson::GetPoseFileName()
{
    if (!IsExistPoseFile())return "";
    return _json->GetRoot()[FileReferences][Pose].GetRawString();
}

csmInt32 CubismModelSettingJson::GetExpressionCount()
{
    if (!IsExistExpressionFile())return 0;
    return _json->GetRoot()[FileReferences][Expressions].GetSize();
}

const csmChar* CubismModelSettingJson::GetExpressionName(csmInt32 index)
{
    return _json->GetRoot()[FileReferences][Expressions][index][Name].GetRawString();
}

const csmChar* CubismModelSettingJson::GetExpressionFileName(csmInt32 index)
{
    return _json->GetRoot()[FileReferences][Expressions][index][FilePath].GetRawString();
}

// モーションについて
csmInt32 CubismModelSettingJson::GetMotionGroupCount()
{
    if (!IsExistMotionGroups())
    {
        return 0;
    }
    return _json->GetRoot()[FileReferences][Motions].GetKeys().GetSize();
}

const csmChar* CubismModelSettingJson::GetMotionGroupName(csmInt32 index)
{
    if (!IsExistMotionGroups())
    {
        return NULL;
    }
    return _json->GetRoot()[FileReferences][Motions].GetKeys()[index].GetRawString();
}

csmInt32 CubismModelSettingJson::GetMotionCount(const csmChar* groupName)
{
    if (!IsExistMotionGroupName(groupName))return 0;
    return _json->GetRoot()[FileReferences][Motions][groupName].GetSize();
}

const csmChar* CubismModelSettingJson::GetMotionFileName(const csmChar* groupName, csmInt32 index)
{
    if (!IsExistMotionGroupName(groupName))return "";
    return _json->GetRoot()[FileReferences][Motions][groupName][index][FilePath].GetRawString();
}

const csmChar* CubismModelSettingJson::GetMotionSoundFileName(const csmChar* groupName, csmInt32 index)
{
    if (!IsExistMotionSoundFile(groupName, index))return "";
    return _json->GetRoot()[FileReferences][Motions][groupName][index][SoundPath].GetRawString();
}

csmFloat32 CubismModelSettingJson::GetMotionFadeInTimeValue(const csmChar* groupName, csmInt32 index)
{
    if (!IsExistMotionFadeIn(groupName, index))return -1.0f;
    return _json->GetRoot()[FileReferences][Motions][groupName][index][FadeInTime].ToFloat();
}

csmFloat32 CubismModelSettingJson::GetMotionFadeOutTimeValue(const csmChar* groupName, csmInt32 index)
{
    if (!IsExistMotionFadeOut(groupName, index))return -1.0f;
    return _json->GetRoot()[FileReferences][Motions][groupName][index][FadeOutTime].ToFloat();
}


const csmChar* CubismModelSettingJson::GetUserDataFile()
{
    if (!IsExistUserDataFile())
    {
        return "";
    }
    return _json->GetRoot()[FileReferences][UserData].GetRawString();
}

csmBool CubismModelSettingJson::GetLayoutMap(csmMap<csmString, csmFloat32>& outLayoutMap) 
{
    csmMap<csmString, Utils::Value*>* map = _json->GetRoot()[Layout].GetMap();
    if (map == NULL)
    {
        return false;
    }
    csmMap<csmString, Utils::Value*>::const_iterator map_ite;
    csmBool ret = false;
    for (map_ite = map->Begin(); map_ite != map->End(); ++map_ite)
    {
        outLayoutMap[map_ite->First] = map_ite->Second->ToFloat();
        ret = true;
    }
    return ret;
}

csmInt32 CubismModelSettingJson::GetEyeBlinkParameterCount()
{
    if (!IsExistEyeBlinkParameters())
    {
        return 0;
    }

    csmInt32 num = 0;
    for (csmInt32 i = 0; i < _json->GetRoot()[Groups].GetSize(); i++)
    {
        if (strcmp(_json->GetRoot()[Groups][i][Name].GetRawString(), EyeBlink) == 0)
        {
            num = _json->GetRoot()[Groups][i][Ids].GetVector()->GetSize();
            break;
        }
    }

    return num;
}

CubismIdHandle CubismModelSettingJson::GetEyeBlinkParameterId(csmInt32 index)
{
    if (!IsExistEyeBlinkParameters())
    {
        return NULL;
    }

    for (csmInt32 i = 0; i < _json->GetRoot()[Groups].GetSize(); i++)
    {
        if (strcmp(_json->GetRoot()[Groups][i][Name].GetRawString(), EyeBlink) == 0)
        {
            return CubismFramework::GetIdManager()->GetId(_json->GetRoot()[Groups][i][Ids][index].GetRawString());
        }
    }
    return NULL;
}

csmInt32 CubismModelSettingJson::GetLipSyncParameterCount()
{
    if (!IsExistLipSyncParameters())
    {
        return 0;
    }

    csmInt32 num = 0;
    for (csmInt32 i = 0; i < _json->GetRoot()[Groups].GetSize(); i++)
    {
        if (strcmp(_json->GetRoot()[Groups][i][Name].GetRawString(), LipSync) == 0)
        {
            num = _json->GetRoot()[Groups][i][Ids].GetVector()->GetSize();
            break;
        }
    }

    return num;
}

CubismIdHandle CubismModelSettingJson::GetLipSyncParameterId(csmInt32 index)
{
    if (!IsExistLipSyncParameters())
    {
        return NULL;
    }

    for (csmInt32 i = 0; i < _json->GetRoot()[Groups].GetSize(); i++)
    {
        if (strcmp(_json->GetRoot()[Groups][i][Name].GetRawString(), LipSync) == 0)
        {
            return CubismFramework::GetIdManager()->GetId(_json->GetRoot()[Groups][i][Ids][index].GetRawString());
        }
    }
    return NULL;
}

}}}
