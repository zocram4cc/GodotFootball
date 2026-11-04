// The kick chooser FUN_1408364D0(descriptor out, P, request) on GF's window (docs/ports/pes-port-KickKind.md, -KickKind2.md): the
// request the Kick action's start builds with FUN_140833CD0 (W5, FUN_140724CC0) and the descriptor the chooser answers. The
// chooser and the 175..193 functions it enters are translated (src/pes/translated, set kickkind) and proven bit-exact on the
// recorded kick_chooser calls. Offsets are bytes the chooser reads/writes (live-in set of the captures); names are the builder's.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pes {

// The request, at the entry rsp + kKickRequestFrameOffset (r8). The chooser reads 0x118 bytes of the caller's frame from it (the
// manifest's S ranges run to rsp + 0x1F0 = request + 0x118).
inline constexpr std::size_t kKickRequestBytes = 0x118;
inline constexpr std::size_t kKickRequestActionOffset = 0x0;     // s32: the anime action id, 8 lofted pass, 9 ground pass, 10 shot (cmd[0])
inline constexpr std::size_t kKickRequestSubtypeOffset = 0x4;    // s32: FUN_1407204E0(cmd.ptr, type, cmd[7])
inline constexpr std::size_t kKickRequestFirstTimeOffset = 0x2C; // s32: req[0xB] = FUN_140837FC0(player), the first-time byte
inline constexpr std::size_t kKickRequestTargetOffset = 0x64;    // 3 x f32: req[0x19..0x1B], the target point
inline constexpr std::size_t kKickRequestActorOffset = 0x110;   // u64: the kicker's actor object P (FUN_140821050(req, ap) stores it)
struct PesKickRequest {
  std::array<uint8_t, kKickRequestBytes> bytes{};
};

// The request GF can build (FUN_140833CD0 builds the whole 0x118 bytes from the AI's order record, which GF has no state for): the
// action id, the subtype and the target point. `target` is in PES axes (x, height, z).
// ponytail: FUN_1407204E0 reads the order bytes +0x149 / +0x14A / +0x14C and cmd[7] for the subtype; with them zero it answers 7
// (lofted), 1 (ground) or 9 (shot), which is what GF writes. The first-time word (+0x2C, FUN_140837FC0), the geometry words
// (+0x8 .. +0xAC: distances, headings, the order's flags) and every other byte of the request stay zero until FUN_140833CD0 runs
// on an order record built from GF's pass/shot decision.
inline PesKickRequest KickRequestFor(int32_t action, const float (&target)[3]) {
  constexpr int32_t kLofted = 8, kGround = 9, kShot = 10, kOtherSubtype = 0x14;  // the chooser's request ids; 0x14 = FUN_1407204E0's default
  constexpr int32_t kSubtypeLofted = 7, kSubtypeGround = 1, kSubtypeShot = 9;
  const int32_t subtype = action == kLofted ? kSubtypeLofted : action == kGround ? kSubtypeGround : action == kShot ? kSubtypeShot : kOtherSubtype;
  PesKickRequest r;
  __builtin_memcpy(r.bytes.data() + kKickRequestActionOffset, &action, sizeof action);
  __builtin_memcpy(r.bytes.data() + kKickRequestSubtypeOffset, &subtype, sizeof subtype);
  __builtin_memcpy(r.bytes.data() + kKickRequestTargetOffset, target, sizeof target);
  return r;
}

// The descriptor the chooser answers (rcx), FUN_140821170's copy of the chooser's local record. FUN_140724CC0 reads the kind from
// +0, the band from +4, the bucket from +8, the first-time byte from +0xC and falls back to the alternate kind at +0x28 when +0
// is the none value.
inline constexpr std::size_t kKickDescriptorBytes = 0x108;
inline constexpr std::size_t kKickDescriptorKindOffset = 0x0;     // s32: the kick kind (ap+0x2752), kKickNoKind = none
inline constexpr std::size_t kKickDescriptorBandOffset = 0x4;     // s32: ap+0x2754
inline constexpr std::size_t kKickDescriptorBucketOffset = 0x8;   // s32: ap+0x2755
inline constexpr std::size_t kKickDescriptorSideOffset = 0xC;     // s32: ap+0x2756
inline constexpr std::size_t kKickDescriptorAltKindOffset = 0x28; // s32: the retry kind (result[10])
inline constexpr int32_t kKickNoKind = 0x97;
inline constexpr std::size_t kKickDescriptorFlagAOffset = 0x10;   // u8: cStack_158 of FUN_140724CC0, the flag the row and the motion take
inline constexpr std::size_t kKickDescriptorNoAltOffset = 0xDE;   // u8: cStack_8a: set, FUN_140724CC0 drops the alternate kind
struct PesKickDescriptor {
  std::array<uint8_t, kKickDescriptorBytes> bytes{};
  int32_t Word(std::size_t offset) const {
    int32_t v;
    __builtin_memcpy(&v, bytes.data() + offset, sizeof v);
    return v;
  }
  int32_t Kind() const { return Word(kKickDescriptorKindOffset); }
  int32_t Band() const { return Word(kKickDescriptorBandOffset); }
  int32_t Bucket() const { return Word(kKickDescriptorBucketOffset); }
  int32_t Side() const { return Word(kKickDescriptorSideOffset); }
  int32_t AltKind() const { return Word(kKickDescriptorAltKindOffset); }
  uint8_t FlagA() const { return bytes[kKickDescriptorFlagAOffset]; }
  // The kind the Kick start plays (FUN_140724CC0's decompile: `if (cStack_8a != 0) iStack_140 = 0x97`, then the kind, or the
  // alternate kind when the kind is none and the alternate is not; the byte at +0xDE set drops the alternate).
  int32_t ChosenKind() const {
    const int32_t alt = bytes[kKickDescriptorNoAltOffset] != 0 ? kKickNoKind : AltKind();
    return Kind() == kKickNoKind && alt != kKickNoKind ? alt : Kind();
  }
};

// The caller's frame in the window stack: rsp at the chooser's entry (the return address slot), the request at +0xD8 and the
// descriptor at +0x228, as the Kick start (FUN_140724CC0) lays them out.
inline constexpr std::size_t kKickFrameBytes = 0x400;
inline constexpr std::size_t kKickRequestFrameOffset = 0xD8;
inline constexpr std::size_t kKickDescriptorFrameOffset = 0x228;

}  // namespace pes
