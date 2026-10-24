// PES 2021's mark table (team-tick step 35, FUN_1404F61E0): the 11-byte opponent mask
// T+0xB7EC[11] plus the per-team marked-man slot word T+0xB7D8. Pure functions over the facts
// the builder reads; unwired (no caller yet). CONFIRMED pieces carry their VAs; MarkGateRE's
// four open inputs arrive as caller-supplied facts with ponytail sites.

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace pes {

class PeImage;

namespace marktable {

inline constexpr int kSlots = 11;
inline constexpr int kNoSlot = 0xFF;
inline constexpr int kMaxMarks = 3;  // 0x1404F65D7 CMP R9B,3; JNC: at most three commits

// T+0xB7D8: the per-team marked-man slot, within-side 0..10, 0xFF = empty (writers
// FUN_140546FB0/549D30, teardown FUN_14057A1F0/3E0; pes-re-mark-cone.md section 1).
struct MarkState {
  int markSlot = kNoSlot;
  std::array<uint8_t, kSlots> mark{};  // T+0xB7EC[11]
};

// Every float constant the builder reads, with the VA each is loaded from (the install's
// .rdata). Angles are in degrees; distances squared in m^2.
struct Constants {
  float rampBase = 0.0f;    // 0x14259BDF0 = 7.0 (0x1404F6354 SUBSS; NOT 8.0)
  float rampTop = 0.0f;     // 0x14258C264 = 3.0 (0x1404F635C ADDSS)
  float coneOffset = 0.0f;  // 0x14259BE0C = 15.0 (0x1404F638F SUBSS)
  float ceilD2 = 0.0f;      // 0x1425C48E4 = 1369.0 = 37^2 (0x1404F6439 COMISS; hard exit)
  float wrapCap = 0.0f;     // 0x1425C48D0 = 105.0 (0x1404F64D9 COMISS)
  float wrapSide = 0.0f;    // 0x142596E6C = 80.0 (0x1404F650D/532 COMISS pair)
  float coneDeg = 0.0f;     // 0x14259BE5C = 90.0 (0x1404F653C, cone arg)
  float coneLen = 0.0f;     // 0x14259BE04 = 12.0 (0x1404F6558, cone outer)
  float half = 0.0f;        // 0x142565600 = 0.5 (0x140A53E49 half-cone scale)
  float gateFrac = 0.0f;    // 0x14259BE44 = 64.0 (0x1404F656C, count-gate folded v)
  float gateRadius = 0.0f;  // 0x14259BE04 = 12.0 (T7a stack slot, count-gate radius)
  float gateWidth = 0.0f;   // 0x142565604 = 2.0 (0x140A4E905, count-gate width)
};

std::optional<Constants> LoadMarkTableConstants(const PeImage& exe, std::string* error);

// One candidate from the holder's NearestList row: the listed id and d^2 plus the pair-table
// bearings the wrap tests read (P[cand][tgt], P[cand][holder] via FUN_140439FE0).
struct Candidate {
  int id = -1;
  float d2 = 0.0f;
  int actionWord = 0;  // cand pl+0x5D0: admit iff 1 or 0x3F (0x1404F6484-492; writer FUN_140A6CA10)
  float posX = 0.0f;  // cand pl+0x55C (FUN_140A4C720's candX, 0x1404F6469/6F read pair)
  float posZ = 0.0f;  // cand pl+0x564 (cone live-bearing origin)
  float pairToTarget = 0.0f;
  float pairToHolder = 0.0f;
  // The candidate's facing word [cand+0x554] (0x1404F6498 MOVSS into XMM7): the subtrahend of both wrap tests
  // (FUN_14043A0A0 folds b - a with a = facing, at 0x1404F64C5 and 0x1404F64F9). FUN_1404AB8E0's bearing
  // (0x1404F64A8) goes to a local that T7a never reads.
  float head = 0.0f;
};

// Everything one BuildMarkTable call reads. Slots are global (0..21, 0xFF = none).
struct Facts {
  int side = 0;                // [RDX+8]: the team side 0..1 (0x1404F620D-215)
  int holder = kNoSlot;        // FUN_140A627E0, else MA+0x2483C (0x1404F623B-255)
  bool holderIsKeeper = false;  // FUN_140A6E320(holderClass) != 0 (0x1404F626D)
  float sign = 1.0f;           // [T+0x254] side sign (0x1404F6316)
  float holderX = 0.0f;        // holder pl+0x55C (0x1404F6348 MULSS)
  float holderZ = 0.0f;        // holder pl+0x564 (cone live-bearing origin)
  float goalAreaHalfWidth_m = 0.0f;  // FieldInfo F[5] (0x1404F633C-343 reads [[G+0x2AA8]+0x18])
  // The holder-side mates for the FUN_140A4EAF0 count gate (0x1404F656C-59F): ground-plane
  // positions; the gate counts mates within 12 m agreeing with u = P[cand][holder] to 32 deg.
  std::array<std::array<float, 2>, kSlots> gateMates{};
  // --- The two remaining open inputs (ponytail sites below): caller-supplied until decoded ---
  // ponytail: FUN_140A53DA0/53DF0 cone test (0x1404F6567: strict annulus + half-cone; T7a args 90/12/0).
  // ponytail: FUN_140A4EAF0/4E7C0 count gate <= (cone != 0) (0x1404F659D-59F; 12.0 radius, 32 deg band).
  std::array<Candidate, kSlots> candidates{};
};

// T7a's cone inner word: 0x1404F654F writes 0.0 to the stack slot, 0x1404F6580
// overwrites its low byte with 1, i.e. 0x00000001 ~= 1.4e-45. Constructed in
// code (never transcribed): d2 <= inner^2 can only fire at coincident points,
// where quirk and 0.0 are indistinguishable in f32 arithmetic.
inline float InnerQuirk() {
  uint32_t bits = 1;
  float v = 0.0f;
  static_assert(sizeof(v) == sizeof(bits));
  std::memcpy(&v, &bits, sizeof v);
  return v;
}

// The cone test (FUN_140A53DA0 -> FUN_140A53DF0): strict annulus inner^2 < d^2 < outer^2
// on the ground plane (0x140A53E20/26/2D + 0x140A53E2F/38/3F JBE: touching either
// radius FAILS), then |wrap180(live - headArg)| < 0.5 * coneDeg where live is the
// bearing from the candidate to the holder (FUN_140439EF0).
// candPos/holderPos are ground-plane (x, z); headArg is P[cand][holder]; inner is
// T7a's stack word (InnerQuirk() at the BuildMarkTable call site).
bool ConePasses(float candX, float candZ, float holderX, float holderZ, float headArg,
                float inner, const Constants& k);

// The count gate (FUN_140A4EAF0 -> FUN_140A4E7C0): counts the holder-side mates within
// 12 m of the holder whose bearing-from-holder agrees with u = P[cand][holder] to within
// 32.0 (64.0 / 2.0). T7a commits iff count <= (cone != 0).
int CountGate(float holderX, float holderZ, float u, const Facts& facts, const Constants& k);

// Step 35 (FUN_1404F61E0): zero the mask, gate on tgt/holder, walk the 11 candidates, commit
// mark[id % 11] = 1 up to three. Pure; the caller's Facts carry the open predicates.
void BuildMarkTable(MarkState* state, const Facts& facts, const Constants& k);

}  // namespace marktable
}  // namespace pes
