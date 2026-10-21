#include "pes/derivation.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace pes {
namespace {

constexpr const char* TABLE_PATH = "common/anime/playerID/bin/Derivationdatatable.bin";

// The file's stride: 848 bytes = 8 x 106 entries. The walkers read dword0 (the id and the motion
// out) and the low 16 bits of dword1 (the motion in and the sub).
constexpr size_t ENTRY_BYTES = 8;

// One instruction immediate or displacement. The `opcodeLen` bytes before it must be `opcode`, so
// an exe that differs fails loudly instead of yielding a neighbouring constant.
struct Site {
  uint64_t va;
  uint8_t opcode[3];
  uint8_t opcodeLen;
  uint8_t width;  // 1: a sign-extended imm8 or disp8; 4: an imm32 or disp32
};

// ---- FUN_1406B3560, the player walk: its switch arms, then the packing its loop unpacks.
// MOV R11D, imm32 per kind 0..5 (the case targets of the jump table at 0x1406B3744).
constexpr Site W1_LENGTH[DERIVATION_KINDS] = {
    {0x1406B3595, {0x41, 0xBB}, 2, 4}, {0x1406B35AD, {0x41, 0xBB}, 2, 4}, {0x1406B35A5, {0x41, 0xBB}, 2, 4},
    {0x1406B359D, {0x41, 0xBB}, 2, 4}, {0x1406B35B5, {0x41, 0xBB}, 2, 4}, {0x1406B35BD, {0x41, 0xBB}, 2, 4},
};
constexpr Site W1_MOTION_OUT_MASK = {0x1406B35DA, {0x41, 0xBC}, 2, 4};       // MOV R12D, 0xFFF
constexpr Site W1_MOTION_IN_MASK = {0x1406B35E5, {0x41, 0xBE}, 2, 4};        // MOV R14D, 0x7FF
constexpr Site W1_MOTION_IN_BASE = {0x1406B35EF, {0x41, 0xBF}, 2, 4};        // MOV R15D, 0x3CA
constexpr Site W1_MOTION_OUT_DELTA = {0x1406B35F5, {0x45, 0x8D, 0x6F}, 3, 1};  // LEA R13D, [R15-0x23]
constexpr Site W1_PLAYER_ID_MASK = {0x1406B3662, {0x25}, 1, 4};             // AND EAX, 0x1FFFF
constexpr Site W1_MOTION_OUT_SHIFT = {0x1406B366D, {0x41, 0xC1, 0xE9}, 3, 1};  // SHR R9D, 0x11
constexpr Site W1_MOTION_OUT_SHIFT_ZERO = {0x1406B3686, {0xC1, 0xE8}, 2, 1};   // SHR EAX, 0x11 (id 0 path)

// ---- FUN_1406B3780, the generic walk: the same lengths and packing, and the sub field.
constexpr Site W2_LENGTH[DERIVATION_KINDS] = {
    {0x1406B37B5, {0x41, 0xBB}, 2, 4}, {0x1406B37CD, {0x41, 0xBB}, 2, 4}, {0x1406B37C5, {0x41, 0xBB}, 2, 4},
    {0x1406B37BD, {0x41, 0xBB}, 2, 4}, {0x1406B37D5, {0x41, 0xBB}, 2, 4}, {0x1406B37DD, {0x41, 0xBB}, 2, 4},
};
constexpr Site W2_MOTION_IN_MASK = {0x1406B37FA, {0x41, 0xBC}, 2, 4};        // MOV R12D, 0x7FF
constexpr Site W2_MOTION_IN_BASE = {0x1406B3800, {0x41, 0xBD}, 2, 4};        // MOV R13D, 0x3CA
constexpr Site W2_MOTION_OUT_MASK = {0x1406B380B, {0x41, 0xB9}, 2, 4};       // MOV R9D, 0xFFF
constexpr Site W2_MOTION_OUT_DELTA = {0x1406B3811, {0x45, 0x8D, 0x7D}, 3, 1};  // LEA R15D, [R13-0x23]
constexpr Site W2_PLAYER_ID_MASK = {0x1406B3859, {0x41, 0xF7, 0xC0}, 3, 4};  // TEST R8D, 0x1FFFF
constexpr Site W2_SUB_SHIFT = {0x1406B3878, {0xC1, 0xE8}, 2, 1};            // SHR EAX, 0xB
constexpr Site W2_SUB_MASK = {0x1406B387B, {0x24}, 1, 1};                    // AND AL, 0x1F
constexpr Site W2_MOTION_OUT_SHIFT = {0x1406B3886, {0x41, 0xC1, 0xE8}, 3, 1};  // SHR R8D, 0x11

// ---- FUN_1406B3560's tail: the two cards with no list rows, and their three rewrites.
constexpr Site TAIL_ID_A = {0x1406B36C1, {0x81, 0xFE}, 2, 4};                   // CMP ESI, 0x20DA3
constexpr Site TAIL_ID_B = {0x1406B36C9, {0x81, 0xFE}, 2, 4};                   // CMP ESI, 0x20D30
constexpr Site TAIL_FK_IDLE = {0x1406B36D1, {0x41, 0x81, 0xF8}, 3, 4};          // CMP R8D, 0x4D1
constexpr Site TAIL_FK_KIND = {0x1406B36DA, {0x83, 0xFB}, 2, 1};                // CMP EBX, 2
constexpr Site TAIL_BECKHAM_2002_OFFSET = {0x1406B36DF, {0x41, 0x8D, 0x40}, 3, 1};  // LEA EAX, [R8+3]
constexpr Site TAIL_FK_RUNUP = {0x1406B36FB, {0x41, 0x81, 0xF8}, 3, 4};        // CMP R8D, 0x4E0
constexpr Site TAIL_CURVE = {0x1406B3725, {0x41, 0x81, 0xF8}, 3, 4};           // CMP R8D, 0x94C
constexpr Site TAIL_CURVE_KIND = {0x1406B372E, {0x83, 0xFB}, 2, 1};            // CMP EBX, 1
constexpr Site TAIL_CURVE_BECKHAM_2002 = {0x1406B3731, {0xB8}, 1, 4};          // MOV EAX, 0x93F

// ---- FUN_14071E570, the long-kick selection.
constexpr Site SEL_BECKHAM_ID[BECKHAM_CARDS] = {
    {0x14071E5C3, {0x3D}, 1, 4}, {0x14071E5D2, {0x3D}, 1, 4}, {0x14071E5E1, {0x3D}, 1, 4},
    {0x14071E5F0, {0x3D}, 1, 4}, {0x14071E5FF, {0x3D}, 1, 4},  // CMP EAX, imm32 per card
};
constexpr Site SEL_CURVE = {0x14071E60B, {0x8D, 0x87}, 2, 4};              // LEA EAX, [RDI-0x93D]
constexpr Site SEL_CURVE_SPAN = {0x14071E611, {0x83, 0xF8}, 2, 1};         // CMP EAX, 1
constexpr Site SEL_CURVE_MAX = {0x14071E616, {0x81, 0xFF}, 2, 4};          // CMP EDI, 0x948
constexpr Site SEL_BECKHAM_VARIANT = {0x14071E62B, {0x83, 0xF8}, 2, 1};    // CMP EAX, 8
constexpr Site SEL_BECKHAM_CURVE = {0x14071E630, {0xB8}, 1, 4};            // MOV EAX, 0x93F
constexpr Site SEL_PLAYER_KIND = {0x14071E65C, {0x41, 0xB9}, 2, 4};        // MOV R9D, 3
constexpr Site SEL_CURVE_CHECK = {0x14071E672, {0x3D}, 1, 4};              // CMP EAX, 0x93D
constexpr Site SEL_PLACEKICK = {0x14071E677, {0x41, 0xBE}, 2, 4};          // MOV R14D, 0x94C
constexpr Site SEL_GENERIC_KIND = {0x14071E698, {0x41, 0xB9}, 2, 4};       // MOV R9D, 1
constexpr Site SEL_PLACEKICK_DELTA = {0x14071E6AF, {0x41, 0x8D, 0x46}, 3, 1};  // LEA EAX, [R14-0xF]
constexpr Site SEL_REMAP_FROM = {0x14071E6B5, {0x81, 0xFF}, 2, 4};         // CMP EDI, 0xA5E
constexpr Site SEL_REMAP_DELTA = {0x14071E6BD, {0x8D, 0x47}, 2, 1};        // LEA EAX, [RDI+0x1D]
constexpr Site SEL_REMAP_TO_2 = {0x14071E6C2, {0xB8}, 1, 4};               // MOV EAX, 0xADC
constexpr Site SEL_REMAP_FROM_2 = {0x14071E6C7, {0x81, 0xFF}, 2, 4};       // CMP EDI, 0xABE

// Reads the sites in order. The first failure is the error; later reads return 0.
class SiteReader {
 public:
  SiteReader(const PeImage& exe, std::string* error) : exe_(exe), error_(error) {}

  int operator()(const Site& s) {
    if (!ok_) return 0;
    uint8_t b[sizeof s.opcode + 4] = {};
    if (!exe_.ReadVa(s.va, s.opcodeLen + s.width, b) || std::memcmp(b, s.opcode, s.opcodeLen) != 0) {
      char va[24];
      std::snprintf(va, sizeof va, "0x%llX", static_cast<unsigned long long>(s.va));
      *error_ = std::string("derivation: PES2021.exe does not hold the instruction the port reads at ") + va;
      ok_ = false;
      return 0;
    }
    if (s.width == 1) return int(int8_t(b[s.opcodeLen]));
    int32_t v;
    std::memcpy(&v, b + s.opcodeLen, sizeof v);
    return int(v);
  }

  bool ok() const { return ok_; }

 private:
  const PeImage& exe_;
  std::string* error_;
  bool ok_ = true;
};

// One entry: dword0 = id field (bits 0..16) | motion out (bits 17..28); word1 = motion in (bits
// 0..10) | sub (bits 11..15). Bits above these fields are not read.
DerivationEntry DecodeEntry(const uint8_t* p, const DerivationLayout& l) {
  const uint32_t dword0 = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
  const uint32_t word1 = uint32_t(p[4]) | uint32_t(p[5]) << 8;
  const uint32_t stored = dword0 & uint32_t(l.playerIdMask);
  DerivationEntry e{};
  if (stored != 0) e.playerId = int(stored) - 1;
  e.motionOut = int((dword0 >> l.motionOutShift) & uint32_t(l.motionOutMask)) + l.motionOutBase;
  e.motionIn = int(word1 & uint32_t(l.motionInMask)) + l.motionInBase;
  e.sub = int((word1 >> l.subShift) & uint32_t(l.subMask));
  return e;
}

}  // namespace

std::optional<DerivationLayout> LoadDerivationLayout(const PeImage& exe, std::string* error) {
  SiteReader read(exe, error);
  DerivationLayout l{};
  std::array<int, DERIVATION_KINDS> walker2Length{};
  for (int k = 0; k < DERIVATION_KINDS; ++k) {
    l.listLength[k] = read(W1_LENGTH[k]);
    walker2Length[k] = read(W2_LENGTH[k]);
  }
  l.motionInMask = read(W1_MOTION_IN_MASK);
  l.motionInBase = read(W1_MOTION_IN_BASE);
  const int motionOutDelta = read(W1_MOTION_OUT_DELTA);
  l.motionOutBase = l.motionInBase + motionOutDelta;
  l.motionOutMask = read(W1_MOTION_OUT_MASK);
  l.playerIdMask = read(W1_PLAYER_ID_MASK);
  l.motionOutShift = read(W1_MOTION_OUT_SHIFT);
  const int zeroIdShift = read(W1_MOTION_OUT_SHIFT_ZERO);
  const int w2InMask = read(W2_MOTION_IN_MASK), w2InBase = read(W2_MOTION_IN_BASE);
  const int w2OutMask = read(W2_MOTION_OUT_MASK), w2OutDelta = read(W2_MOTION_OUT_DELTA);
  const int w2IdMask = read(W2_PLAYER_ID_MASK), w2OutShift = read(W2_MOTION_OUT_SHIFT);
  l.subShift = read(W2_SUB_SHIFT);
  l.subMask = read(W2_SUB_MASK);
  if (!read.ok()) return std::nullopt;
  if (walker2Length != l.listLength || w2InMask != l.motionInMask || w2InBase != l.motionInBase ||
      w2OutMask != l.motionOutMask || w2OutDelta != motionOutDelta || w2IdMask != l.playerIdMask ||
      w2OutShift != l.motionOutShift || zeroIdShift != l.motionOutShift) {
    *error = "derivation: PES2021.exe's two walkers unpack the entries differently";
    return std::nullopt;
  }
  return l;
}

std::optional<DerivationRules> LoadDerivationRules(const PeImage& exe, std::string* error) {
  SiteReader read(exe, error);
  DerivationRules r{};
  for (int i = 0; i < BECKHAM_CARDS; ++i) r.beckhamIds[i] = read(SEL_BECKHAM_ID[i]);
  r.tailIdA = read(TAIL_ID_A);
  r.tailIdB = read(TAIL_ID_B);
  r.freeKickIdle = read(TAIL_FK_IDLE);
  r.freeKickRunup = read(TAIL_FK_RUNUP);
  r.freeKickKind = read(TAIL_FK_KIND);
  r.beckham2002Offset = read(TAIL_BECKHAM_2002_OFFSET);
  r.curveKind = read(TAIL_CURVE_KIND);
  r.curvePlacekick = read(TAIL_CURVE);
  r.curveBeckham2002 = read(TAIL_CURVE_BECKHAM_2002);
  r.curve = -read(SEL_CURVE);  // the LEA's displacement is -0x93D
  r.curveSpan = read(SEL_CURVE_SPAN);
  r.curveMax = read(SEL_CURVE_MAX);
  r.beckhamVariant = read(SEL_BECKHAM_VARIANT);
  const int selBeckhamCurve = read(SEL_BECKHAM_CURVE);
  r.playerKind = read(SEL_PLAYER_KIND);
  const int selCurve = read(SEL_CURVE_CHECK);
  const int selPlacekick = read(SEL_PLACEKICK);
  r.genericKind = read(SEL_GENERIC_KIND);
  r.curvePlacekickDelta = read(SEL_PLACEKICK_DELTA);
  r.remapFrom = read(SEL_REMAP_FROM);
  r.remapDelta = read(SEL_REMAP_DELTA);
  r.remapTo2 = read(SEL_REMAP_TO_2);
  r.remapFrom2 = read(SEL_REMAP_FROM_2);
  if (!read.ok()) return std::nullopt;
  // The selection names the same clips and cards as the tail; the exe must say so too.
  if (selBeckhamCurve != r.curveBeckham2002 || selCurve != r.curve || selPlacekick != r.curvePlacekick ||
      r.tailIdA != r.beckhamIds[4] || r.tailIdB != r.beckhamIds[3]) {
    *error = "derivation: PES2021.exe's copies of a rule disagree";
    return std::nullopt;
  }
  return r;
}

std::optional<DerivationTable> ParseDerivationTable(const std::vector<uint8_t>& bytes, const DerivationLayout& layout,
                                                    const DerivationRules& rules, std::string* error) {
  size_t entries = 0;
  for (int n : layout.listLength) entries += size_t(n);
  if (bytes.size() != entries * ENTRY_BYTES) {
    *error = "derivation: the table is " + std::to_string(bytes.size()) + " bytes, not " +
             std::to_string(entries * ENTRY_BYTES);
    return std::nullopt;
  }
  DerivationTable t{};
  t.rules = rules;
  size_t at = 0;
  for (int k = 0; k < DERIVATION_KINDS; ++k)
    for (int i = 0; i < layout.listLength[k]; ++i, at += ENTRY_BYTES)
      t.lists[k].push_back(DecodeEntry(bytes.data() + at, layout));
  return t;
}

std::optional<DerivationTable> LoadDerivationTable(const PeImage& exe, const PesVfs& vfs, std::string* error) {
  const auto layout = LoadDerivationLayout(exe, error);
  if (!layout) return std::nullopt;
  const auto rules = LoadDerivationRules(exe, error);
  if (!rules) return std::nullopt;
  const auto bytes = vfs.Read(TABLE_PATH, error);
  if (!bytes) return std::nullopt;
  return ParseDerivationTable(*bytes, *layout, *rules, error);
}

int DerivePlayerMotion(const DerivationTable& t, int playerId, int motionIn, int kind) {
  int out = motionIn;
  if (kind >= 0 && kind < DERIVATION_KINDS)
    for (const DerivationEntry& e : t.lists[kind]) {
      // A zero player id skips the id test (FUN_1406B3560 @0x1406B3653).
      const bool playerMatches = playerId == 0 || (e.playerId && *e.playerId == playerId);
      if (e.motionIn == motionIn && playerMatches) out = e.motionOut;
      if (out != motionIn) break;  // the scan stops at the first real change (@0x1406B3694)
    }
  // The tail (@0x1406B36C1..0x1406B3731) rewrites its two cards whatever the list said.
  const DerivationRules& r = t.rules;
  if (playerId != r.tailIdA && playerId != r.tailIdB) return out;
  if (kind == r.freeKickKind && (motionIn == r.freeKickIdle || motionIn == r.freeKickRunup))
    return motionIn + r.beckham2002Offset;
  if (kind == r.curveKind && motionIn == r.curvePlacekick) return r.curveBeckham2002;
  return out;
}

int DeriveGenericMotion(const DerivationTable& t, int stat, int motionIn, int kind) {
  int out = motionIn;
  if (kind >= 0 && kind < DERIVATION_KINDS)
    for (const DerivationEntry& e : t.lists[kind]) {
      // A generic entry (id field 0) whose sub minus one is the stat (FUN_1406B3780 @0x1406B3859..0x1406B3884).
      if (!e.playerId && e.motionIn == motionIn && e.sub - 1 == stat) out = e.motionOut;
      if (out != motionIn) break;
    }
  return out;
}

int SelectLongKickClip(const DerivationTable& t, const LongKickInput& in) {
  const DerivationRules& r = t.rules;
  const bool beckhamCard = std::find(r.beckhamIds.begin(), r.beckhamIds.end(), in.playerId) != r.beckhamIds.end();
  // The curve range (picked - curve, unsigned, <= span) or the third curve pick (@0x14071E60B..0x14071E616).
  const bool curvePick = uint32_t(in.picked - r.curve) <= uint32_t(r.curveSpan) || in.picked == r.curveMax;
  if (beckhamCard && !in.flag && curvePick && in.animeVariant == r.beckhamVariant) return r.curveBeckham2002;
  int clip = in.picked;
  if (in.substitutable) {
    clip = DerivePlayerMotion(t, in.playerId, in.picked, r.playerKind);
    if (clip == in.picked) {
      // The curve is walked as the placekick it becomes (@0x14071E672..0x14071E677).
      clip = DeriveGenericMotion(t, in.stat, in.picked == r.curve ? r.curvePlacekick : in.picked, r.genericKind);
      if (clip == r.curvePlacekick) return r.curvePlacekick + r.curvePlacekickDelta;  // back to the curve (@0x14071E6AF)
    }
  }
  if (clip == r.remapFrom) return clip + r.remapDelta;  // @0x14071E6B5, 0x14071E6BD
  if (clip == r.remapFrom2) return r.remapTo2;          // @0x14071E6C7, 0x14071E6C2
  return clip;
}

}  // namespace pes
