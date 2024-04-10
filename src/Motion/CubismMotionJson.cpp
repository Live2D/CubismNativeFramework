/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "CubismMotionJson.hpp"
#include "Id/CubismId.hpp"
#include "Id/CubismIdManager.hpp"
#include <cassert>
#include <sstream>

namespace Live2D { namespace Cubism {
namespace Framework {


namespace {
// JSON keys
const csmChar* Meta = "Meta";
const csmChar* Duration = "Duration";
const csmChar* Loop = "Loop";
const csmChar* AreBeziersRestricted = "AreBeziersRestricted";
const csmChar* CurveCount = "CurveCount";
const csmChar* Fps = "Fps";
const csmChar* TotalSegmentCount = "TotalSegmentCount";
const csmChar* TotalPointCount = "TotalPointCount";
const csmChar* Curves = "Curves";
const csmChar* Target = "Target";
const csmChar* Id = "Id";
const csmChar* FadeInTime = "FadeInTime";
const csmChar* FadeOutTime = "FadeOutTime";
const csmChar* Segments = "Segments";
const csmChar* UserData = "UserData";
const csmChar* UserDataCount = "UserDataCount";
const csmChar* TotalUserDataSize = "TotalUserDataSize";
const csmChar* Time = "Time";
const csmChar* Value = "Value";
}

CubismMotionJson::CubismMotionJson(const csmByte* buffer, csmSizeInt size, bool trustUserBounds) : _trustUserBounds(trustUserBounds)
{
    CreateCubismJson(buffer, size);
    if (!trustUserBounds)
        RecoverTotals();
}

CubismMotionJson::~CubismMotionJson()
{
    DeleteCubismJson();
}

csmFloat32 CubismMotionJson::GetMotionDuration() const
{
    return _json->GetRoot()[Meta][Duration].ToFloat();
}

csmBool CubismMotionJson::IsMotionLoop() const
{
    return _json->GetRoot()[Meta][Loop].ToBoolean();
}

csmBool CubismMotionJson::GetEvaluationOptionFlag(const csmInt32 flagType) const
{
    if (EvaluationOptionFlag_AreBeziersRestricted == flagType)
    {
        return _json->GetRoot()[Meta][AreBeziersRestricted].ToBoolean();
    }

    return false;
}

csmInt32 CubismMotionJson::GetMotionCurveCount() const
{
    if (_trustUserBounds)
        return _json->GetRoot()[Meta][CurveCount].ToInt();
    else
        return CountCurves();
    
}

csmFloat32 CubismMotionJson::GetMotionFps() const
{
    return _json->GetRoot()[Meta][Fps].ToFloat();
}

csmInt32 CubismMotionJson::GetMotionTotalSegmentCount() const
{
    if(_trustUserBounds)
        return _json->GetRoot()[Meta][TotalSegmentCount].ToInt();
    else
        return CountSegments();
}

csmInt32 CubismMotionJson::GetMotionTotalPointCount() const
{
    if(_trustUserBounds)
        return _json->GetRoot()[Meta][TotalPointCount].ToInt();
    else
        return CountPoints();
}

csmBool CubismMotionJson::IsExistMotionFadeInTime() const
{
    return !_json->GetRoot()[Meta][FadeInTime].IsNull();
}

csmBool CubismMotionJson::IsExistMotionFadeOutTime() const
{
    return !_json->GetRoot()[Meta][FadeOutTime].IsNull();
}

csmFloat32 CubismMotionJson::GetMotionFadeInTime() const
{
    return _json->GetRoot()[Meta][FadeInTime].ToFloat();
}

csmFloat32 CubismMotionJson::GetMotionFadeOutTime() const
{
    return _json->GetRoot()[Meta][FadeOutTime].ToFloat();
}

const csmChar* CubismMotionJson::GetMotionCurveTarget(csmInt32 curveIndex) const
{
    return _json->GetRoot()[Curves][curveIndex][Target].GetRawString();
}

CubismIdHandle CubismMotionJson::GetMotionCurveId(csmInt32 curveIndex) const
{
    return CubismFramework::GetIdManager()->GetId(_json->GetRoot()[Curves][curveIndex][Id].GetRawString());
}

csmBool CubismMotionJson::IsExistMotionCurveFadeInTime(csmInt32 curveIndex) const
{
    return !_json->GetRoot()[Curves][curveIndex][FadeInTime].IsNull();
}

csmBool CubismMotionJson::IsExistMotionCurveFadeOutTime(csmInt32 curveIndex) const
{
    return !_json->GetRoot()[Curves][curveIndex][FadeOutTime].IsNull();
}

csmFloat32 CubismMotionJson::GetMotionCurveFadeInTime(csmInt32 curveIndex) const
{
    return _json->GetRoot()[Curves][curveIndex][FadeInTime].ToFloat();
}

csmFloat32 CubismMotionJson::GetMotionCurveFadeOutTime(csmInt32 curveIndex) const
{
    return _json->GetRoot()[Curves][curveIndex][FadeOutTime].ToFloat();
}

csmInt32 CubismMotionJson::GetMotionCurveSegmentCount(csmInt32 curveIndex) const
{
    return static_cast<csmInt32>(_json->GetRoot()[Curves][curveIndex][Segments].GetVector()->GetSize());
}

csmFloat32 CubismMotionJson::GetMotionCurveSegment(csmInt32 curveIndex, csmInt32 segmentIndex) const
{
    return _json->GetRoot()[Curves][curveIndex][Segments][segmentIndex].ToFloat();
}

csmInt32 CubismMotionJson::GetEventCount() const
{
    return _json->GetRoot()[Meta][UserDataCount].ToInt();
}

csmInt32 CubismMotionJson::GetTotalEventValueSize() const
{
    return _json->GetRoot()[Meta][TotalUserDataSize].ToInt();
}

csmFloat32 CubismMotionJson::GetEventTime(csmInt32 userDataIndex) const
{
    return _json->GetRoot()[UserData][userDataIndex][Time].ToFloat();
}

const csmChar* CubismMotionJson::GetEventValue(csmInt32 userDataIndex) const
{
    return _json->GetRoot()[UserData][userDataIndex][Value].GetRawString();
}
constexpr char TOTAL_CURVES = 0;
constexpr char TOTAL_SEGMENTS = 1;
constexpr char TOTAL_POINTS = 2;
inline csmInt32 CubismMotionJson::GetRecoveredTotal(csmChar const key) const {
    assert(0 <= key && key <= 2);
    if(recoveredTotals == nullptr)
        throw std::invalid_argument("Requested totals which have not been calculated yet.");
    csmInt32 out = 0;
    switch (key) {
    case TOTAL_CURVES:
        out = std::get<TOTAL_CURVES>(*recoveredTotals);
        break;
    case TOTAL_SEGMENTS:
        out = std::get<TOTAL_SEGMENTS>(*recoveredTotals);
        break;
    case TOTAL_POINTS:
        out = std::get<TOTAL_POINTS>(*recoveredTotals);
        break;
    default:
        std::ostringstream errmsg;
        errmsg << "Invalid tuple offset: " << key << std::endl;
        throw std::invalid_argument(errmsg.str());
    }
    return out;
}
inline csmInt32 CubismMotionJson::CountCurves() const {
  return GetRecoveredTotal(TOTAL_CURVES);
}
inline csmInt32 CubismMotionJson::CountSegments() const {
  return GetRecoveredTotal(TOTAL_SEGMENTS);
}
constexpr csmChar SEGMENT_LINEAR = 0;
constexpr csmChar SEGMENT_CUBIC_BÉZIER = 1;
constexpr csmChar SEGMENT_STEPPED = 2;
constexpr csmChar SEGMENT_INVERSE_STEPPED = 3;
inline csmInt32 CubismMotionJson::CountPoints() const {
  return GetRecoveredTotal(TOTAL_POINTS);
}
/* reconstruct the total numbers using the data. This kind of makes the total numbers in the json really pointless, but w/e ¯\_(ツ)_/¯
   If you trust user data absolutely you can turn off this check in the constructor.
   Algorithm mostly based on {2}, with additional documentation from {1}.
 */
void CubismMotionJson::RecoverTotals() {
    auto totalPointCount = 0;
    auto totalSegmentCount = 0;
    auto curves = _json->GetRoot()[Curves].GetVector();
    auto curveCount = curves->GetSize();
    // clang complains that this weird archaic iterator stuff is deprecated but this fits the style of the other modules in this codebase.
    for ( auto ite = curves->Begin(); ite != curves->End();) {
      auto curve = (*ite)->GetMap();

        for (auto ckeyref = curve->Begin(); ckeyref != curve->End();) {
            auto ckey = *ckeyref;
            if (ckey.First == Segments) {
                auto segments = ckey.Second->GetVector();
                auto segments_size = segments->GetSize();
                // »Each curve starts with the first point followed by the segment identifier. Therefore, the first segment identifier is the third number in the flat segments array.« {1}
                auto pos = 2u;
                totalPointCount += 1;
                
                while (pos < segments_size) {
                    auto curveType = segments->At(pos)->ToInt();
                    
                    // » A segment identifier is followed by 1 point in case of linear, stepped, and inverse stepped segments, that represents the end of the segment, or 3 point[s] in case of bézier segments « {1}
                    switch (curveType) {
                    case SEGMENT_LINEAR:
                        totalPointCount += 1;
                        totalSegmentCount += 1;
                        pos += 3;
                        break;
                    case SEGMENT_CUBIC_BÉZIER:
                        totalPointCount += 3;
                        totalSegmentCount += 1;
                        pos += 7;
                        break;
                    case SEGMENT_STEPPED:
                        totalPointCount += 1;
                        totalSegmentCount += 1;
                        pos += 3;
                        break;
                    case SEGMENT_INVERSE_STEPPED:
                        totalPointCount += 1;
                        totalSegmentCount += 1;
                        pos += 3;
                        break;
                    default:
                        std::ostringstream errmsg;
                        errmsg << "Invalid curve type " << curveType << std::endl;
                        throw std::invalid_argument("Invalid curve type ");
                    }
                }
            }
            ++ckeyref;
        }
        ++ite;
    }

recoveredTotals.reset(new std::tuple<csmInt32,csmInt32,csmInt32>{curveCount,totalSegmentCount,totalPointCount});
    
}

} // namespace Framework
} // namespace Cubism
} // namespace Live2D
// {1} https://github.com/Live2D/CubismSpecs/blob/ab04558655dfd4d8c2d529286124e274648c772d/FileFormats/motion3.json.md
// {2} https://gist.github.com/MizunagiKB/7fd109925e56db0ed88b77450cdea579
