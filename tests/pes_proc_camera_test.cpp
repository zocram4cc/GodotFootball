// GF's FixDemo procedural camera rig (pes::FixDemoCameraRig: the translated FixDemoCamera constructor 0x141F7AE60 and per-frame
// engine 0x141F7BFE0 on a GF-owned arena, with GF's own CAMERA_TARGET registry) against PES's own pose for three consecutive
// frames of a live-captured goal's first step (the behind-the-goal telephoto: camera record mode 4, no CANM clip).
//
// Oracle: rendezvous captures of the hook proc_camera_141F7BFE0, live2/pid3234533 seq6220 (the init call, camera+0x128 == 1),
// seq6224 and seq6228 (the next two frames, +0x128 == 2). Each case pins what tools/dev/cutscene/proc_camera_cases.py reads
// from the snapshot at the call (the camera record behind camera+0x130, the CAMERA_TARGET_0201 record the scorer's actor held,
// the player time the time setter left in camera+0x214) and what PES's own bytes answered (the params block 0x1437F4A40 after
// emulating the call). The record and the target are the only inputs: the rig builds everything else, so a tie of the full
// 0x60-byte block (eye, target, up, fov, near, far and the rig's other outputs) justifies every stand-in in the rig.
//
// Needs the unpacked image (PES_EXE); skips without it.

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/fixdemo_camera_rig.hpp"

namespace {

std::vector<uint8_t> Hex(const std::string& h) {
  std::vector<uint8_t> out(h.size() / 2);
  for (size_t i = 0; i < out.size(); ++i) out[i] = static_cast<uint8_t>(std::stoul(h.substr(2 * i, 2), nullptr, 16));
  return out;
}

float FloatAt(const std::vector<uint8_t>& b, size_t off) {
  float f;
  std::memcpy(&f, b.data() + off, 4);
  return f;
}

// The registry record (0x30 bytes: a header word, then the info the readers 0x14089AD80 / 0x14089AC40 take) as the target.
pes::PesCameraRegistry::Target TargetOf(const std::string& recordHex) {
  const std::vector<uint8_t> r = Hex(recordHex);
  pes::PesCameraRegistry::Target t;
  t.valid = r[4] != 0;
  for (size_t i = 0; i < 3; ++i) {
    t.ground[i] = FloatAt(r, 0x08 + 4 * i);
    t.pointA[i] = FloatAt(r, 0x14 + 4 * i);
    t.pointB[i] = FloatAt(r, 0x20 + 4 * i);
  }
  t.facingDeg = FloatAt(r, 0x2C);
  return t;
}

const char* kRecord =
    "0000000000000000ffff0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000008be53441000096430000803f000000000000403f0001010000000000000000000400ffff"
    "32000000ffffffffffffffff00000000ffffffffffffffffffffffffffffffffffffffff323200640000000000000064000000000000000000000001"
    "0000000000005c42000000003333b33f3333334000005c420000c03f0000c03f0000c03f000000000000f041";

struct Frame {
  uint32_t timeBits;
  const char* scorer;  // CAMERA_TARGET_0201's record
  const char* params;  // the params block after the call
};

const Frame kFrames[] = {
    {0x00000000,
     "01000000010000002767164200000000f68835c1fca71642c8e9cb3f2ade36c13f621642586b703f5f1c35c126fc1543",
     "5c881642c817aa3f02ab37c100005e429a99193f000050c15ec0253deec97f3f96ba61bb5c881642c717aa3f03ab37c194dd953d8227bd400000fa437493a8"
     "3f000000000000000001000100ea918f413333b33f6f12033b00005c420000c03f"},
    {0x3f8e38e4,
     "0100000001000000a19616420000000041df36c1e9d416428d23cc3f834138c13c8f164231dc703fd77536c126fc1543",
     "c2b516428a51aa3faa0b39c100005e429a99193f000050c1d4b0263d5bc97f3f5eb056bbc2b516428a51aa3faa0b39c1794b963d3a82bb400000fa437493a8"
     "3f0000000000000000010001001d298f413333b33f6f12033b00005c420000c03f"},
    {0x400e38e4,
     "01000000010000001bc61642000000008c3538c1200217421b9bcc3f0c9d39c13fbc1642f2c1713f13cf37c126fc1543",
     "8ce31642d4f8aa3f7a673ac100005e429a99193f000050c10c65283d46c87f3f508f4cbb8ce31642d4f8aa3f7a673ac1794b963d466eba400000fa433018a8"
     "3f000000000000000001000100c3c08e413333b33f6f12033b00005c420000c03f"},
};

constexpr unsigned kScorerSlot = 1;  // CAMERA_TARGET_0201 held the scorer; the record's target byte (+0xB1) is 0, the actor's slot + 1

class ProcCamera : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* path = std::getenv("PES_EXE");
    if (!path) GTEST_SKIP() << "PES_EXE (the unpacked image) is not set";
    std::string err;
    auto exe = pes::PeImage::Open(path, &err);
    ASSERT_TRUE(exe) << err;
    rig = pes::FixDemoCameraRig::Create(*exe, &err);
    ASSERT_TRUE(rig) << err;
  }
  std::unique_ptr<pes::FixDemoCameraRig> rig;
};

TEST_F(ProcCamera, ThreeCapturedFramesTieToPesBitForBit) {
  const size_t cam = rig->AddCamera(Hex(kRecord));
  for (const Frame& f : kFrames) {
    float t;
    std::memcpy(&t, &f.timeBits, 4);
    rig->SetTarget(kScorerSlot, TargetOf(f.scorer));
    const pes::CameraPose pose = rig->Step(cam, t);
    const std::vector<uint8_t> want = Hex(f.params);
    ASSERT_EQ(want.size(), pes::kCameraParamsBytes);
    for (size_t i = 0; i < want.size(); ++i) EXPECT_EQ(pose.raw[i], want[i]) << "frame time bits " << std::hex << f.timeBits << " params +0x" << i;
  }
}

// The target is read from the registry slot the record names, not from any other: the scorer in slot 0 poses a camera that
// frames nothing.
TEST_F(ProcCamera, TheTargetComesFromTheRecordsSlot) {
  const size_t cam = rig->AddCamera(Hex(kRecord));
  float t = 0;
  rig->SetTarget(0, TargetOf(kFrames[0].scorer));
  const pes::CameraPose pose = rig->Step(cam, t);
  const std::vector<uint8_t> want = Hex(kFrames[0].params);
  EXPECT_NE(std::memcmp(pose.raw.data(), want.data(), pes::kCameraParamsBytes), 0);
}

}  // namespace
