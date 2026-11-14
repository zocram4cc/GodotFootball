// PES's world object's table accessors, run in place. 0x140A48910 (anime) and 0x140A48A10 (man) answer the VA of the
// record an entry names, and fall back to entry 0 past the table's count, as PES does. GF's readers of the two tables
// run them on the world window.

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "onthepitch/pesworldrecord.hpp"
#include "onthepitch/pesball.hpp"
#include "pes/translated/pes_dispatch.hpp"
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

// PES's man accessor 0x140A48A10 answers the table entry unchecked (a slot past the table reads entry 0), and PesWorld binds every
// entry (BindSlot, BindOfficial): an answer outside the window is a table nobody bound, never a record to run on. Stop there rather
// than hand the callers a null they would read through.
uint8_t* BoundRecord(uint8_t* world, uint64_t va) {
  uint8_t* record = InWindow(world, va);
  if (record == nullptr) {
    std::fprintf(stderr, "PesWorld: the man table answered 0x%llx, outside the window: the slot was never bound\n",
                 static_cast<unsigned long long>(va));
    std::abort();
  }
  return record;
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
  AttachImageTables(s);
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

void PesWorld::SetCarrierProfile(int side, uint32_t slot, uint64_t skillMask) {
  uint8_t* record = TeamInfoRecord(side);
  StoreU32(record, kSideIdOffset, static_cast<uint32_t>(side));
  for (uint32_t k = 0; k < kSlotsPerSide; ++k) StoreU32(record, kSideInfoIndexOffset + sizeof(uint32_t) * k, k);
  const std::size_t info = kSidePlayerInfoOffset + kSidePlayerInfoStride * (slot % kSlotsPerSide);
  std::memcpy(record + info + kSideSkillMaskOffset, &skillMask, sizeof skillMask);
  // The walk reads the taker from G's FIRST side table (0x14210F110: FUN_140A48A30(G, side) -> +0x7C), the side record, not the
  // team-info table the skill reads go through.
  // ponytail: the designated taker is the carrier: a set-piece decider is the taker; the writer is the restart set-up, undecoded
  StoreU32(SideRecord(side), kSideKickerOffset, slot);
}

// FUN_140A58F00(record, row): five 16-byte copies of row[0x00..0x50) to record +0x70..+0xC0, called by FUN_140922BC0 with the roster
// entry's row (entry +0x38). The caller's other stores (FUN_140922DD0, record +0x64, FUN_140A592B0) are not in GF's window.
void PesWorld::LoadRosterRow(int slot, const std::array<uint8_t, pes::kRosterRowBytes>& row) {
  std::memcpy(Block(slot) + kSlotRecordOffset + kSlotAbilityOffset, row.data(), row.size());
}

void PesWorld::StoreLiveAbility(int slot, uint32_t id, uint8_t base, const AbilityCells& cells) {
  uint8_t* block = Block(slot) + kSlotRecordOffset + kSlotAbilityOffset;
  block[id] = base;
  // 0x14092B849-0x14092B86D: the cells' product in PES's order (A2, A1, A3, A4, A5), 0x14092B890/0x14092B894: the base times it,
  // truncated (CVTTSS2SI) and read as a byte.
  const float product = cells.a2 * cells.a1 * cells.a3 * cells.a4 * cells.a5;
  const uint8_t value = static_cast<uint8_t>(static_cast<int64_t>(static_cast<float>(base) * product));
  if (value == block[id]) return;  // 0x14092B8B7: an unchanged byte is not written
  // 0x14092B8CF: the clamp bounds 0x1408CAF60(&lo, &hi, id) (lo at rcx, hi at rdx), answering 0 for an id past the table.
  PesState s = NewCall(Bytes());
  const uint64_t loVa = kScratchVa, hiVa = kScratchVa + 1;
  s.gpr[1] = loVa;
  s.gpr[2] = hiVa;
  s.gpr[8] = id;
  ::pes_fn_1408CAF60(s);
  if ((s.gpr[0] & 0xFF) == 0) return;  // 0x14092B8F3: the byte stays the roster's
  const uint8_t lo = *InWindow(Bytes(), loVa), hi = *InWindow(Bytes(), hiVa);
  block[id] = value > hi ? hi : (value < lo ? lo : value);  // 0x14092B8DD-0x14092B8F1
}

void PesWorld::RunCarrierUpdate(int side, uint32_t slot, const PesDangerExternals& ext) {
  if (!heap_) heap_ = std::make_unique<PesHeap>();
  const uint64_t bp = CarrierVa(side);
  const auto call = [&](void (*fn)(::PesState&), uint64_t rcx, uint64_t r8) {
    PesState s = NewCall(Bytes());
    heap_->Attach(s);
    AttachImageTables(s);
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
  // The driver 0x142110C90 resets the block (0x14210E5F0: the state's constructor 0x14063A3B0, the previous kind 0x19, the proposal
  // records, the carrier manager and every kind object) when its unit goes inactive: a new carrier starts a fresh block, whose
  // counters then run from zero. The first call builds the block (0x14210D320, whose last call is that reset).
  if (!constructed_[side]) {
    RunCarrierConstruct(side, ext);
    stateReady_[side] = true;
  } else if (!stateReady_[side] || LoadU32(Carrier(side), kBpUnitInfoOffset + kUnitSlotOffset) != slot) {
    call(&::pes_fn_14210E5F0, bp, 0);
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
  AttachImageTables(s);
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

void PesWorld::RunCarrierConstruct(int side, const PesDangerExternals& ext) {
  if (!heap_) heap_ = std::make_unique<PesHeap>();
  PesState s = NewCall(Bytes());
  heap_->Attach(s);
  AttachImageTables(s);
  for (const PesTrapSegment& seg : ext.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, seg.bytes});
  const uint64_t entry = kStackVa + kStackBytes - kTurnFrameBytes;
  s.wr(entry, 0, 8);
  s.gpr[4] = entry;
  s.gpr[1] = CarrierVa(side);  // rcx: BP
  s.gpr[2] = static_cast<uint32_t>(side);
  s.mem.wlog = ext.writeLog;
  ::pes_fn_14210D320(s);
  constructed_[side] = true;
}

void PesWorld::RunTurnReference(int side, const PesDangerExternals& ext) {
  if (!heap_) heap_ = std::make_unique<PesHeap>();
  const uint64_t bp = CarrierVa(side);
  PesState s = NewCall(Bytes());
  heap_->Attach(s);
  AttachImageTables(s);
  for (const PesTrapSegment& seg : ext.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, seg.bytes});
  const uint64_t entry = kStackVa + kStackBytes - kTurnFrameBytes;
  s.wr(entry, 0, 8);
  s.wr(entry + kTurnArg5Offset, bp + kBpImageAOffset, 8);  // imageA
  s.wr(entry + kTurnArg6Offset, bp + kBpPlanOffset, 8);    // the plan record
  s.wr(entry + kTurnArg7Offset, SideVa(side), 8);          // 0x140A48A30(G, side): the side record
  s.wr(entry + kTurnArg8Offset, bp + kBpProposalOffset, 8);
  s.gpr[4] = entry;
  s.gpr[1] = bp + kBpManagerOffset;
  s.gpr[2] = CtxVa(side);
  s.gpr[8] = bp + kBpUnitInfoOffset;
  s.gpr[9] = bp + kBpStateOffset;
  s.mem.wlog = ext.writeLog;
  ::pes_fn_14211D710(s);
}

void PesWorld::RunDribbleProposal(int side, uint32_t slot, const PesDangerExternals& ext) {
  // ponytail: the side record's T+0x10, T+0x2C and T+0x30 (the last Cross carrier and the two frame counters the Cross unit's start 0x14211AB10
  // gates on) stay 0 in the window. Live PES holds them (pid 2254416: T+0x10 = 18, T+0x2C/T+0x30 = 451/347 and rising per frame on one side);
  // their writer is not decoded (docs/ports/pes-port-TeamTick5.md, open item 2), so GF's Cross start has no cool-down.
  RunTurnReference(side, ext);  // 0x14211D710: ticks the carrier manager, writes BP+0x3380
  uint8_t* bp = Carrier(side);
  const uint64_t bpVa = CarrierVa(side);
  const uint64_t top = kStackVa + kStackBytes;
  const uint64_t stepInput = top - kStepInputBytes;      // S
  const uint64_t stepObject = top - kStepScratchBytes;   // the 72-byte object 0x14211ED10 fills, below S (16-aligned)
  const uint64_t entry = stepObject - kTurnFrameBytes;   // the call frame below both
  // One translated call on the window: the registers PES's driver passes, the stack arguments past the 0x20 shadow.
  const auto call = [&](void (*fn)(::PesState&), uint64_t rcx, uint64_t rdx, uint64_t r8, uint64_t r9, uint64_t arg5 = 0,
                        uint64_t arg6 = 0, uint64_t arg7 = 0, uint64_t arg8 = 0) {
    PesState s = NewCall(Bytes());
    heap_->Attach(s);
    AttachImageTables(s);
    for (const PesTrapSegment& seg : ext.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, seg.bytes});
    s.wr(entry, 0, 8);
    s.wr(entry + kTurnArg5Offset, arg5, 8);
    s.wr(entry + kTurnArg6Offset, arg6, 8);
    s.wr(entry + kTurnArg7Offset, arg7, 8);
    s.wr(entry + kTurnArg8Offset, arg8, 8);
    s.gpr[4] = entry;
    s.gpr[1] = rcx;
    s.gpr[2] = rdx;
    s.gpr[8] = r8;
    s.gpr[9] = r9;
    s.mem.wlog = ext.writeLog;
    fn(s);
    return static_cast<uint32_t>(s.gpr[0]);
  };
  PesState in = NewCall(Bytes());  // S: the slot, the output record BP+0x4258 and the ctx (CONFIRMED on seq16184)
  for (std::size_t off = 0; off < kStepScratchBytes; off += 8) in.wr(stepObject + off, 0, 8);
  in.wr(stepInput + kStepInputSlotOffset, slot, 4);
  in.wr(stepInput + kStepInputOutputOffset, bpVa + kBpStepOutputOffset, 8);
  in.wr(stepInput + kStepInputCtxOffset, CtxVa(side), 8);
  call(&::pes_fn_14211ED10, stepObject, stepInput, bpVa + kBpUnitInfoOffset, bpVa + kBpStateOffset, bpVa + kBpImageAOffset,
       bpVa + kBpPlanOffset, bpVa + kBpManagerOffset, SideVa(side));
  // The driver goes on only while the carrier is in neither of the two flag classes (0x140A6E320 of the anime class byte and of
  // the slot record's word +0x14); otherwise it runs 0x14210DD60.
  // ponytail: 0x14210DD60 (the flagged carrier's branch) is untranslated: GF leaves the proposal as it is for such a carrier.
  const int8_t animeClass = static_cast<int8_t>(AnimeRecord(static_cast<int>(slot))[1]);
  if ((call(&::pes_fn_140A6E320, static_cast<uint64_t>(static_cast<int64_t>(animeClass)), 0, 0, 0) & 0xFF) != 0 ||
      (call(&::pes_fn_140A6E320, LoadU32(Block(static_cast<int>(slot)) + kSlotRecordOffset, kSlotClassWordOffset), 0, 0, 0) & 0xFF) != 0)
    return;
  StoreU32(bp, kBpChosenKindOffset, kWalkNoKind);
  call(&::pes_fn_142119D20, bpVa + kBpAdoptedOffset, 0, 0, 0);
  if (LoadU32(bp, kBpStateOffset) != 0) return;
  const uint32_t kind = (call(&::pes_fn_140A638C0, CtxVa(side), slot, 0, 0) & 0xFF) != 0 ? kProposalKindTaker : kBpDefaultKind;
  call(&::pes_fn_1421102D0, bpVa, kind, stepObject, bpVa + kBpProposalOffset);
  std::memcpy(bp + kBpAdoptedOffset, bp + kBpProposalOffset, kProposalRecordBytes);
  StoreU32(bp, kBpChosenKindOffset, kBpDefaultKind);
}

// The chain's externals on a call: the image, the loader segments (lent: moved in, handed back by ReturnSegments, so the 4.9 MB is
// never copied) and the image globals that point at them. Returns how many segments the call held before.
static std::size_t LendExternals(PesState& s, PesTrapExternals& ext) {
  s.mem.flat = ext.image;
  s.mem.flat_base = ext.imageBase;
  s.mem.flat_size = ext.imageBytes;
  const std::size_t lent = s.mem.segs.size();
  for (PesTrapSegment& seg : ext.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, std::move(seg.bytes)});
  for (const auto& [va, value] : ext.globals) s.mem.wr(va, value, 8);
  return lent;
}

static void ReturnSegments(PesState& s, PesTrapExternals& ext, std::size_t lent) {
  for (std::size_t i = 0; i < ext.segments.size(); ++i) ext.segments[i].bytes = std::move(s.mem.segs[lent + i].bytes);
}

PesTrapResult PesWorld::RunTrapKind0(int slot, const PesTrapRequest& request, PesTrapExternals& ext) {
  PesState s = NewCall(Bytes());
  const std::size_t lent = LendExternals(s, ext);

  // The dispatcher 0x1407A01C0 (kind <= 0): two bytes of the request go into the action, the first word of the request is
  // copied to the dispatcher's frame (r9), and the chain takes the other fields as stack arguments (6th: the out word's pointer).
  const uint64_t entry = kStackVa + kStackBytes - kTrapFrameBytes;  // rsp at the chain's entry: the return address slot
  const uint64_t action = PVa(slot) + kPActionOffset;
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
  s.gpr[2] = PVa(slot);                       // rdx: P
  s.gpr[8] = BallVa();                        // r8: the ball record
  s.gpr[9] = entry + kFrameRequestCopyOffset; // r9: the dispatcher's copy of the request
  s.mem.wlog = ext.writeLog;
  ::pes_fn_14079A470(s);
  ReturnSegments(s, ext, lent);
  PesTrapResult result;
  result.issued = (s.gpr[0] & 0xFF) != 0;
  result.out = static_cast<uint32_t>(s.mem.rd(entry + kFrameOutWordOffset, 4));
  return result;
}

// The ActionPassGet tick's window state: the constructed object's rows, built once per slot from
// pespassgetobject_stores.inc (PES's constructor 0x14091DAB0, gen_passgetobject.py). The tick keeps its route state in the
// object between calls, so a replay's overlay (the capture's object bytes) must be applied on a constructed object, not over
// zero bytes: an overlay-only run would lose the vtable cells and every pointer into the object.
namespace {
void BuildPassGetObject(pes::PesWorld& world, int slot) {
  uint8_t* object = world.PassGetObject(slot);
  for (const PassGetStore& row : kPassGetObjectStores) {
    // kind 1: a pointer into the object itself (the route objects' vtable cells); PES's constructor wrote its own VA there.
    const uint64_t value = row.kind == 1 ? pes::PesWorld::PassGetVa(slot) + row.value : row.value;
    std::memcpy(object + row.offset, &value, sizeof value);
  }
}
}  // namespace

void PesWorld::SetDispatchThrowStatic(bool on) { pes_dispatch_throw_handler = on ? [](uint64_t) {} : nullptr; }

// The tick on the window for `slot`: rcx = the object, rdx = pc, r8 = R, r9 = the u32 the caller passes by pointer.
// The per-slot objects are constructed once (BuildPassGetObject over the slot's bytes); every run after that reuses the object's state.
void PesWorld::ConstructPassGetObject(int slot) { BuildPassGetObject(*this, slot); }
void PesWorld::AttachImageRun(uint64_t va, std::vector<uint8_t> bytes) {
  imageRuns_.push_back({va, std::move(bytes)});
}


void PesWorld::RunPassGetTick(int slot, PesTrapExternals& ext) {
  if (LoadU64(PassGetObject(slot), 0) == 0) BuildPassGetObject(*this, slot);  // the vtable word is the constructor's first store
  PesState s = NewCall(Bytes());
  for (const ImageRun& run : imageRuns_) s.mem.img.push_back({run.va, run.bytes});
  const std::size_t lent = LendExternals(s, ext);
  const std::size_t entry = kStackVa + kStackBytes - 0x40;  // the call's frame: the return address slot
  s.wr(entry, 0, 8);
  s.gpr[4] = entry;
  s.gpr[1] = PassGetVa(slot);  // rcx: the action object
  s.gpr[2] = PassGetPcVa(slot);  // rdx: pc
  s.gpr[8] = PassGetRVa(slot);  // r8: the command record R
  s.gpr[9] = PassGetArgVa(slot);  // r9: the u32 the caller passes by pointer
  s.mem.wlog = ext.writeLog;
  s.mem.readLog = ext.readLog;
  ::pes_fn_14094E0E0(s);
  ReturnSegments(s, ext, lent);
}

PesPassGetResult PesWorld::RunPassGetAction(int slot, uint32_t actionId, PesTrapExternals& ext) {
  const int side = slot >= kSlotsPerSide ? 1 : 0;
  uint8_t* pc = PassGetPc(slot);
  std::memset(pc, 0, kPassGetPcBytes);
  StoreU32(pc, kPcIdOffset, static_cast<uint32_t>(slot));
  StoreU64(pc, kPcCtxOffset, CtxVa(side));
  StoreU64(pc, kPcPOffset, PVa(slot));
  uint8_t* arg = PassGetObject(slot) + PassGetArgVa(slot) - PassGetVa(slot);
  StoreU32(arg, 0, actionId);
  RunPassGetTick(slot, ext);
  // R is the AI's command record: it keeps what a tick did not write (the target, the words) until the next tick overwrites it, as
  // PES's A+0x874.. does between thinks.
  const uint8_t* r = PassGetR(slot);
  PesPassGetResult out;
  for (int k = 0; k < 3; ++k) std::memcpy(&out.target[k], r + kRTargetOffset + 4 * k, sizeof(float));
  out.commandId = LoadU32(r, kRCommandIdOffset);
  out.moveKind = LoadU32(r, kRMoveKindOffset);
  out.precise = LoadU32(r, kRPreciseOffset);
  out.speedClass = LoadU32(r, kRSpeedClassOffset);
  out.speedUse = r[kRSpeedUseOffset] != 0;
  std::memcpy(&out.speedKmh, r + kRSpeedKmhOffset, sizeof(float));
  out.routeResult = LoadU32(PassGetObject(slot), kPassGetResultOffset);
  return out;
}

uint8_t PesWorld::RunAbilityByte(int slot, uint32_t id) {
  uint8_t* p = P(slot);
  p[kPByte2458Offset] = static_cast<uint8_t>(slot);
  StoreU64(p, kPInterfaceVtable0Offset, kPInterfaceVtableVa);
  PesState s = NewCall(Bytes());
  AttachImageTables(s);
  s.gpr[1] = PVa(slot);  // rcx: P
  s.gpr[2] = id;         // edx: the ability id
  ::pes_fn_1406E6660(s);
  return static_cast<uint8_t>(s.gpr[0]);
}

bool PesWorld::RunEntryAim(int slot, int side, uint32_t playState) {
  uint8_t* p = P(slot);
  p[kPByte2458Offset] = static_cast<uint8_t>(slot);
  StoreU64(p, kPInterfaceVtable0Offset, kPInterfaceVtableVa);
  StoreU64(p, kPPtr3AC0Offset, CtxVa(side));
  PeriodRecord& period = Period();
  const uint32_t saved = period.playState;
  period.playState = playState;
  PesState s = NewCall(Bytes());
  AttachImageTables(s);
  s.gpr[1] = PVa(slot);  // rcx: P
  ::pes_fn_1406BA7D0(s);
  period.playState = saved;
  return (s.gpr[0] & 0xFF) != 0;
}

PesTickResult PesWorld::RunTrapTick(int slot, PesTrapExternals& ext) {
  PesState s = NewCall(Bytes());
  const std::size_t lent = LendExternals(s, ext);
  // The caller (0x1406E5FC0) hands the tick the action (rcx), P (rdx) and a pointer to a slot that holds its frame F (r8); F's
  // +0x870 is the ball record (the phase handlers read the ball's flags through it).
  const uint64_t entry = kStackVa + kStackBytes - kTrapFrameBytes;
  uint8_t* frame = TickFrame();
  StoreU64(frame, kFrameBallOffset, BallVa());
  StoreU64(frame, kFrameXPtrOffset, TickFrameXVa());
  s.wr(entry, 0, 8);
  s.wr(entry + kTickEntryFrameSlotOffset, TickFrameVa(), 8);
  s.gpr[4] = entry;
  s.gpr[1] = PVa(slot) + kPActionOffset;
  s.gpr[2] = PVa(slot);
  s.gpr[8] = entry + kTickEntryFrameSlotOffset;
  s.mem.wlog = ext.writeLog;
  ::pes_fn_140797CD0(s);
  ReturnSegments(s, ext, lent);
  const uint8_t* p = P(slot);
  PesTickResult result;
  std::memcpy(&result.motion, p + kPWordAE0Offset, sizeof result.motion);
  result.mirror = p[kPMirrorOffset];
  result.segment = p[kPSegmentOffset];
  result.phase = p[kPPhaseOffset];
  result.playback = LoadF32(p, kPPlaybackOffset);
  return result;
}

// The scan's call frame: the output record sits above the entry rsp in the window's stack, zeroed as the anime driver zeroes it before
// the scan; the callee frames grow below the entry.
static constexpr std::size_t kScanFrameBytes = 0x600, kScanOutAboveEntry = 0x200;
// Win64: the 5th integer-class argument sits above the return address and the 32-byte shadow space.
static constexpr uint64_t kArmFirstStackArg = 0x28;

PesBodyScanOut PesWorld::RunBodyScan(int slot, PesTrapExternals& ext) {
  PesState s = NewCall(Bytes());
  const std::size_t lent = LendExternals(s, ext);
  const uint64_t entry = kStackVa + kStackBytes - kScanFrameBytes;
  const uint64_t out = entry + kScanOutAboveEntry;
  s.wr(entry, 0, 8);
  for (std::size_t i = 0; i < kScanOutBytes; i += sizeof(uint64_t)) s.wr(out + i, 0, sizeof(uint64_t));
  s.gpr[4] = entry;
  s.gpr[1] = PVa(slot);  // rcx: P
  s.gpr[2] = out;        // rdx: the driver's output record
  s.mem.wlog = ext.writeLog;
  ::pes_fn_1406EDCE0(s);
  ReturnSegments(s, ext, lent);
  PesBodyScanOut result;
  for (std::size_t i = 0; i < kScanOutBytes; ++i) result.bytes[i] = static_cast<uint8_t>(s.mem.rd(out + i, 1));
  return result;
}

PesWorld::AdjusterFrame PesWorld::StepAdjusters(int slot, float degToRad) {
  uint8_t* p = P(slot);
  AdjusterFrame frame;
  const uint64_t chain = LoadU64(p, kPScalePtrOffset);  // the anime chain *(P+0xA08): the window's physique object
  const std::size_t order[3] = {kPAdjusterOffset, kPHeightAdjusterOffset, kPYawAdjusterOffset};
  const uint8_t yawOverride = p[kPYawAdjusterOffset + kAdjYawFlagOffset];  // FUN_141EB3AF0's finish zeroes it
  // FUN_141EB3AF0 steps an adjuster when its state is 2 and the counter is past the hold (+0xE) and not past the last tick (+0x10).
  const auto steps = [p](std::size_t at) {
    uint32_t state;
    int16_t count, delay, ticks;
    std::memcpy(&state, p + at + kAdjStateOffset, sizeof state);
    std::memcpy(&count, p + at + kAdjCountOffset, sizeof count);
    std::memcpy(&delay, p + at + kAdjDelayOffset, sizeof delay);
    std::memcpy(&ticks, p + at + kAdjTicksOffset, sizeof ticks);
    return state == kAdjRunning && count >= delay && count <= ticks;
  };
  const bool aStepped = steps(kPAdjusterOffset), yawStepped = steps(kPYawAdjusterOffset);
  for (const std::size_t at : order) {
    PesState s = NewCall(Bytes());
    AttachImageTables(s);  // the adjusters' vtable slots (Match binds them) and the sine/frame-rate cells
    const uint64_t entry = kStackVa + kStackBytes - kScanFrameBytes;
    s.wr(entry, 0, 8);
    s.gpr[4] = entry;
    s.gpr[1] = PVa(slot) + at;  // rcx: the adjuster
    s.gpr[2] = chain;           // rdx: the anime chain
    ::pes_fn_141EB3AF0(s);
  }
  // A's step adds the frame's root displacement to the layer's root point FUN_141AF3CA0 loaded (the man's position, O2+0xD0), so the
  // displacement is the difference. A that did not step leaves the point as loaded (a difference of zero).
  const uint8_t* o2 = InWindow(Bytes(), LoadU64(PhysiqueA(slot), kAChildO2Offset));
  const uint8_t* a = p + kPAdjusterOffset + kAdjStepOffset;
  frame.strikeMoved = aStepped;
  if (aStepped) {
    frame.x = LoadF32(a, 0) - LoadF32(o2, kO2PointOffset);
    frame.z = LoadF32(a, 2 * sizeof(float)) - LoadF32(o2, kO2PointOffset + 2 * sizeof(float));
  }
  // The yaw adjuster's output (+0x20, P+0x4F0): FUN_141EB1A50's curves store acc = from + (the error spread so far) in degrees and
  // output acc * 0.0174533, i.e. the ABSOLUTE yaw in radians; with the flag (+0x24 = P+0x4F4, FUN_1406EEDF0 0x1406EEF75) it outputs
  // err / N, a rate in degrees per tick that EEDF0 turns into radians per tick.
  frame.yawStepped = yawStepped;
  frame.yawIsRate = yawOverride != 0;
  const float yaw = LoadF32(p, kPYawAdjusterOffset + kAdjYawOutOffset);
  frame.yawRad = frame.yawIsRate ? yaw * degToRad : yaw;
  return frame;
}

void PesWorld::ArmYaw(int slot, float fromDeg, float toDeg, int ticks, bool overrideMode) {
  const uint64_t entry = kStackVa + kStackBytes - kScanFrameBytes;
  uint32_t bits;
  PesState s = NewCall(Bytes());
  AttachImageTables(s);
  s.wr(entry, 0, 8);
  s.gpr[4] = entry;
  s.gpr[1] = PVa(slot) + kPYawAdjusterOffset;
  std::memcpy(&bits, &fromDeg, sizeof bits);
  s.xmm[1][0] = bits;
  std::memcpy(&bits, &toDeg, sizeof bits);
  s.xmm[2][0] = bits;
  s.gpr[9] = static_cast<uint32_t>(ticks);
  s.wr(entry + kArmFirstStackArg, overrideMode ? 1 : 0, 1);  // 5th argument: the override flag; the 6th and 7th are 0
  s.wr(entry + kArmFirstStackArg + 8, 0, 1);
  s.wr(entry + kArmFirstStackArg + 16, 0, 1);
  ::pes_fn_141EB35A0(s);
  PesState arm = NewCall(Bytes());
  AttachImageTables(arm);
  arm.wr(entry, 0, 8);
  arm.gpr[4] = entry;
  arm.gpr[1] = PVa(slot) + kPYawAdjusterOffset;
  ::pes_fn_141EB3AE0(arm);  // state 1 -> 2
}

void PesWorld::SetRootFacing(int slot, float angleDeg) {
  StoreF32(P(slot), kPHeadingOffset, angleDeg);
  ArmYaw(slot, angleDeg, angleDeg, 1, false);
}

void PesWorld::WriteJointBlock(int slot, const BoneFrames& frames, const std::array<bool, kScanBoneCount>& mask) {
  uint8_t* p = P(slot);
  constexpr std::size_t kPosBytes = 3 * sizeof(float), kRotBytes = 4 * sizeof(float);
  for (std::size_t bone = 0; bone < kScanBoneCount; ++bone) {
    if (!mask[bone]) continue;
    std::memcpy(p + kPJointPrevPosOffset + bone * kPosBytes, p + kPJointPosOffset + bone * kPosBytes, kPosBytes);
    std::memcpy(p + kPJointPrevRotOffset + bone * kRotBytes, p + kPJointRotOffset + bone * kRotBytes, kRotBytes);
    std::memcpy(p + kPJointPosOffset + bone * kPosBytes, frames[bone].position.data(), kPosBytes);
    std::memcpy(p + kPJointRotOffset + bone * kRotBytes, frames[bone].rotation.data(), kRotBytes);
  }
  p[kPJointReadyOffset] = 1;
}

void PesWorld::SetTickFrame(int slot, float bodyHeadingDeg) {
  uint8_t* frame = TickFrame();
  const TickPlan& plan = tickPlan_[slot];
  StoreF32(frame, kFrameHeadingOffset, plan.headingDeg);
  StoreU32(frame, kFrameKindOffset, static_cast<uint32_t>(plan.kind));
  StoreF32(frame, kFrameWeightOffset, plan.kind == kTickKindDirectional ? kTickWeightDirectional : kTickWeightControl);
  StoreF32(TickFrameX(), kFrameXHeadingOffset, bodyHeadingDeg);
}

uint8_t* PesWorld::AnimeRecord(int slot) { return InWindow(Bytes(), Answer(&::pes_fn_140A48910, Bytes(), slot)); }

uint8_t* PesWorld::ManRecord(int slot) { return BoundRecord(Bytes(), Answer(&::pes_fn_140A48A10, Bytes(), slot)); }

bool PesWorld::Involved(int official) {
  uint8_t* frame = Block(official) + kOfficialFrameOffset;
  StoreU32(frame, kOfficialFrameIndexOffset, static_cast<uint32_t>(official));
  PesState s = NewCall(Bytes());
  s.gpr[1] = VaOf(frame);  // rcx = A0, the official's per-frame record
  ::pes_fn_140A3A690(s);
  return (s.gpr[0] & 0xFF) != 0;  // al
}

void PesWorld::AttachImageTables(::PesState& s) const {
  for (const PesTrapSegment& seg : imageTables_.segments) s.mem.segs.push_back(PesMem::Seg{seg.va, seg.bytes});
}

bool PesWorld::RunPassTarget(int side, uint32_t playState, std::vector<std::pair<uint64_t, uint8_t>>* writeLog) {
  uint8_t* brain = Brain(side);
  StoreU64(brain, kBrainTeamOffset, SideVa(side));  // the tick's bind (step 0): Y+0x6D50 = T, cleared at its end (step 48)
  PeriodRecord& period = Period();
  const uint32_t saved = period.playState;
  period.playState = playState;
  PesState s = NewCall(Bytes());
  AttachImageTables(s);  // the none record the event queries return on an empty ring (kind 0x37), and the 54 Hz cells
  s.mem.wlog = writeLog;
  s.gpr[1] = BrainVa(side);  // rcx: Y
  ::pes_fn_1404FB2A0(s);
  StoreU64(brain, kBrainTeamOffset, 0);
  period.playState = saved;
  return (s.gpr[0] & 0xFF) != 0;
}

void PesWorld::BindAi(int side) {
  uint8_t* ai = Ai(side);
  StoreU64(ai, kAiContextPtrOffset, AiVa(side) + kAiArrayOffset);
  StoreU64(ai, kAiMaPtrOffset, MaVa(side));
  uint8_t* array = ai + kAiArrayOffset;
  const auto entry = [array](std::size_t index, uint64_t va) { StoreU64(array, sizeof(uint64_t) * index, va); };
  entry(kAiArrayMatchInfo, MatchInfoVa());
  entry(kAiArraySetup, AiVa(side) + kAiSetupOffset);
  entry(kAiArrayPitch, PitchVa());
  entry(kAiArrayBall, BallVa());
  entry(kAiArrayRoster, RosterVa());
  entry(kAiArrayEvents, EventsVa());
  for (int slot = 0; slot < kSideCount * kSlotsPerSide; ++slot) {
    entry(kAiArrayMen + slot, ManVa(slot));
    entry(kAiArraySlots + slot, SlotVa(slot));
    entry(kAiArrayAnime + slot, AnimeVa(slot));
  }
  for (int team = 0; team < kSideCount; ++team) {
    entry(kAiArrayInfo + team, TeamInfoVa(team));
    entry(kAiArraySides + team, SideVa(team));
  }
  entry(kAiArrayChaser, AiVa(side) + kAiChaserOffset);
}

void PesWorld::RunArrivalList(int side, std::vector<std::pair<uint64_t, uint8_t>>* writeLog) {
  PesState s = NewCall(Bytes());
  AttachImageTables(s);  // the fps cells and the angle tables the list's time terms read
  s.mem.wlog = writeLog;
  s.gpr[1] = AiVa(side);  // rcx: the AI object
  ::pes_fn_14045E0D0(s);
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

uint32_t PesWorld::CommitJudge() {
  PesState s = NewCall(Bytes());
  s.gpr[1] = SanctionVa(offsetof(PesSanctionImage, judge));                // rcx = the judge
  s.gpr[2] = SanctionVa(offsetof(PesSanctionImage, judge)) + kJudgePendingRecordOffset;  // rdx = the pending foul
  ::pes_fn_143F546F0(s);
  const uint8_t* result = JudgeResult();
  std::memcpy(MatchInfo() + kMiRestartKindOffset, result, kJudgeSyncBytes);
  return LoadU32(result, kJudgeResultCardLevelOffset);
}

void PesWorld::EndWhistle() {
  std::memset(image_->sanction.judgeResult.data(), 0, kJudgeSyncBytes);
  std::memset(MatchInfo() + kMiRestartKindOffset, 0, kJudgeSyncBytes);
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
