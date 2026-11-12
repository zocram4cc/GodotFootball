// FUN_1406bdf10 (pes::movestep::GotoRun, the GOTO_RUN handler) against PES's own bytes. GF_PES_GOTORUN_ORACLE names rows made by
// tools/dev/translate/handler_oracle.py from the real calls of the capture hook mv_1406BDF10 (pid3597187 seq003 ... seq031) and
// perturbed variants of the request words, labelled SYNTHETIC; the callees still run PES's bytes, so every host answer is PES's
// own for that input. The port runs on a host that answers every read from the rows. A case whose rows contain a call into
// another ported handler (the entries FUN_1406ce820, 1406d01d0, 1406d5d60, 1406d2590, 1406ccd00, 1406d38d0, 1406d2f60) is a
// DELEGATED case: the rows carry that handler's own reads nowhere, so the port must reach a read the rows cannot answer and its
// request words are then not compared; every other case must finish on the rows and match word for word. Without the rows
// (PES-derived, never committed) the cases skip. PES21_ROOT names the install.
#include "movestep_row_host.hpp"
#include "onthepitch/pesclipreaders.hpp"
#include "pes/vfs.hpp"

namespace {

constexpr uint64_t kAdjusterBusy = 0x141eb3320;  // FUN_141eb3320(P+0x4d0): the facing adjuster's state == 2
constexpr uint64_t kDelegates[] = {0x1406ce820, 0x1406d01d0, 0x1406d5d60, 0x1406d2590, 0x1406ccd00, 0x1406d38d0, 0x1406d2f60};

class GotoRunHost : public RowHost {
 public:
  GotoRunHost(const Case& c, const Install& in) : RowHost(c, in.run, in.tables, in.playback, in.buckets, in.move, in.turn, *in.mbinfo) {}
  bool FacingAdjusterBusy() const override { return (Need(kAdjusterBusy).rax & 0xff) != 0; }
  bool Delegates() const {
    for (const uint64_t target : kDelegates)
      if (First(target) != nullptr) return true;
    return false;
  }
};

}  // namespace

TEST(GotoRunRows, GotoRunWritesPesRequestWordsOnItsRows) {
  const char* rows = std::getenv("GF_PES_GOTORUN_ORACLE");
  if (!rows) GTEST_SKIP() << "GF_PES_GOTORUN_ORACLE unset";
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
    GotoRunHost host(c, in);
    host.extra = &in.extra;
    pes::MoveReq req;
    for (size_t i = 0; i < c.reqIn.size(); ++i) req.SetU(static_cast<int>(i), c.reqIn[i]);
    bool reachedDelegate = false;
    try {
      pes::movestep::GotoRun(host, req);
    } catch (const movestep_test::UnboundRead&) {
      if (!host.Delegates()) throw;
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
