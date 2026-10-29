// The records PES's Trap action chain reads beside G, in PES's byte layout (docs/ports/pes-port-TrapGF.md): P, the actor
// object the action belongs to, and the request the action's +0x68 slot (0x1407A01C0) takes. The chain is the kind-0
// control touch 0x14079A470 and everything it calls (TrapChain2/3: proven bit-exact on seven captures).
//
// Offsets are the bytes the chain reads, measured as live-in on the captures (the bytes a call reads that it has not written
// itself; tools/dev/translate/trap_oracle.py), not P's whole layout. Names are provisional (prov_) where the meaning is INF.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pes {

// P (rdx of 0x14079A470): the actor object; its Trap action sits inside it at +0xC78 (rcx). 0x3B1C is the last byte read.
inline constexpr std::size_t kTrapPBytes = 0x3B40;
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
inline constexpr std::size_t kSlotPPtrTargetOffset = 0x15C0;  // the slot-record offset P+0x3AD8 names

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

// The window's trap member: P.
struct PesTrapImage {
  std::array<uint8_t, kTrapPBytes> p{};
};

// What the chain reads that GF does not hold as state yet (each a stand-in naming the PES site that removes it): the install's
// image (the vtables' bytes), the loader data the chain's vtable calls and registry lookups reach (the constant registry
// 0x1437F4808, the anime-table manager 0x1436F2378 and 0x1436F23C0, the motion table 0x1406B20E0's records), as bytes at
// their VAs, and the writable image globals that point at them.
struct PesTrapSegment {
  uint64_t va = 0;
  std::vector<uint8_t> bytes;
};
struct PesTrapExternals {
  uint8_t* image = nullptr;  // the image from base 0x140000000, writable, caller-owned (translated code writes globals)
  uint64_t imageBytes = 0;
  std::vector<PesTrapSegment> segments;
  std::vector<std::pair<uint64_t, uint64_t>> globals;  // (image VA, 8-byte value)
  std::vector<std::pair<uint64_t, uint8_t>>* writeLog = nullptr;  // every byte the call writes, in order (tests)
};

struct PesTrapResult {
  bool issued = false;   // rax low byte: 1 = a clip was committed
  uint32_t out = 0;      // the out word: a refusal status (1, 3, 7, 8, 9) or the selected candidate's key
};

inline constexpr uint64_t kImageBaseVa = 0x140000000ULL;

}  // namespace pes
