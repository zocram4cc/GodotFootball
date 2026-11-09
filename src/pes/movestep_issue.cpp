// The anime interface calls every MOVESTEP entry makes around a clip issue; see movestep_issue.hpp. Every compare and store
// cites the instruction it comes from.
#include "pes/movestep_issue.hpp"

#include <array>
#include <cmath>

#include "pes/movestep_run.hpp"

namespace pes {
namespace {

constexpr uint32_t kMotionLimit = 0x1125;  // FUN_1406d8620 / the tail: a motion at or past it has no hit record
constexpr uint32_t kSlots = 22;            // a slot below 0x16 is a player
constexpr int kHitDistanceWord = 0x34;     // req+0xd0, written by FUN_1406d8620

}  // namespace

float AbilityWeight(MoveHost& host, bool da) {
  const RunConstants& k = host.RunConstantsView();
  const int own = host.ActionIndex();
  if (static_cast<uint32_t>(own) >= kSlots) return 0.0f;  // 0x1406e9fac CMP EAX,0x15 / JA
  float ability = static_cast<float>(host.AbilityByte(da ? kAbilityDa : kAbilityAcceleration));
  const int condition = host.Team().Condition(own);
  if (static_cast<uint32_t>(condition) <= 10u)  // 0x1406ea054 CMP AL,0xa / JA
    ability -= k.condition10 - static_cast<float>(condition);
  if (ability <= k.rec40) return 0.0f;
  if (ability >= k.rec99) return k.one;
  return (ability - k.rec40) / k.rec59;
}

float EntryPlayback(MoveHost& host) {
  // The flag every entry hands vt+0x30: the own slot is the match-up man (FUN_140a62960's answer == the own slot), then
  // vt+0x50 turns the weight into the playback.
  const bool matchUp = host.Team().IsMatchUpMan(host.ActionIndex());
  return TurnPlayback(host.PhysiqueG(), AbilityWeight(host, matchUp), host.PlaybackConstantsView());
}

float ClipHitSeconds(const MoveHost& host, int motion, float playback, int index) {
  if (static_cast<uint32_t>(motion) >= kMotionLimit) return 0.0f;
  const float one = host.RunConstantsView().one;  // DAT_14256ae1c: the rate both readers take
  int ticks = host.ClipTicks(ClipReader::kBlend, motion, one, 0);
  const int count = host.Motion(motion).CancelCount();  // FUN_1406afbc0(.., 5)
  if (count > 0) {
    uint32_t at = static_cast<uint32_t>(index);
    if (static_cast<uint32_t>(count) <= at) at = static_cast<uint32_t>(count) - 1u;
    ticks = host.ClipTicks(ClipReader::kCancel, motion, one, static_cast<int>(at));
  }
  return (static_cast<float>(ticks) / host.TicksPerSecond()) / playback;
}

// FUN_1406e7730 (+0xab8 vt+0x58): the clip's root offset at hit `index`, in metres, scaled by the physique scale. The blend
// record's offset is read first (FUN_1406af6d0, 0x1406e777a); a motion with cancel records replaces it with the cancel record
// at min(index, count - 1) (FUN_1406af9a0, 0x1406e77c5). A motion at or past the table reads 0 (0x1406e774a).
float ClipHitDistance(const MoveHost& host, int motion, float g, int index) {
  if (static_cast<uint32_t>(motion) >= kMotionLimit) return 0.0f;
  const float centi = host.RunConstantsView().centi;  // 0x1406e777f: DAT_14256ae18
  const MotionRecord& record = host.Motion(motion);
  std::array<float, 2> root = record.BlendRoot();
  const int count = record.CancelCount();
  if (count > 0) {
    uint32_t at = static_cast<uint32_t>(index);
    if (static_cast<uint32_t>(count) <= at) at = static_cast<uint32_t>(count) - 1u;  // 0x1406e77bd CMP EDI,EAX / JB
    root = record.CancelRoot(static_cast<int>(at));
  }
  const float x = root[0] * centi;  // xmm7 (0x1406e779c)
  const float z = root[1] * centi;  // xmm6 (0x1406e77a1)
  return std::sqrt(z * z + x * x) * g;  // 0x1406e77dd..0x1406e7809
}

void WriteIssue(MoveHost& host, MoveReq& req, const MotionRequest& issue) {
  using namespace moveword;
  if (static_cast<uint32_t>(issue.motion) < kMotionLimit) {
    req.SetF(kDwellLimit, ClipHitSeconds(host, issue.motion, issue.playback, 0));  // req+0x6c
    req.SetF(kHitDistanceWord, ClipHitDistance(host, issue.motion, host.PhysiqueG(), 0));
  } else {
    req.SetU(kDwellLimit, 0);
    req.SetU(kHitDistanceWord, 0);
  }
  req.SetF(kDwellLimit2, issue.duration);                              // req+0xb8
  req.SetF(kDuration, issue.duration);                                 // req+0xc8
  req.SetF(kPlayback, issue.playback);                                 // req+0xd4
  req.SetU(kMotion, static_cast<uint32_t>(issue.motion));              // req+0xc0
  req.SetU(kTransitionWord, static_cast<uint32_t>(issue.transition));  // req+0xd8
  req.SetF(kStartTime, issue.startTime);                               // req+0xcc
  req.SetU(kDwellElapsed, 0);                                          // req+0x68
  req.SetU(kDwellElapsed2, 0);                                         // req+0xb4
  req.SetByte(kMotionPending, 1);                                      // req+0xbc
  req.SetByte(kMirror, issue.mirror ? 1 : 0);                          // req+0xc4
}

void CopyHitWords(MoveReq& req, int motion) {
  using namespace moveword;
  const bool hits = static_cast<uint32_t>(motion) < kMotionLimit;
  req.SetU(kHitFrame, hits ? req.U(kDwellLimit) : 0u);        // req[0x28] = the clip's hit seconds (req[0x1b])
  req.SetU(kHitMotion, hits ? req.U(kHitDistanceWord) : 0u);  // req[0x27] = the hit distance (req[0x34])
}

}  // namespace pes
