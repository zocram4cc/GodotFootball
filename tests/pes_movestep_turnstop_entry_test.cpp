// FUN_1406d2590 (pes::movestep::EnterTurnStop) against PES's own bytes. GF_PES_TURNSTOP_ORACLE names rows made by
// tools/dev/translate/handler_oracle.py with --entry 0x1406d2590 over the real captures of the movestep hooks (the request and
// the player objects of real calls, then perturbed variants of the request words and of the memory the handler reads, labelled
// SYNTHETIC: PES's own bytes answer every perturbed case). Per case the request words at the entry and at the return, the memory
// probes and the handler's own calls. The port runs on a host that answers every read from those rows; the stop cell (the
// interface's vt+0xd0, which the rows list as one call) is read from the install's animetable, and the issue's clip readers from
// PES's translated readers: the request words the port writes (motion, mirror, blend, heading stamp, history words) are checked
// against PES's. Without the rows (PES-derived, never committed) the cases skip. PES21_ROOT names the install.
#include "movestep_row_host.hpp"
#include "onthepitch/pesclipreaders.hpp"
#include "pes/animetable.hpp"
#include "pes/movestep_step.hpp"
#include "pes/vfs.hpp"

namespace {

constexpr uint64_t kRingJoint = 0x141eb51a0;  // FUN_141eb51a0(P, out, joint): the bone ring's joint position

class TurnStopHost : public RowHost {
 public:
  TurnStopHost(const Case& c, const Install& in, const pes::Animetable& table)
      : RowHost(c, in.run, in.tables, in.playback, in.buckets, in.move, in.turn, *in.mbinfo), c_(c), table_(table) {}

  float BodyDeg() const override { return F32(Le<uint32_t>(c_.ctx, 0x28)); }
  pes::MoveCell Cell(int row, int column, bool walk) override {
    EXPECT_FALSE(walk);
    const pes::AnimetableCell cell = table_.Cell(row, column);
    pes::MoveCell out;
    out.found = cell.motionNo >= 0;
    out.motionNo = cell.motionNo;
    out.mirror = cell.mirror;
    return out;
  }
  // FUN_141eb51a0 writes the joint's position at its out pointer: the call's out window after the call.
  void PositionAt(int index, float out[3]) const override {
    for (const Call* call : All(kRingJoint))
      if (static_cast<int32_t>(call->reg[2]) == index) {
        for (int i = 0; i < 3; ++i) out[i] = F32(Le<uint32_t>(call->after[1], 4 * i));
        return;
      }
    ADD_FAILURE() << "no joint call for index " << index;
    std::abort();
  }

 private:
  const Case& c_;
  const pes::Animetable& table_;
};

}  // namespace

TEST(TurnStopEntryRows, EnterTurnStopWritesPesRequestWordsOnItsRows) {
  const char* rows = std::getenv("GF_PES_TURNSTOP_ORACLE");
  if (!rows) GTEST_SKIP() << "GF_PES_TURNSTOP_ORACLE unset";
  Install in;
  std::string skip;
  if (!LoadInstall(in, &skip)) GTEST_SKIP() << skip;
  std::string err;
  auto exe = pes::PeImage::Open(std::string(std::getenv("PES21_ROOT")) + "/PES2021.exe", &err);
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(std::getenv("PES21_ROOT"), &err)) << err;
  auto table = pes::LoadAnimetable(*exe, vfs, &err);
  ASSERT_TRUE(table) << err;
  const pes::PesClipReaders readers(*in.mbinfo);
  in.extra.ticks = [&readers](pes::ClipReader reader, int motion, float rate, int index) {
    return readers.Read(reader, motion, rate, index, 54.0f);
  };
  const std::vector<Case> cases = LoadCases(rows);
  ASSERT_FALSE(cases.empty());
  int real = 0;
  for (const Case& c : cases) {
    SCOPED_TRACE(c.label + " " + c.mode);
    TurnStopHost host(c, in, *table);
    host.extra = &in.extra;
    pes::MoveReq req;
    for (size_t i = 0; i < c.reqIn.size(); ++i) req.SetU(static_cast<int>(i), c.reqIn[i]);
    pes::movestep::EnterTurnStop(host, req);
    for (size_t i = 0; i < c.reqOut.size(); ++i)
      EXPECT_EQ(req.U(static_cast<int>(i)), c.reqOut[i]) << "request word " << std::hex << i;
    if (c.mode == "real") ++real;
  }
  RecordProperty("cases", static_cast<int>(cases.size()));
  RecordProperty("real", real);
}
