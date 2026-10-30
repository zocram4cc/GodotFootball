// The records PES's Trap action chain reads beside G, in PES's byte layout (docs/ports/pes-port-TrapGF.md): P, the actor
// object the action belongs to, and the request the action's +0x68 slot (0x1407A01C0) takes. The chain is the kind-0
// control touch 0x14079A470 and everything it calls (TrapChain2/3: proven bit-exact on seven captures).
//
// Offsets are the bytes the chain reads, measured as live-in on the captures (the bytes a call reads that it has not written
// itself; tools/dev/translate/trap_oracle.py), not P's whole layout. Names are provisional (prov_) where the meaning is INF.

#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <vector>

namespace pes {

// P (rdx of 0x14079A470): the actor object; its Trap action sits inside it at +0xC78 (rcx). The window holds P to the extent of its
// constructor FUN_1406E03D0: the last store is the member FUN_1406E07D0 at +0x3B90 (0x30 bytes). SetMotion (FUN_1406F0F70) writes
// +0x3B50..+0x3B8C, so a P cut at 0x3B40 let those stores land in the next window object (the physique chain's A).
// OPEN: PES's allocation may be larger; nothing past +0x3BC0 is read or written by the 14 captures.
inline constexpr std::size_t kTrapPBytes = 0x3BC0;
inline constexpr std::size_t kPActionOffset = 0xC78;  // the Trap action object (rcx of the chain)

// P+0x8: the actor's ground position (x, height, z; m), +0x14 its second point (the next frame's), +0x20 its heading
// (degrees, INF). 0x14079A470 measures the reach to the ball from +0x8/+0x10.
inline constexpr std::size_t kPPosOffset = 0x8;
inline constexpr std::size_t kPNextPosOffset = 0x14;
inline constexpr std::size_t kPHeadingOffset = 0x20;
// P+0x2FC: the actor's velocity (x, height, z; m per frame, INF).
inline constexpr std::size_t kPVelocityOffset = 0x2FC;
inline constexpr std::size_t kPWord430Offset = 0x430;  // u32
// P+0xA08: pointer to a record whose +0xF0 points to a record holding an f32 at +0xC0 (a per-actor scale, INF).
inline constexpr std::size_t kPScalePtrOffset = 0xA08;
// P+0xAB8 / +0xAC0: two embedded interface objects, each a vtable pointer into the image (the chain calls through them);
// +0xAC8..+0xAE1 are their fields. The vtables are P's class constants (its constructor stores them: OPEN, not decoded).
inline constexpr std::size_t kPInterfaceVtable0Offset = 0xAB8;
inline constexpr std::size_t kPInterfaceVtable1Offset = 0xAC0;
inline constexpr std::size_t kPByteAC8Offset = 0xAC8;
inline constexpr std::size_t kPByteAC9Offset = 0xAC9;
inline constexpr std::size_t kPWordAE0Offset = 0xAE0;  // u16
inline constexpr std::size_t kPByteB08Offset = 0xB08;
inline constexpr std::size_t kPByte2458Offset = 0x2458;
inline constexpr std::size_t kPPhaseOffset = 0x245C;      // u8: the locomotion phase (class table index; not read by the chain)
inline constexpr std::size_t kPPrevActionOffset = 0xACA;  // u8: the previous action id; +0xACB its sub id
inline constexpr std::size_t kPWord277COffset = 0x277C;   // f32; +0x2780 the next f32: the class writer's phase-1 test
inline constexpr std::size_t kPF377CScaleOffset = 0x377C;  // f32: the rate ceiling FUN_1406F99F0 stores (0x1406F9B4C)
inline constexpr std::size_t kPPtr3AC0Offset = 0x3AC0;     // pointer (read, handed on)
inline constexpr std::size_t kPSlotPtrOffset = 0x3AD8;     // pointer into the actor's slot record (+0x15C0)
inline constexpr std::size_t kPF3B18Offset = 0x3B18;       // f32
inline constexpr std::size_t kPPlaybackOffset = 0x3758;    // f32: the playback rate SetMotion stores (FUN_1406F0F70)
inline constexpr std::size_t kSlotPPtrTargetOffset = 0x15C0;  // the slot-record offset P+0x3AD8 names
// The constructor FUN_1406E03D0's non-zero stores in P's tail (0x1406E0xxx stores; the member at +0x3B90 is FUN_1406E07D0's).
inline constexpr std::size_t kPTailNoneOffsets[] = {0x3B50, 0x3B7C, 0x3BBC};  // u32 0xFFFFFFFF
inline constexpr std::size_t kPTailOneOffsets[] = {0x3B5C, 0x3BB4};           // f32 1.0 (0x3B5C is the low half of a u64 0x3F800000)

// The member object FUN_141EB42B0 constructs at P+8 (param_1; CONFIRMED against the 14 captures: its vtable store at param_1+0x420 is
// the word the capture holds at P+0x428). Offsets below are from param_1; the stores are the constructor's non-zero ones (the rest
// is zero, as the window's P starts). Immediates are the constructor's own (VAs on each line).
inline constexpr std::size_t kPMemberOffset = 0x8;
inline constexpr std::size_t kMemberRateOffset = 0x414;                    // f32 = [0x14256AE1C] / tick rate (0x141EB4333..0x141EB433F)
inline constexpr std::size_t kMemberVtableOffset[3] = {0x420, 0x490, 0x4C8};  // vtable stores 0x141EB437C, 0x141EB43D6, 0x141EB43FF
inline constexpr std::size_t kMemberNoneOffset[3] = {0x474, 0x476, 0x478};   // u16 0xFFFF (0x141EB4399..0x141EB43A1)
inline constexpr uint16_t kMemberNoneIndex = 0xFFFF;
inline constexpr std::size_t kMemberMinusOneOffset = 0x484, kMemberOneOffset = 0x488;  // f32 -1.0, 1.0 (0x141EB43A5, 0x141EB43AC)
inline constexpr std::size_t kMemberFlagsOffset = 0x4F8;                   // u16 0xFF00 (0x141EB4406)
inline constexpr uint16_t kMemberFlagsInit = 0xFF00;
inline constexpr std::size_t kMemberUnitTableOffset = 0x6F8, kMemberUnitTableCount = 0x26, kMemberUnitTableStride = 0x10,
                             kMemberUnitTableCellOffset = 0xC;             // f32 1.0 at +0xC of each of 0x26 entries (0x141EB4470..0x141EB4495)
inline constexpr std::size_t kMemberUnitRunOffset = 0x958, kMemberUnitRunCount = 0x28;  // 0x28 f32 1.0 (two 0x14-runs, 0x141EB44AB..0x141EB44D5)
inline constexpr std::size_t kMemberScaleOffset = 0x9F8, kMemberPairOffset = 0x9FC;     // f32 1.0; u32 0x10001 (0x141EB4497, 0x141EB44A1)
inline constexpr uint32_t kMemberPairInit = 0x10001;

// The request the Trap action's +0x68 slot takes (r9 of 0x1407A01C0), 0x24 bytes in PES's layout
// (docs/re/pes-re-trap-kind0.md 1). The dispatcher copies the words below into its own frame and calls the kind's chain; kind <= 0 is
// the control touch. Only the fields kind 0 forwards are named.
inline constexpr std::size_t kRequestBytes = 0x28;
inline constexpr std::size_t kReqWord0Offset = 0x0;   // u32 (an f32 in the captures), copied to the dispatcher's frame: r9 of the chain
inline constexpr std::size_t kReqWord1Offset = 0x4;   // u32: the chain's 5th argument
inline constexpr std::size_t kReqKindOffset = 0x8;    // s32
inline constexpr std::size_t kReqWord3Offset = 0xC;   // u32: the chain's 7th argument
inline constexpr std::size_t kReqFlagAOffset = 0x1C;  // u8: the chain's 13th argument (a flag the admission step tests)
inline constexpr std::size_t kReqByte1DOffset = 0x1D; // u8: stored at the action +0x3D by the dispatcher
inline constexpr std::size_t kReqByte1EOffset = 0x1E; // u8: the chain's 9th argument
inline constexpr std::size_t kReqByte1FOffset = 0x1F; // u8: the chain's 10th argument
inline constexpr std::size_t kReqByte20Offset = 0x20; // u8: the chain's 11th argument
inline constexpr std::size_t kReqByte21Offset = 0x21; // u8: stored at the action +0x3E by the dispatcher
inline constexpr std::size_t kReqByte22Offset = 0x22; // u8: the chain's 12th argument
inline constexpr std::size_t kActionByte3DOffset = 0x3D;
inline constexpr std::size_t kActionByte3EOffset = 0x3E;
struct PesTrapRequest {
  std::array<uint8_t, kRequestBytes> bytes{};
};

// The caller's frame the chain reads, at the window stack's top: the dispatcher's frame (its copy of the request's first
// word at +0xA8, the out word's slot at +0x228) above the entry rsp. Offsets are from the entry rsp.
inline constexpr std::size_t kTrapFrameBytes = 0x300;
inline constexpr std::size_t kFrameArg5Offset = 0x28;   // the chain's stack arguments, one qword each (5th at +0x28)
inline constexpr std::size_t kFrameOutPtrOffset = 0x30; // 6th: the pointer to the out word
inline constexpr std::size_t kFrameArg7Offset = 0x38;
inline constexpr std::size_t kFrameArg8Offset = 0x40;
inline constexpr std::size_t kFrameArg9Offset = 0x48;
inline constexpr std::size_t kFrameArg10Offset = 0x50;
inline constexpr std::size_t kFrameArg11Offset = 0x58;
inline constexpr std::size_t kFrameArg12Offset = 0x60;
inline constexpr std::size_t kFrameArg13Offset = 0x68;
inline constexpr std::size_t kFrameRequestCopyOffset = 0xA8;  // the dispatcher's copy of request word 0 (r9)
inline constexpr std::size_t kFrameOutWordOffset = 0x228;     // the out word: a refusal status, or the selected candidate's key

// The physique chain P+0xA08 reaches (FUN_141AF3180: *(*(P+0xA08)+0xF0)+0xC0 is G): two small heap objects in PES, here one
// after P in the window. Their other bytes are not read by the chain.
inline constexpr std::size_t kPhysiqueObjectBytes = 0x200;  // 0x100 for the chain's reads; the tick reads A+0x118 and A+0x188
inline constexpr std::size_t kPhysiqueNextOffset = 0xF0;  // object A +0xF0: the VA of object B
inline constexpr std::size_t kPhysiqueGOffset = 0xC0;     // object B +0xC0: G (f32)
inline constexpr std::size_t kPGOffset = 0xA00;           // P+0xA00: the clamped G FUN_141EB5810 stores beside the chain's copy

// The anime layer's objects the action tick (FUN_140797CD0) reaches from P, in PES's layout (docs/ports/pes-port-TrapTick.md 2;
// bytes the 14 captured ticks read or write, names INF): A (P+0xA08 target) holds two children, O1 at +0x118 and O2 at +0x188;
// O2+0xD0 is the root's world point (x, y, z, 1) which SetMotion's solver reads and writes; Q (P+0x3AA8 target) points to R at +8, and
// R is a joint chain header (+0x10 -> its pointer array at +0x30, +0x18 a length, +0x20 the joint count); S (P+0x3AE8 target) is a
// small record whose +0x14 SetMotion writes.
inline constexpr std::size_t kAChildO1Offset = 0x118, kAChildO2Offset = 0x188;
inline constexpr std::size_t kO1Bytes = 0x600, kO2Bytes = 0x100, kQBytes = 0x10, kRBytes = 0x90, kSBytes = 0x20;
inline constexpr std::size_t kO2PointOffset = 0xD0;  // f32 x, y, z, w
inline constexpr std::size_t kQChainPtrOffset = 8;   // Q+8 -> R
inline constexpr std::size_t kRArrayPtrOffset = 0x10, kRArrayOffset = 0x30;
inline constexpr std::size_t kPQPtrOffset = 0x3AA8, kPSPtrOffset = 0x3AE8;  // P -> Q, P -> S
inline constexpr std::size_t kPSegmentOffset = 0x2471, kPMirrorOffset = 0x2472;  // u8: the clip's segment index, the mirror byte
inline constexpr std::size_t kPActionVtableOffset = kPActionOffset;  // the Trap action's vtable word (constructor 0x140795630)

// The caller's frame object F (*r8 of the tick): its +0x870 is the ball record; the other fields the captured ticks read are
// named in docs/ports/pes-port-TrapTick.md 3.
inline constexpr std::size_t kTickFrameBytes = 0x880, kTickFrameXBytes = 0x600;
inline constexpr std::size_t kFrameXPtrOffset = 0x868, kFrameBallOffset = 0x870;
inline constexpr std::size_t kFrameHeadingOffset = 0x744, kFrameKindOffset = 0x748;  // f32 degrees, s32
inline constexpr std::size_t kFrameXHeadingOffset = 0x5E4;                           // F+0x868's target: f32 degrees at +0x5E4
inline constexpr std::size_t kTickEntryFrameSlotOffset = 0x48;  // the tick's r8: a stack slot (entry rsp + 0x48) holding F

// The window's trap member: P, then the physique chain's two objects and the layer objects the tick reaches.
struct PesTrapImage {
  std::array<uint8_t, kTrapPBytes> p{};
  std::array<uint8_t, kPhysiqueObjectBytes> physiqueA{};
  std::array<uint8_t, kPhysiqueObjectBytes> physiqueB{};
  std::array<uint8_t, kO1Bytes> o1{};
  std::array<uint8_t, kO2Bytes> o2{};
  std::array<uint8_t, kQBytes> q{};
  std::array<uint8_t, kRBytes> r{};
  std::array<uint8_t, kSBytes> s{};
};
inline constexpr std::size_t kTrapO1Offset = kTrapPBytes + 2 * kPhysiqueObjectBytes;
inline constexpr std::size_t kTrapO2Offset = kTrapO1Offset + kO1Bytes;
inline constexpr std::size_t kTrapQOffset = kTrapO2Offset + kO2Bytes;
inline constexpr std::size_t kTrapROffset = kTrapQOffset + kQBytes;
inline constexpr std::size_t kTrapSOffset = kTrapROffset + kRBytes;

// The facts GF writes into the tick's frame F and F+0x868's object before each tick (PesWorld::SetTickFrame; the oracle supplies the
// same bytes from the capture). INF, from 14 captures: F+0x744 is the planned touch's heading (degrees, PES yaw) and F+0x748 its kind
// (3 for a control touch, 4 for the directional one, which the request kind 0 and 4 map to); F+0x5E4 is 0.6 for kind 3 and 1.0 for
// kind 4 (a weight, its writer is the tick's caller's caller: OPEN); F+0x868's object holds the body's heading (degrees) at +0x5E4.
inline constexpr int32_t kTickKindControl = 3, kTickKindDirectional = 4;
// ponytail: F+0x5E4, the weight the captures show per kind (0.6 control, 1.0 directional, read off 14 ticks). The tick's caller's caller
// writes it from a source not decoded; the decode of that writer removes these two numbers.
inline constexpr std::size_t kFrameWeightOffset = 0x5E4;
inline constexpr float kTickWeightControl = 0.6f, kTickWeightDirectional = 1.0f;

// What a tick leaves for the body: the motion number the anime layer plays (P+0xAE0), its mirror byte (P+0x2472), the segment
// index (P+0x2471), the playback rate (P+0x3758) and the phase byte (P+0x245C) the next tick switches on.
struct PesTickResult {
  uint16_t motion = 0;
  uint8_t mirror = 0;
  uint8_t segment = 0;
  uint8_t phase = 0;
  float playback = 0.0f;
};

// The clip a successful call commits: the Trap action object's u16 at +8 (P+0xC80), a motion number of the Animetable
// (docs/re/pes-re-trap-inputs.md 3.2; `out[0]` is a refusal status and nothing else). `p` is the actor's P.
inline constexpr std::size_t kActionClipOffset = 0x8;
inline uint16_t CommittedTrapClip(const uint8_t* p) {
  uint16_t clip;
  std::memcpy(&clip, p + kPActionOffset + kActionClipOffset, sizeof clip);
  return clip;
}

// What the chain reads that GF does not hold as state yet (each a stand-in naming the PES site that removes it): the install's
// image (the vtables' bytes), the loader data the chain's vtable calls and registry lookups reach (the constant registry
// 0x1437F4808, the anime-table manager 0x1436F2378 and 0x1436F23C0, the motion table 0x1406B20E0's records), as bytes at
// their VAs, and the writable image globals that point at them.
struct PesTrapSegment {
  uint64_t va = 0;
  std::vector<uint8_t> bytes;
};
struct PesTrapExternals {
  uint8_t* image = nullptr;  // the image's bytes from imageBase, writable, caller-owned (translated code writes globals)
  uint64_t imageBase = 0x140000000ULL;  // the VA of image[0]: a slice of the exe's data sections, or the whole flat image
  uint64_t imageBytes = 0;
  std::vector<PesTrapSegment> segments;  // lent to a call (PesWorld::RunTrapKind0 moves them in and back)
  std::vector<std::pair<uint64_t, uint64_t>> globals;  // (image VA, 8-byte value)
  std::vector<std::pair<uint64_t, uint8_t>>* writeLog = nullptr;  // every byte the call writes, in order (tests)
};

struct PesTrapResult {
  bool issued = false;   // rax low byte: 1 = a clip was committed
  uint32_t out = 0;      // the out word: a refusal status (1, 3, 7, 8, 9) or the selected candidate's key
};

inline constexpr uint64_t kImageBaseVa = 0x140000000ULL;

}  // namespace pes
