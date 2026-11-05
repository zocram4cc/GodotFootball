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

// The order the Kick start hands FUN_140833CD0 (r8 = cmd = param_4+0xB8 of FUN_140724CC0): cmd[0] is the action id (8 lofted pass,
// 9 ground pass, 10 shot), cmd+8 points to the AI's order record, cmd[7] (+0x1C) names the order's kind. The record is a 0x160-byte
// block; the offsets are the bytes the builder and FUN_1407204E0 read (docs/ports/pes-port-KickHeader.md, readset.py over the 15
// kick_header captures; a read in one capture only is marked). Its PES writer is OPEN: the record is a stack copy built in the AI
// order path above FUN_140724CC0 (docs/ports/pes-port-KickHeader.md section 4), so GF's order is built by PesKickOrder's setters.
inline constexpr std::size_t kKickOrderRecordBytes = 0x160;
inline constexpr std::size_t kOrderHeadingOffset = 0x0;     // f32 deg: the order's heading, read when the target height +0x30 < 0 (shots)
inline constexpr std::size_t kOrderPowerOffset = 0x8;       // f32: request +0xC, the order's distance / power (0.7, 1.0 or 0)
inline constexpr std::size_t kOrderReceiverOffset = 0x14;   // u32: the receiver's slot, >= 0x16 none (0xFF)
inline constexpr std::size_t kOrderWord1COffset = 0x1C;     // s32: request +0x1C (0 in every capture)
inline constexpr std::size_t kOrderWord28Offset = 0x28;     // s32: request +0x24 (0 in every capture)
inline constexpr std::size_t kOrderTargetOffset = 0x2C;     // 3 x f32 (x, height, z): the target point, copied to request +0x64
inline constexpr std::size_t kOrderFlag5COffset = 0x5C;     // u8: request +0x29 (0 in every capture)
inline constexpr std::size_t kOrderFlag5DOffset = 0x5D;     // u8: request +0x2A (0 in every capture)
inline constexpr std::size_t kOrderSpeedOffset = 0xB4;      // f32: read for cmd[7] == 0x13 (the type-8 receiver pass)
inline constexpr std::size_t kOrderByteCCOffset = 0xCC;     // u8: request +0x28, read for cmd[7] == 0x13
inline constexpr std::size_t kOrderByte149Offset = 0x149;   // u8: FUN_1407204E0's lofted-pass subtype selector
inline constexpr std::size_t kOrderByte14AOffset = 0x14A;   // u8: its ground-pass / shot subtype selector
inline constexpr std::size_t kOrderFlags14COffset = 0x14C;  // u8: flag bits (0x1, 0x10, 0x20) the builder and FUN_1407204E0 test
inline constexpr int32_t kOrderKindReceiverPass = 0x13;     // cmd[7]: the type-8 pass to a receiver
// cmd[7] of the orders GF issues: lofted 0x15 (the immediate FUN_1407204E0 tests for subtype 8), ground pass 0x16 and shot 0x19 (the
// value in the 4 ground-pass and 2 shot captures of pid802552).
inline constexpr int32_t kOrderKindLofted = 0x15, kOrderKindGround = 0x16, kOrderKindShot = 0x19;
inline constexpr uint32_t kOrderNoReceiver = 0xFF;  // +0x14 >= 0x16: no receiver (the shot and no-receiver lofted captures)
struct PesKickOrder {
  int32_t action = 0;  // cmd[0]
  int32_t kind = 0;    // cmd[7]
  std::array<uint8_t, kKickOrderRecordBytes> record{};
  void SetF32(std::size_t offset, float v) { __builtin_memcpy(record.data() + offset, &v, sizeof v); }
  void SetU32(std::size_t offset, uint32_t v) { __builtin_memcpy(record.data() + offset, &v, sizeof v); }
  void SetTarget(const float (&p)[3]) { __builtin_memcpy(record.data() + kOrderTargetOffset, p, sizeof p); }
};

// The order GF builds from what its decision holds: the action id, the target point (PES axes: x, height, z), the receiver's slot
// (kOrderNoReceiver when GF's decision names none), the launch speed, and for a shot its heading (deg) and power. cmd[7] follows the
// action as the captures show it: a lofted pass to a receiver 0x13, without 0x15, a ground pass 0x16, a shot 0x19.
// The 15 kick_header captures (pid802552; hdrprobe over their orders) carry in every order +0x149 == 2 (the record's default word
// +0x148 = 0x200: FUN_1407204E0 then answers subtype 5 for a receiver pass, where a zero byte would answer 7); +0x8 = 1.0 in the four
// lofted passes without a receiver (0 in the receiver passes and ground passes, 0.7 in the shots); +0xB4 = the pass speed in the five
// receiver passes (24.05..29.04, GF: the launch speed, whose factors the builder takes: x 3.6 / 3.5).
// ponytail: every other byte of the record (+0x1C, +0x28, +0x5C/+0x5D, +0xCC, +0x14A/+0x14C) stays zero as in all 15 captures; the
// values above are the AI order path's (OPEN, docs/ports/pes-port-KickHeader.md section 4): the record's initialiser and the order
// writers are not found yet. Remove each when that writer is ported.
inline constexpr uint8_t kOrderByte149Default = 2;  // +0x149 in 15 of 15 captured orders
inline constexpr float kOrderLoftedNoReceiverPower = 1.0f;  // +0x8 in the 4 captured lofted passes without a receiver
inline PesKickOrder KickOrderFor(int32_t action, const float (&target)[3], uint32_t receiver, float headingDeg, float power,
                                 float launchSpeed) {
  constexpr int32_t kLofted = 8, kShot = 10;  // the action ids (cmd[0])
  PesKickOrder o;
  o.action = action;
  o.kind = action == kShot ? kOrderKindShot
           : action == kLofted ? (receiver < 0x16 ? kOrderKindReceiverPass : kOrderKindLofted)
                               : kOrderKindGround;
  o.SetTarget(target);
  o.SetU32(kOrderReceiverOffset, receiver);
  o.record[kOrderByte149Offset] = kOrderByte149Default;
  if (action == kShot) {
    o.SetF32(kOrderHeadingOffset, headingDeg);
    o.SetF32(kOrderPowerOffset, power);
  } else if (o.kind == kOrderKindLofted) {
    o.SetF32(kOrderPowerOffset, kOrderLoftedNoReceiverPower);
  } else if (o.kind == kOrderKindReceiverPass) {
    o.SetF32(kOrderSpeedOffset, launchSpeed);
  }
  return o;
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

// The caller's frame in the window stack: rsp at the callee's entry (the return address slot), the builder's stack arguments at
// +0x28 (u8), +0x30 (s32), +0x38 (f32) and +0x40 (a pointer), the request at +0xD8 and the descriptor at +0x228, as the Kick start
// (FUN_140724CC0) lays them out; above them the order the builder reads (cmd, its record, and the word the 8th argument points to).
inline constexpr std::size_t kKickFrameBytes = 0x600;
inline constexpr std::size_t kKickRequestFrameOffset = 0xD8;
inline constexpr std::size_t kKickDescriptorFrameOffset = 0x228;
inline constexpr std::size_t kKickArg5Offset = 0x28, kKickArg6Offset = 0x30, kKickArg7Offset = 0x38, kKickArg8Offset = 0x40;
inline constexpr std::size_t kKickCmdFrameOffset = 0x400;     // cmd: action +0, record pointer +8, kind +0x1C
inline constexpr std::size_t kKickRecordFrameOffset = 0x420;  // the order record (kKickOrderRecordBytes)
inline constexpr std::size_t kKickArg8WordFrameOffset = 0x580;
inline constexpr std::size_t kCmdRecordPtrOffset = 0x8, kCmdKindOffset = 0x1C;

// The Kick start's own arguments (FUN_140724CC0): r9 is the info record whose +0xB8 is the cmd (it passes r9+0xB8 to the builder at
// 0x140724DBC; cmd +0x1C is r9+0xD4, read at 0x140724D57), and the fifth argument, at the entry rsp + 0x28 (kKickArg5Offset), points
// to the output record: +0 and +4 words, +0x24 a vector, +0x3C the Animetable motion the cell named, +0x50 a count. The record sits above the order in the
// frame; the info record is the frame bytes below the cmd.
inline constexpr std::size_t kKickInfoCmdOffset = 0xB8;
inline constexpr std::size_t kKickInfoFrameOffset = kKickCmdFrameOffset - kKickInfoCmdOffset;
inline constexpr std::size_t kKickOutFrameOffset = 0x5A0, kKickOutBytes = 0x58, kKickOutMotionOffset = 0x3C;
// What the Kick start stores in the kicker's P on a start (0x140724F25..0x140724F4E): the kind (u16), band, bucket and side (bytes).
inline constexpr std::size_t kPKickKindOffset = 0x2752, kPKickBandOffset = 0x2754, kPKickBucketOffset = 0x2755,
                             kPKickSideOffset = 0x2756;

// The Kick start's answer: started = it returned 1 (the cell exists: PES starts the Kick action); the kind, band, bucket and side it
// stored in P and the Animetable motion it wrote to the output record.
struct PesKickStartResult {
  bool started = false;
  int kind = kKickNoKind, band = 0, bucket = 0, side = 0, motion = 0;
};

}  // namespace pes
