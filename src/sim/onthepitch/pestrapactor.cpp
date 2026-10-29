#include "onthepitch/pestrapactor.hpp"

#include <cmath>
#include <cstring>
#include <initializer_list>

namespace pes {
namespace {

// A rip-relative instruction the constant is read through: its bytes at `va` must start with `opcode`, the disp32 sits
// `dispOffset` bytes in, and the instruction is `length` bytes long (the target is relative to its end).
struct Site {
  uint64_t va;
  std::initializer_list<uint8_t> opcode;
  int dispOffset;
  int length;
};
// FUN_1406E03D0 (the constructor): `lea rax,[vtable]` before each of the two vtable stores.
constexpr Site kVtable0Site{0x1406E0412, {0x48, 0x8D, 0x05}, 3, 7};
constexpr Site kVtable1Site{0x1406E0420, {0x48, 0x8D, 0x05}, 3, 7};
// FUN_1406D8BF0: `movss xmm1,[standing speed]` (class 3 above it, 1 at or below).
constexpr Site kStandingSite{0x1406D8C35, {0xF3, 0x0F, 0x10, 0x0D}, 4, 8};
// FUN_1406EA110 (the root speed in km/h): rate * 60 * 60 / 1000 * |displacement|.
constexpr Site kSixtySite{0x1406EA14E, {0xF3, 0x44, 0x0F, 0x59, 0x05}, 5, 9};
constexpr Site kSixtyAgainSite{0x1406EA15B, {0xF3, 0x44, 0x0F, 0x59, 0x05}, 5, 9};
constexpr Site kThousandSite{0x1406EA168, {0xF3, 0x44, 0x0F, 0x5E, 0x05}, 5, 9};
// FUN_141E532D0 (the tick rate): `movss xmm0,[rate]; ret`.
constexpr Site kTickRateSite{0x141E532D0, {0xF3, 0x0F, 0x10, 0x05}, 4, 8};
// FUN_1406EB210: `divss xmm0,[180]` for P+0x3B18.
constexpr Site kHeightDivisorSite{0x1406EB231, {0xF3, 0x0F, 0x5E, 0x05}, 4, 8};
// FUN_1406D8BF0: `cmp byte [phase],0x18` and `movzx eax,byte [rdi + rcx*4 + disp32]` with rdi = the image base.
constexpr uint64_t kPhaseCountVa = 0x1406D8C59;
constexpr uint64_t kClassTableVa = 0x1406D8C92;
constexpr uint8_t kPhaseCountOpcode[] = {0x80, 0x38};
constexpr uint8_t kClassTableOpcode[] = {0x0F, 0xB6, 0x84, 0x8F};
constexpr std::size_t kClassTableStride = 4;

struct ImageView {
  const PeImage& exe;
  bool Read(uint64_t va, std::size_t n, void* out) const { return exe.ReadVa(va, n, static_cast<uint8_t*>(out)); }
  bool OpcodeAt(uint64_t va, const uint8_t* opcode, std::size_t n) const {
    uint8_t got[16];
    return n <= sizeof got && Read(va, n, got) && std::memcmp(got, opcode, n) == 0;
  }
  // The target VA of a rip-relative site, or 0 when its opcode is not the one expected.
  uint64_t Target(const Site& s) const {
    const std::size_t n = s.opcode.size();
    const std::initializer_list<uint8_t>& op = s.opcode;
    if (!OpcodeAt(s.va, op.begin(), n)) return 0;
    int32_t disp = 0;
    if (!Read(s.va + static_cast<uint64_t>(s.dispOffset), sizeof disp, &disp)) return 0;
    return s.va + static_cast<uint64_t>(s.length) + static_cast<uint64_t>(static_cast<int64_t>(disp));
  }
  bool Float(const Site& s, float* out) const {
    const uint64_t at = Target(s);
    return at != 0 && Read(at, sizeof *out, out);
  }
};

}  // namespace

std::optional<PesActorConstants> LoadActorConstants(const PeImage& exe, std::string* error) {
  const ImageView img{exe};
  PesActorConstants c;
  float again = 0.0f;
  const uint64_t vt0 = img.Target(kVtable0Site), vt1 = img.Target(kVtable1Site);
  bool ok = vt0 != 0 && vt1 != 0;
  c.interfaceVtable[0] = vt0;
  c.interfaceVtable[1] = vt1;
  ok = ok && img.Float(kStandingSite, &c.standingSpeedKmh) && img.Float(kSixtySite, &c.secondsPerHour60) &&
       img.Float(kSixtyAgainSite, &again) && again == c.secondsPerHour60 && img.Float(kThousandSite, &c.metresPerKm) &&
       img.Float(kTickRateSite, &c.tickRate) && img.Float(kHeightDivisorSite, &c.heightDivisor);
  // The class table: its base is the image base plus the instruction's disp32, and the guard before it bounds the phase.
  uint8_t bound = 0;
  int32_t disp = 0;
  ok = ok && img.OpcodeAt(kPhaseCountVa, kPhaseCountOpcode, sizeof kPhaseCountOpcode) &&
       img.Read(kPhaseCountVa + sizeof kPhaseCountOpcode, 1, &bound) && bound == kLocomotionPhases &&
       img.OpcodeAt(kClassTableVa, kClassTableOpcode, sizeof kClassTableOpcode) &&
       img.Read(kClassTableVa + sizeof kClassTableOpcode, sizeof disp, &disp);
  for (std::size_t i = 0; ok && i < kLocomotionPhases; ++i)
    ok = img.Read(kImageBaseVa + static_cast<uint64_t>(static_cast<int64_t>(disp)) + i * kClassTableStride, 1,
                  &c.locomotionClass[i]);
  if (!ok) {
    if (error != nullptr) *error = "PES image: the trap actor's constants did not match their opcodes";
    return std::nullopt;
  }
  return c;
}

uint8_t LocomotionClass(const PesActorConstants& c, const PesActorFacts& f, const uint8_t* p) {
  if (f.phase == 0) {
    // FUN_1406EA110: the root speed in km/h from the displacement P+0x8 - P+0x14 (x and z; 0x141EB50B0 idx < 2).
    const float dx = LoadF32(p, kPPosOffset) - LoadF32(p, kPNextPosOffset);
    const float dz = LoadF32(p, kPPosOffset + 8) - LoadF32(p, kPNextPosOffset + 8);
    const float perTick = c.tickRate * c.secondsPerHour60 * c.secondsPerHour60 / c.metresPerKm;
    const float speed = perTick * std::sqrt(dz * dz + dx * dx);
    return speed <= c.standingSpeedKmh ? 1 : 3;
  }
  if (f.phase >= kLocomotionPhases) return 0;
  if (f.phase == 1 && f.word2780 > f.word277C) return 4;
  return c.locomotionClass[f.phase];
}

void WriteActorP(PesWorld& world, const PesActorFacts& f, const PesActorOpenInputs& open, const PesActorConstants& c,
                 const StaminaConstants& stamina, const PhysiqueConstants& physique) {
  uint8_t* p = world.P(f.slot);
  const uint8_t* man = world.ManRecord(f.slot);
  // P's position, previous position and heading are the man record's (CONFIRMED 6/6, pes-re-trap-inputs.md 1.2).
  std::memcpy(p + kPPosOffset, man + kManPosXOffset, 12);
  std::memcpy(p + kPNextPosOffset, man + kManPrevPosXOffset, 12);
  std::memcpy(p + kPHeadingOffset, man + kManFacingOffset, 4);
  p[kPByte2458Offset] = static_cast<uint8_t>(f.slot);  // FUN_1406EB670's store 0x1406EB6C1
  p[kPPhaseOffset] = f.phase;
  std::memcpy(p + kPInterfaceVtable0Offset, &c.interfaceVtable[0], sizeof(uint64_t));
  std::memcpy(p + kPInterfaceVtable1Offset, &c.interfaceVtable[1], sizeof(uint64_t));
  p[kPByteAC8Offset] = f.action;  // FUN_1406EEBB0's stores: current id and sub, then the previous pair
  p[kPByteAC9Offset] = f.sub;
  p[kPPrevActionOffset] = f.prevAction;
  p[kPPrevActionOffset + 1] = f.prevSub;
  std::memcpy(p + kPWordAE0Offset, &f.motion, sizeof f.motion);
  StoreU32(p, kPWord430Offset, f.kickApproach);
  StoreF32(p, kPWord277COffset, f.word277C);
  StoreF32(p, kPWord277COffset + 4, f.word2780);
  p[kPByteB08Offset] = LocomotionClass(c, f, p);  // FUN_1406D8BF0 (after the position it measures)
  const uint64_t slotPtr = PesWorld::SlotVa(f.slot) + kSlotPPtrTargetOffset;  // FUN_140925470: slot record + 0x15C0
  std::memcpy(p + kPSlotPtrOffset, &slotPtr, sizeof slotPtr);
  StoreF32(p, kPF3B18Offset, f.heightCm / c.heightDivisor);  // FUN_1406EB210 (0x1406EB23E)
  // FUN_141EB5810's root-bone arm: G stored at P+0xA00 and pushed into the chain *(*(P+0xA08)+0xF0)+0xC0 (FUN_141AF4EB0); the
  // rate ceiling reads the same G (FUN_1406F99F0 via FUN_141AF3180).
  const float g = PhysiqueG(physique, {f.heightCm, f.legSlider, false});
  StoreF32(p, kPGOffset, g);
  const uint64_t chainA = PesWorld::PhysiqueAVa(f.slot), chainB = PesWorld::PhysiqueBVa(f.slot);
  std::memcpy(p + kPScalePtrOffset, &chainA, sizeof chainA);
  std::memcpy(world.PhysiqueA(f.slot) + kPhysiqueNextOffset, &chainB, sizeof chainB);
  StoreF32(world.PhysiqueB(f.slot), kPhysiqueGOffset, g);
  StoreF32(p, kPF377CScaleOffset, MoveTopRate(stamina, f.speedByte, f.stamina, g));  // FUN_1406F99F0 (0x1406F9B4C)
  std::memcpy(p + kPVelocityOffset, open.velocityRing.data(), open.velocityRing.size());
  const uint64_t ctx = PesWorld::CtxVa(f.side);  // P+0x3AC0 = PM+0x7E0: the side's AI context (+0x18 MA, +0xE0 E)
  std::memcpy(p + kPPtr3AC0Offset, &ctx, sizeof ctx);
}

}  // namespace pes
