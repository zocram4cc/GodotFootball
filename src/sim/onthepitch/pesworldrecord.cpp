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
void Bind(PesState& s, uint8_t* world) {
  s.mem.window.base = kWorldWindowVa;
  s.mem.window.size = kWorldBytes;
  s.mem.window.rw = world;
  s.mem.segs.push_back(PesMem::Seg{kWorldGlobalVa, std::vector<uint8_t>(sizeof(uint64_t))});
  std::memcpy(s.mem.segs.back().bytes.data(), &kWorldWindowVa, sizeof(uint64_t));
  s.gpr[4] = kStackVa + kStackBytes;
}

PesState NewCall(uint8_t* world) {
  PesState s;
  Bind(s, world);
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

void PesWorld::BindCall(::PesState& call) { Bind(call, Bytes()); }

PesWorld::~PesWorld() = default;

PesWorld::ClearAnswer PesWorld::RunClearUnit(int side, uint32_t carrierSlot) {
  uint8_t* unit = UnitObject(side);
  StoreU32(Carrier(side), kBpUnitInfoOffset + kUnitSlotOffset, carrierSlot);
  std::memset(unit + kUnitObjectBytes, 0, kAnswerBytes);
  PesState s = NewCall(Bytes());
  s.gpr[2] = CarrierVa(side) + kClearAnswerOffset;     // rdx: the answer record
  s.gpr[8] = CarrierVa(side) + kUnitObjectOffset;      // r8: the unit
  ::pes_fn_142109EB0(s);
  ClearAnswer answer;
  answer.type = LoadU32(Carrier(side), kClearAnswerOffset);
  answer.angle_deg = LoadF32(Carrier(side), kClearAnswerOffset + kAnswerAngleOffset);
  return answer;
}

bool PesWorld::RunDangerBuilder(int side, const PesDangerExternals& ext) {
  if (!heap_) heap_ = std::make_unique<PesHeap>();
  PesState s = NewCall(Bytes());
  heap_->Attach(s);
  for (const PesTrapSegment& seg : imageTables_.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, seg.bytes});
  for (const PesTrapSegment& seg : ext.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, seg.bytes});
  // The call frame of 0x142110D30's call: the return address, then (past the 0x20 shadow) the fifth argument (the carrier
  // team's record, unused by the builder) and the sixth (BP+0x18, the refresh flags).
  const uint64_t bp = CarrierVa(side);
  const uint64_t entry = kStackVa + kStackBytes - kDangerFrameBytes;
  s.wr(entry, 0, 8);
  s.wr(entry + kDangerArg5Offset, SideVa(side), 8);
  s.wr(entry + kDangerArg6Offset, LoadU32(Carrier(side), kBpFlagsOffset), 4);
  s.gpr[4] = entry;
  s.gpr[1] = bp + kBpImageAOffset;       // rcx: imageA
  s.gpr[2] = CtxVa(side);                // rdx: the side's ctx (+0x18 MA, +0xE0 E)
  s.gpr[8] = bp + kBpUnitInfoOffset;     // r8: the unit info
  s.gpr[9] = bp + kBpStateOffset;        // r9: the state
  s.mem.wlog = ext.writeLog;
  ::pes_fn_142119390(s);
  return ClearDanger(side);
}

void PesWorld::SetUnitList(int side, const std::vector<WalkEntry>& list) {
  uint8_t* l = Carrier(side) + kBpListOffset;
  std::memset(l, 0, kListBytes);
  std::memset(l + kListRankOffset, kRankUnlisted, kWalkKindCount);
  StoreU32(l, 0, static_cast<uint32_t>(list.size()));
  for (std::size_t position = 0; position < list.size(); ++position) {
    const WalkEntry& entry = list[position];
    StoreU32(l, sizeof(uint32_t) * (1 + position), entry.kind);
    l[kListRankOffset + entry.kind] = static_cast<uint8_t>(position);
    std::memcpy(l + kListRecordOffset + kListRecordStride * entry.kind, entry.record.data(), kListRecordStride);
  }
}

void PesWorld::SetCarrierProfile(int side, uint32_t slot, uint64_t skillMask, uint8_t heightStatCm) {
  uint8_t* record = TeamInfoRecord(side);
  StoreU32(record, kSideIdOffset, static_cast<uint32_t>(side));
  for (uint32_t k = 0; k < kSlotsPerSide; ++k) StoreU32(record, kSideInfoIndexOffset + sizeof(uint32_t) * k, k);
  const std::size_t info = kSidePlayerInfoOffset + kSidePlayerInfoStride * (slot % kSlotsPerSide);
  std::memcpy(record + info + kSideSkillMaskOffset, &skillMask, sizeof skillMask);
  // The walk reads the taker from G's FIRST side table (0x14210F110: FUN_140A48A30(G, side) -> +0x7C), the side record, not the
  // team-info table the skill reads go through.
  // ponytail: the designated taker is the carrier: a set-piece decider is the taker; the writer is the restart set-up, undecoded
  StoreU32(SideRecord(side), kSideKickerOffset, slot);
  Block(static_cast<int>(slot))[kSlotRecordOffset + kSlotByteA1Offset] = heightStatCm;
}

void PesWorld::RunCarrierUpdate(int side, uint32_t slot, const PesDangerExternals& ext) {
  if (!heap_) heap_ = std::make_unique<PesHeap>();
  const uint64_t bp = CarrierVa(side);
  const auto call = [&](void (*fn)(::PesState&), uint64_t rcx, uint64_t r8) {
    PesState s = NewCall(Bytes());
    heap_->Attach(s);
    for (const PesTrapSegment& seg : imageTables_.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, seg.bytes});
    for (const PesTrapSegment& seg : ext.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, seg.bytes});
    const uint64_t entry = kStackVa + kStackBytes - kDangerFrameBytes;
    s.wr(entry, 0, 8);
    s.gpr[4] = entry;
    s.gpr[1] = rcx;
    s.gpr[2] = CtxVa(side);
    s.gpr[8] = r8;
    s.mem.wlog = ext.writeLog;
    fn(s);
  };
  // The driver 0x142110C90 resets the block (0x14210E5F0 -> the state's constructor 0x14063A3B0) when its unit goes inactive: a
  // new carrier starts a fresh state, whose counters then run from zero.
  if (!stateReady_[side] || LoadU32(Carrier(side), kBpUnitInfoOffset + kUnitSlotOffset) != slot) {
    call(&::pes_fn_14063A3B0, bp + kBpStateOffset, 0);
    StoreU32(Carrier(side), kBpChosenKindOffset, kWalkNoKind);  // 0x14210E6B8
    stateReady_[side] = true;
  }
  call(&::pes_fn_14063C9F0, bp + kBpUnitInfoOffset, slot);                      // FUN_14063C9F0(unit info, ctx, slot)
  call(&::pes_fn_14063A490, bp + kBpStateOffset, bp + kBpUnitInfoOffset);       // FUN_14063A490(state, ctx, unit info)
  // The driver 0x142110D30 sets the previous kind before it calls the walk: the none word (0x142110F34), then the Dribble default
  // (0x142110F88) while the state's first word is 0.
  StoreU32(Carrier(side), kBpChosenKindOffset,
           LoadU32(Carrier(side), kBpStateOffset) == 0 ? kBpDefaultKind : kWalkNoKind);
  // ponytail: BP+0x18 (the builders' refresh flags) is all five bits in every capture (0x142117370 writes it, translated but
  // not run here: its inputs are the plan block's); a constant until that block is.
  StoreU32(Carrier(side), kBpFlagsOffset, kBpRefreshAll);
}

uint32_t PesWorld::RunWalk(int side, const PesDangerExternals& ext) {
  if (!heap_) heap_ = std::make_unique<PesHeap>();
  PesState s = NewCall(Bytes());
  heap_->Attach(s);
  for (const PesTrapSegment& seg : imageTables_.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, seg.bytes});
  for (const PesTrapSegment& seg : ext.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, seg.bytes});
  const uint64_t entry = kStackVa + kStackBytes - kDangerFrameBytes;
  s.wr(entry, 0, 8);
  s.gpr[4] = entry;
  s.gpr[1] = CarrierVa(side);                    // rcx: BP
  s.gpr[2] = CarrierVa(side) + kUnitObjectOffset;  // rdx: {unit info VA, ctx VA}
  s.mem.wlog = ext.writeLog;
  ::pes_fn_14210F110(s);
  return static_cast<uint32_t>(s.gpr[0]);
}

PesTrapResult PesWorld::RunTrapKind0(const PesTrapRequest& request, const PesTrapExternals& ext) {
  PesState s = NewCall(Bytes());
  s.mem.flat = ext.image;
  s.mem.flat_base = kImageBaseVa;
  s.mem.flat_size = ext.imageBytes;
  for (const PesTrapSegment& seg : ext.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, seg.bytes});
  for (const auto& [va, value] : ext.globals) s.mem.wr(va, value, 8);

  // The dispatcher 0x1407A01C0 (kind <= 0): two bytes of the request go into the action, the first word of the request is
  // copied to the dispatcher's frame (r9), and the chain takes the other fields as stack arguments (6th: the out word's pointer).
  const uint64_t entry = kStackVa + kStackBytes - kTrapFrameBytes;  // rsp at the chain's entry: the return address slot
  const uint64_t action = PVa() + kPActionOffset;
  const auto word = [&request](std::size_t off) { return LoadU32(request.bytes.data(), off); };
  const auto byte = [&request](std::size_t off) { return static_cast<uint64_t>(request.bytes[off]); };
  s.wr(action + kActionByte3DOffset, byte(kReqByte1DOffset), 1);
  s.wr(action + kActionByte3EOffset, byte(kReqByte21Offset), 1);
  s.wr(entry, 0, 8);
  s.wr(entry + kFrameRequestCopyOffset, word(kReqWord0Offset), 4);
  s.wr(entry + kFrameArg5Offset, word(kReqWord1Offset), 4);
  s.wr(entry + kFrameOutPtrOffset, entry + kFrameOutWordOffset, 8);
  s.wr(entry + kFrameArg7Offset, word(kReqWord3Offset), 4);
  s.wr(entry + kFrameArg8Offset, 0, 4);
  s.wr(entry + kFrameArg9Offset, byte(kReqByte1EOffset), 1);
  s.wr(entry + kFrameArg10Offset, byte(kReqByte1FOffset), 1);
  s.wr(entry + kFrameArg11Offset, byte(kReqByte20Offset), 1);
  s.wr(entry + kFrameArg12Offset, byte(kReqByte22Offset), 1);
  s.wr(entry + kFrameArg13Offset, byte(kReqFlagAOffset), 1);
  s.wr(entry + kFrameOutWordOffset, 0, 4);
  s.gpr[4] = entry;
  s.gpr[1] = action;                          // rcx: the Trap action
  s.gpr[2] = PVa();                           // rdx: P
  s.gpr[8] = BallVa();                        // r8: the ball record
  s.gpr[9] = entry + kFrameRequestCopyOffset; // r9: the dispatcher's copy of the request
  s.mem.wlog = ext.writeLog;
  ::pes_fn_14079A470(s);
  PesTrapResult result;
  result.issued = (s.gpr[0] & 0xFF) != 0;
  result.out = static_cast<uint32_t>(s.mem.rd(entry + kFrameOutWordOffset, 4));
  return result;
}

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
  for (int side = 0; side < kSideCount; ++side) {
    uint8_t* team = TeamInfoRecord(side);
    StoreU32(team, kSideIdOffset, static_cast<uint32_t>(side));
    // ponytail: GF's squad place is the record index; PES's map is a permutation (0x140A3AF90 reads it), written by its
    // squad loader, which is not decoded.
    for (int place = 0; place < kSlotsPerSide; ++place)
      StoreU32(team, kSideInfoIndexOffset + sizeof(uint32_t) * place, static_cast<uint32_t>(place));
  }
  const uint64_t block = SanctionVa(offsetof(PesSanctionImage, analyzeBlock));
  const uint64_t events = EventsVa();  // A1+0xE0 is the match's own store: the ball events the judge reads are the ones it wrote
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
  uint8_t* info = image_->pitch.bytes.data();
  StoreF32(info, kFieldInfoHalfLengthOffset, halfLength_m);
  StoreF32(info, kFieldInfoHalfWidthOffset, halfWidth_m);
}

void PesWorld::SetPlayerInfo(int slot, uint32_t id, int yellows) {
  uint8_t* record = TeamInfoRecord(slot / kSlotsPerSide) + kSidePlayerInfoOffset +
                    static_cast<std::size_t>(slot % kSlotsPerSide) * kSidePlayerInfoStride;
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
  s.gpr[1] = EventsVa() + kEventFoulRing * kEventRingStride;
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

PesWorld::JudgeVerdict PesWorld::DecideContact(const ContactFacts& facts) {
  constexpr std::size_t kStagedBytes = kAnimeTackleKindOffset + 4;
  uint8_t* animeA = Block(facts.victim);
  uint8_t* animeB = Block(facts.fouler);
  uint8_t savedA[kStagedBytes], savedB[kStagedBytes];
  std::memcpy(savedA, animeA, kStagedBytes);
  std::memcpy(savedB, animeB, kStagedBytes);
  std::memset(animeA, 0, kStagedBytes);
  std::memset(animeB, 0, kStagedBytes);
  const auto setFlag = [](uint8_t* anime, int bit) {
    uint64_t flags;
    std::memcpy(&flags, anime + kAnimeFlagsOffset, sizeof flags);
    flags |= uint64_t{1} << bit;
    std::memcpy(anime + kAnimeFlagsOffset, &flags, sizeof flags);
  };
  animeA[kAnimeActionOffset] = facts.victimFell ? kActionFall : facts.victimStaggered ? kActionStagger : 0;
  if (facts.victimFace) setFlag(animeA, kFlagFace);
  animeB[kAnimeActionOffset] = facts.foulerAction;
  // 0x140A63DA0: a challenge (0xB..0xE) has played the ball with flag 15 set, any other action with flag 16.
  if (facts.playedBall) {
    const bool challenge = facts.foulerAction >= kActionSlide && facts.foulerAction <= kActionBlock;
    setFlag(animeB, challenge ? kFlagPlayedBall : kFlagChallenge);
  }
  uint8_t* slotB = SlotRecord(facts.fouler);
  slotB[kSlotIntentOffset] = facts.intent ? 1 : 0;
  slotB[kSlotFairOffset] = 0;
  StoreF32(slotB, kSlotApproachAngleOffset, facts.targeted ? facts.approachAngle_deg : 0.0f);
  StoreU32(slotB, kSlotTargetValidOffset, facts.targeted ? 1u : 0u);
  StoreU32(slotB, kSlotTargetIdOffset, facts.targeted ? static_cast<uint32_t>(facts.victim) : 0xFFFFFFFFu);
  StoreU32(slotB, kSlotForceTargetOffset, facts.targeted ? static_cast<uint32_t>(facts.victim) : 0xFFFFFFFFu);
  StoreF32(slotB, kSlotTargetPointOffset, facts.targetX);
  StoreF32(slotB, kSlotTargetPointOffset + sizeof(float), facts.targetY);
  StoreF32(slotB, kSlotTargetPointOffset + 2 * sizeof(float), facts.targetZ);
  const JudgeVerdict verdict = DecideJudge(facts.victim, facts.fouler);
  std::memcpy(animeA, savedA, kStagedBytes);
  std::memcpy(animeB, savedB, kStagedBytes);
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
