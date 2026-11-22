// FUN_1406c6480 (pes::movestep::StepLoopChange, STEP_LOOP_CHANGE) against PES's own bytes. GF_PES_STEPCHANGE_ORACLE names rows made
// by tools/dev/translate/handler_oracle.py over the real captures of the stepaudit_1406C6480 hook (and perturbed variants of the
// request words, labelled SYNTHETIC: PES's own bytes answer every perturbed case): per case the request words at the entry and at
// the return and the handler's own calls. The port runs on a host that answers every read from those rows and its written request
// words are checked against PES's. A case whose rows contain a call into another ported handler (FUN_1406d5d60 stop, 1406d38d0,
// 1406ce820 goto-run, 1406d2f60, 1406ccd00) is a DELEGATED case: the rows carry that handler's own reads nowhere, so the port must
// reach a read the rows cannot answer (PES made the call, so the port must too) and its request words are then not compared; it is
// proven only up to the delegation (those handlers have their own rows). Every other case must finish on the rows and match word
// for word. Without the rows (PES-derived, never committed) the cases skip. PES21_ROOT names the install.
#include "movestep_row_host.hpp"
#include "onthepitch/pesclipreaders.hpp"
#include "pes/movestep_step.hpp"
#include "pes/vfs.hpp"

namespace {

constexpr uint64_t kRateBand = 0x1406b9e50;  // +0xac0's class -> rate (called twice, the answers dropped by PES)
constexpr uint64_t kPhysique = 0x141af3180;  // FUN_141af3180(P+0xa08): G
// The ported handlers FUN_1406c6480 tail-calls: their calls in the rows mark a delegated case.
constexpr uint64_t kDelegates[] = {0x1406d5d60, 0x1406d38d0, 0x1406ce820, 0x1406d2f60, 0x1406ccd00};

class StepChangeHost : public RowHost {
 public:
  StepChangeHost(const Case& c, const Install& in)
      : RowHost(c, in.run, in.tables, in.playback, in.buckets, in.move, in.turn, *in.mbinfo) {}

  float PhysiqueG() const override { return F32(Need(kPhysique).xmm0); }
  // FUN_141eb50b0(P, out, count): the bone ring's sample `count` ticks back, written at the out pointer.
  void PositionAt(int index, float out[3]) const override {
    for (const Call* call : All(kRing))
      if (static_cast<int32_t>(call->reg[2]) == index) {
        for (int i = 0; i < 3; ++i) out[i] = F32(Le<uint32_t>(call->after[1], 4 * i));
        return;
      }
    throw movestep_test::UnboundRead("no ring call for index " + std::to_string(index));
  }
  // vt+0x18 of +0xac0 (the rate ceiling) is answered by the first FUN_1406b9e50 row's xmm0 (the interface's own ceiling call).
  float RateCeiling() const override { return F32(Need(kRateBand).xmm0); }

  bool Delegates() const {
    for (const uint64_t target : kDelegates)
      if (First(target) != nullptr) return true;
    return false;
  }
};

}  // namespace

TEST(StepChangeRows, StepLoopChangeWritesPesRequestWordsOnItsRows) {
  const char* rows = std::getenv("GF_PES_STEPCHANGE_ORACLE");
  if (!rows) GTEST_SKIP() << "GF_PES_STEPCHANGE_ORACLE unset";
  Install in;
  std::string skip;
  if (!LoadInstall(in, &skip)) GTEST_SKIP() << skip;
  const pes::PesClipReaders readers(*in.mbinfo);
  in.extra.ticks = [&readers](pes::ClipReader reader, int motion, float rate, int index) {
    return readers.Read(reader, motion, rate, index, 54.0f);
  };
  const std::vector<Case> cases = LoadCases(rows);
  ASSERT_FALSE(cases.empty());
  int real = 0, proven = 0, delegated = 0;
  for (const Case& c : cases) {
    SCOPED_TRACE(c.label + " " + c.mode);
    StepChangeHost host(c, in);
    host.extra = &in.extra;
    pes::MoveReq req;
    for (size_t i = 0; i < c.reqIn.size(); ++i) req.SetU(static_cast<int>(i), c.reqIn[i]);
    bool reachedDelegate = false;
    try {
      pes::movestep::StepLoopChange(host, req);
    } catch (const movestep_test::UnboundRead&) {
      if (!host.Delegates()) throw;  // PES finished in the body; the port read what PES did not
      reachedDelegate = true;
    }
    if (reachedDelegate) {
      ++delegated;
    } else {
      EXPECT_FALSE(host.Delegates()) << "PES called a ported handler the port never reached";
      for (size_t i = 0; i < c.reqOut.size(); ++i)
        EXPECT_EQ(req.U(static_cast<int>(i)), c.reqOut[i]) << "request word " << std::hex << i;
      ++proven;
    }
    if (c.mode == "real") ++real;
  }
  RecordProperty("cases", static_cast<int>(cases.size()));
  RecordProperty("real", real);
  RecordProperty("proven", proven);
  RecordProperty("delegated", delegated);
}
