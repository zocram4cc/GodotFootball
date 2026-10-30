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
struct PesKickRequest {
  std::array<uint8_t, kKickRequestBytes> bytes{};
};

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
};

// The caller's frame in the window stack: rsp at the chooser's entry (the return address slot), the request at +0xD8 and the
// descriptor at +0x228, as the Kick start (FUN_140724CC0) lays them out.
inline constexpr std::size_t kKickFrameBytes = 0x400;
inline constexpr std::size_t kKickRequestFrameOffset = 0xD8;
inline constexpr std::size_t kKickDescriptorFrameOffset = 0x228;

}  // namespace pes
