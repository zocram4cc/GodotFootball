// PES's world object's table accessors, run in place. 0x140A48910 (anime) and 0x140A48A10 (man) answer the VA of the
// record an entry names, and fall back to entry 0 past the table's count, as PES does. GF's readers of the two tables
// run them on the world window.

#include <cstdint>

#include "onthepitch/pesworldrecord.hpp"
#include "pes/translated/translated.hpp"
#include "pes_state.hpp"

namespace pes {
namespace {

// A call's state over the world window: G's VA in rcx is the caller's job; the image global that holds G is a segment,
// and the stack is the window's own (kStackVa), so a call never reaches outside GF's bytes.
PesState NewCall(uint8_t* world) {
  PesState s;
  s.mem.window.base = kWorldWindowVa;
  s.mem.window.size = kWorldBytes;
  s.mem.window.rw = world;
  s.mem.segs.push_back(PesMem::Seg{kWorldGlobalVa, std::vector<uint8_t>(sizeof(uint64_t))});
  std::memcpy(s.mem.segs.back().bytes.data(), &kWorldWindowVa, sizeof(uint64_t));
  s.gpr[4] = kStackVa + kStackBytes;
  return s;
}

// rcx = G (the window's base VA), edx = the slot index. An int argument in edx zero-extends into rdx; the answer is rax.
uint64_t Answer(void (*accessor)(PesState&), uint8_t* world, int index) {
  PesState s = NewCall(world);
  s.gpr[1] = kWorldWindowVa;
  s.gpr[2] = static_cast<uint32_t>(index);
  accessor(s);
  return s.gpr[0];
}

uint8_t* InWindow(uint8_t* world, uint64_t va) {
  if (va < kWorldWindowVa || va >= kWorldWindowVa + kWorldBytes) return nullptr;
  return world + (va - kWorldWindowVa);
}

// PES's bearing 0x140439EF0 on the window: rcx = from, rdx = to, the heading in xmm0 (coincident reads 0, else the fold
// of atan2(to.x - from.x, to.z - from.z)). The points are read in place; the stack is the scratch's.
float RunBearing(uint8_t* world, uint64_t fromVa, uint64_t toVa) {
  PesState s = NewCall(world);
  s.gpr[1] = fromVa;
  s.gpr[2] = toVa;
  s.gpr[4] = kScratchVa + kScratchBytes;
  ::pes_fn_140439EF0(s);
  return s.x_f32(0);
}

}  // namespace

uint8_t* PesWorld::AnimeRecord(int slot) { return InWindow(Bytes(), Answer(&::pes_fn_140A48910, Bytes(), slot)); }

uint8_t* PesWorld::ManRecord(int slot) { return InWindow(Bytes(), Answer(&::pes_fn_140A48A10, Bytes(), slot)); }

bool PesWorld::Involved(int official) {
  uint8_t* frame = Block(official) + kOfficialFrameOffset;
  StoreU32(frame, kOfficialFrameIndexOffset, static_cast<uint32_t>(official));
  PesState s = NewCall(Bytes());
  s.gpr[1] = VaOf(frame);  // rcx = A0, the official's per-frame record
  ::pes_fn_140A3A690(s);
  return (s.gpr[0] & 0xFF) != 0;  // al
}

void PesWorld::BindSanction() {
  PesSanctionImage& z = image_->sanction;
  image_->tables.fieldInfo = SanctionVa(offsetof(PesSanctionImage, fieldInfo));
  for (int side = 0; side < kSideCount; ++side) {
    const std::size_t info = offsetof(PesSanctionImage, teamInfo) + static_cast<std::size_t>(side) * kTeamInfoBytes;
    SetEntry(WorldTable::kSideB, side, SanctionVa(info));
    uint8_t* team = z.teamInfo[side].data();
    StoreU32(team, kTeamInfoSideOffset, static_cast<uint32_t>(side));
    // ponytail: GF's squad place is the record index; PES's map is a permutation (0x140A3AF90 reads it), written by its
    // squad loader, which is not decoded.
    for (int place = 0; place < kSlotsPerSide; ++place)
      StoreU32(team, kTeamInfoPlaceMapOffset + sizeof(uint32_t) * place, static_cast<uint32_t>(place));
  }
  const uint64_t block = SanctionVa(offsetof(PesSanctionImage, analyzeBlock));
  const uint64_t events = SanctionVa(offsetof(PesSanctionImage, events));
  std::memcpy(z.analyze.data() + kAnalyzeBlockPtrOffset, &block, sizeof block);
  std::memcpy(z.analyze.data() + kAnalyzeEventsPtrOffset, &events, sizeof events);
  // The judge's ctx cell names A1, its +0x3C0 names JR: 0x1405B7470 reads the cell as **(judge+0x3B8).
  const uint64_t analyze = SanctionVa(offsetof(PesSanctionImage, analyze));
  std::memcpy(z.judgeCtx.data(), &analyze, sizeof analyze);
  const uint64_t ctx = SanctionVa(offsetof(PesSanctionImage, judgeCtx));
  const uint64_t result = SanctionVa(offsetof(PesSanctionImage, judgeResult));
  std::memcpy(z.judge.data() + kJudgeCtxPtrOffset, &ctx, sizeof ctx);
  std::memcpy(z.judge.data() + kJudgeResultPtrOffset, &result, sizeof result);
}

void PesWorld::SetFieldInfo(float halfLength_m, float halfWidth_m) {
  uint8_t* info = image_->sanction.fieldInfo.data();
  StoreF32(info, kFieldInfoHalfLengthOffset, halfLength_m);
  StoreF32(info, kFieldInfoHalfWidthOffset, halfWidth_m);
}

void PesWorld::SetPlayerInfo(int slot, uint32_t id, int yellows) {
  uint8_t* record = image_->sanction.teamInfo[slot / kSlotsPerSide].data() + kTeamInfoRecordsOffset +
                    static_cast<std::size_t>(slot % kSlotsPerSide) * kTeamInfoRecordBytes;
  record[kTeamInfoYellowsOffset] = static_cast<uint8_t>(yellows);
  StoreU32(record, kTeamInfoIdOffset, id);
}

void PesWorld::PushFoulEvent(const FoulEvent& event) {
  PesSanctionImage& z = image_->sanction;
  uint8_t* source = z.foulSource.data();
  std::memset(source, 0, kEventRecordBytes);
  StoreU32(source, kFoulEventKindOffset, kFoulEventKind);
  // The stamps at +4/+8 are not written: the tree does not read them and GF's judge has no played-frame word yet.
  StoreU32(source, kFoulEventPeriodOffset, event.periodState);
  StoreU32(source, kFoulEventFoulKindOffset, event.foulKind);
  StoreU32(source, kFoulEventCardOffset, event.cardLevel);
  StoreU32(source, kFoulEventSideOffset, event.side);
  source[kFoulEventInjuredOffset] = event.injured ? 1 : 0;
  PesState s = NewCall(Bytes());
  s.gpr[1] = SanctionVa(offsetof(PesSanctionImage, events) + kEventFoulRing * kEventRingStride);
  s.gpr[2] = SanctionVa(offsetof(PesSanctionImage, foulSource));
  ::pes_fn_140A3DAC0(s);
}

uint32_t PesWorld::DecideSanction() {
  PesState s = NewCall(Bytes());
  s.gpr[1] = SanctionVa(offsetof(PesSanctionImage, request));  // rcx = A0
  s.gpr[2] = SanctionVa(offsetof(PesSanctionImage, analyze));  // rdx = A1
  ::pes_fn_14047F170(s);
  StoreU32(image_->sanction.request.data(), kRequestSceneOffset, static_cast<uint32_t>(s.gpr[0]));  // eax
  return static_cast<uint32_t>(s.gpr[0]);
}

PesWorld::JudgeVerdict PesWorld::DecideJudge(int victim, int fouler) {
  uint8_t* judge = image_->sanction.judge.data();
  judge[kJudgePendingOffset] = 0;
  PesState s = NewCall(Bytes());
  s.gpr[1] = SanctionVa(offsetof(PesSanctionImage, judge));  // rcx = the judge
  s.gpr[2] = static_cast<uint32_t>(victim);                  // edx = A
  s.gpr[8] = static_cast<uint32_t>(fouler);                  // r8d = B
  ::pes_fn_1405B7470(s);
  JudgeVerdict verdict;
  verdict.pending = judge[kJudgePendingOffset] != 0;
  if (!verdict.pending) return verdict;
  verdict.level = LoadU32(judge, kJudgeLevelOffset);
  verdict.points = judge[kJudgePointsOffset];
  verdict.restart = LoadU32(judge, kJudgeRestartOffset);
  verdict.type = judge[kJudgeTypeOffset];
  return verdict;
}

void PesWorld::SetTeamLine(int team, float x) {
  StoreF32(image_->sanction.analyzeBlock.data(), kAnalyzeTeamLineOffset[team], x);
}

float PesWorld::Bearing(const uint8_t* from, const uint8_t* to) {
  return RunBearing(Bytes(), VaOf(from) + kManPosXOffset, VaOf(to) + kManPosXOffset);
}

float PesWorld::BearingOfPoints(float fromX, float fromZ, float toX, float toZ) {
  uint8_t* points = Bytes() + (kScratchVa - kWorldWindowVa);
  StoreF32(points, kPointXOffset, fromX);
  StoreF32(points, kPointZOffset, fromZ);
  StoreF32(points, kPointBytes + kPointXOffset, toX);
  StoreF32(points, kPointBytes + kPointZOffset, toZ);
  return RunBearing(Bytes(), kScratchVa, kScratchVa + kPointBytes);
}

}  // namespace pes
