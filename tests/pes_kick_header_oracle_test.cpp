// PES's kick request builder FUN_140833CD0 (translated, capture-bounded closure) on GF's own window, from the recorded
// kick_header_833CD0 calls (docs/ports/pes-port-KickHeader.md). Each capture's inputs are rebuilt as in the chooser oracle
// (pes_kick_oracle_common.hpp); the order (cmd and its record) is read from the capture's memory and handed to
// PesWorld::RunKickRequest, which lays it out in the window frame.
//
//  - the builder's request equals PES's on every byte the reference call wrote;
//  - the chooser answers the same kind, band, bucket, side and flag A on the builder's request as on PES's own request;
//  - the order GF builds from the fields its decision holds (pes::KickOrderFor) gets the same chooser answer.
//
// GF_PES_KICK_HEADER_ORACLE names a directory of per-capture directories (PES-derived, never committed), each made by
// tools/dev/translate/trap_oracle.py from a snapdiff case of 0x140833CD0. PES_EXE names the unpacked image. Without either, skip.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "onthepitch/peskickchooser.hpp"
#include "pes_kick_oracle_common.hpp"

namespace {

using namespace pes_kick_oracle;

// PES's request: the bytes the reference call wrote into [rcx, rcx + kKickRequestBytes), from its write log.
std::map<size_t, uint8_t> ReferenceRequest(const Oracle& o) {
  std::map<size_t, uint8_t> bytes;
  const uint64_t rcx = o.regs.at("rcx");
  const std::string w = Field(o.row, "w");
  for (size_t at = 0; at + 18 <= w.size(); at += 18) {
    const uint64_t addr = std::strtoull(w.substr(at, 16).c_str(), nullptr, 16);
    if (addr < rcx || addr >= rcx + pes::kKickRequestBytes) continue;
    bytes[addr - rcx] = static_cast<uint8_t>(std::strtoul(w.substr(at + 16, 2).c_str(), nullptr, 16));
  }
  return bytes;
}

// The capture's order: cmd = r8 (action +0, record pointer +8, kind +0x1C) and the record it points to.
pes::PesKickOrder CapturedOrder(const Oracle& o) {
  const uint64_t cmd = o.regs.at("r8");
  pes::PesKickOrder order;
  order.action = static_cast<int32_t>(Read(o, cmd, 4));
  order.kind = static_cast<int32_t>(Read(o, cmd + pes::kCmdKindOffset, 4));
  const uint64_t record = Read(o, cmd + pes::kCmdRecordPtrOffset, 8);
  for (size_t i = 0; i < pes::kKickOrderRecordBytes; ++i) order.record[i] = static_cast<uint8_t>(Read(o, record + i, 1));
  return order;
}

float RecordF32(const pes::PesKickOrder& order, size_t offset) {
  float v;
  std::memcpy(&v, order.record.data() + offset, sizeof v);
  return v;
}

uint32_t RecordU32(const pes::PesKickOrder& order, size_t offset) {
  uint32_t v;
  std::memcpy(&v, order.record.data() + offset, sizeof v);
  return v;
}

// The Kick object's stamp the start passes as the sixth argument (the stack word at entry+0x30 of the capture).
void SetKickStamp(const Oracle& o, Built& b) {
  const uint32_t stamp = static_cast<uint32_t>(Read(o, o.regs.at("rsp") + pes::kKickArg6Offset, 4));
  std::memcpy(b.world->MatchInfo() + pes::kMiFrameOffset, &stamp, sizeof stamp);
}

struct Answer {
  int32_t kind, band, bucket, side;
  uint8_t flagA;
  bool operator==(const Answer& a) const {
    return kind == a.kind && band == a.band && bucket == a.bucket && side == a.side && flagA == a.flagA;
  }
};
std::ostream& operator<<(std::ostream& os, const Answer& a) {
  return os << "{kind " << a.kind << " band " << a.band << " bucket " << a.bucket << " side " << a.side << " flagA " << int(a.flagA) << "}";
}
Answer AnswerOf(const pes::PesKickDescriptor& d) { return {d.ChosenKind(), d.Band(), d.Bucket(), d.Side(), d.FlagA()}; }

class KickHeaderOracle : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_KICK_HEADER_ORACLE");
    const char* exe = std::getenv("PES_EXE");
    if (dir == nullptr || *dir == '\0' || exe == nullptr || *exe == '\0')
      GTEST_SKIP() << "GF_PES_KICK_HEADER_ORACLE / PES_EXE unset (PES-derived captures and image, not committed)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv")) dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory under " << dir;
    exe_ = exe;
  }
  std::vector<std::string> dirs_;
  std::string exe_;
};

// The builder on GF's window reproduces PES's request on every byte the reference call wrote (15 real kick_header captures, 04-11-26:
// the manifest's MA ranges are copied into PesWorld::Ma(side) by Build, so the earlier INF "the window's MA holds ball and carrier only"
// no longer applies). The proof of the builder itself is the snapdiff record (results_kick.json and the kickbranch / kickleaf records),
// which the gate re-checks.
TEST_F(KickHeaderOracle, EveryCapturedOrderOnGfsWindowGivesPesRequest) {
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Image image;
    ASSERT_TRUE(image.Open(exe_));
    Built b;
    Build(o, image.bytes, image.size, b, nullptr, "rcx");
    SetKickStamp(o, b);
    const pes::PesKickRequest got = b.world->RunKickRequest(kSlot, CapturedOrder(o), b.ext);
    const std::map<size_t, uint8_t> want = ReferenceRequest(o);
    ASSERT_FALSE(want.empty()) << dir << ": the reference wrote no request byte";
    for (const auto& [off, value] : want) {
      // The actor pointer (request +0x110..) is the window's P, whatever address the capture's P had.
      if (off >= pes::kKickRequestActorOffset && off < pes::kKickRequestActorOffset + 8) continue;
      EXPECT_EQ(got.bytes[off], value) << dir << " request+0x" << std::hex << off;
    }
  }
}

// The chooser answers the same kind, band, bucket, side and flag A on the builder's request as on PES's own request, and on the request
// of the order GF builds. NOTE: PES's own answer is "none" (0x97) on all 15 captures: the Kick start re-picks every tick of the run-up and
// answers none until the strike (docs/ports/pes-port-KickBranch2.md), so this test pins the none path and the builder's request, not a real kind.
TEST_F(KickHeaderOracle, TheChooserAnswersAsPesDoesOnTheBuiltRequestAndOnGfsOrder) {
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Image image;
    ASSERT_TRUE(image.Open(exe_));
    Built b;
    Build(o, image.bytes, image.size, b, nullptr, "rcx");
    SetKickStamp(o, b);
    // PES's answer: the proven chooser on PES's own request.
    pes::PesKickRequest pes_request;
    for (const auto& [off, value] : ReferenceRequest(o)) pes_request.bytes[off] = value;
    const Answer want = AnswerOf(b.world->RunKickChooser(kSlot, pes_request, b.ext));
    const pes::PesKickOrder captured = CapturedOrder(o);
    const Answer built = AnswerOf(b.world->RunKickChooser(kSlot, b.world->RunKickRequest(kSlot, captured, b.ext), b.ext));
    EXPECT_EQ(built, want) << dir << ": the captured order through the builder";
    // GF's order: only the fields its decision holds.
    const float target[3] = {RecordF32(captured, pes::kOrderTargetOffset), RecordF32(captured, pes::kOrderTargetOffset + 4),
                             RecordF32(captured, pes::kOrderTargetOffset + 8)};
    const pes::PesKickOrder gf = pes::KickOrderFor(captured.action, target, RecordU32(captured, pes::kOrderReceiverOffset),
                                                   RecordF32(captured, pes::kOrderHeadingOffset),
                                                   RecordF32(captured, pes::kOrderPowerOffset),
                                                   RecordF32(captured, pes::kOrderSpeedOffset));
    const Answer fed = AnswerOf(b.world->RunKickChooser(kSlot, b.world->RunKickRequest(kSlot, gf, b.ext), b.ext));
    EXPECT_EQ(fed, want) << dir << ": GF's order (action " << captured.action << ")";
  }
}

}  // namespace
