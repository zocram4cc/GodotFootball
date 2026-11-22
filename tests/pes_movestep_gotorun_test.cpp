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
#include "pes/movestep_turn.hpp"
#include "pes/vfs.hpp"

namespace {

constexpr uint64_t kAdjusterBusy = 0x141eb3320;  // FUN_141eb3320(P+0x4d0): the facing adjuster's state == 2
constexpr uint64_t kDelegates[] = {0x1406ce820, 0x1406d01d0, 0x1406d5d60, 0x1406d2590, 0x1406ccd00, 0x1406d38d0, 0x1406d2f60, 0x1406d2030, 0x1406d6780};
constexpr uint64_t kFootBand = 0x1406e80b0;
constexpr uint64_t kRateCeiling = 0x1406eadf0;

// The install tables the handler's helpers read beyond the shared ones.
struct GotoRunTables {
  pes::movestep::MoveTurnConstants turn;
  pes::SpeedRateTable speedRate;
};

class GotoRunHost : public RowHost {
 public:
  GotoRunHost(const Case& c, const Install& in, const GotoRunTables& tables)
      : RowHost(c, in.run, in.tables, in.playback, in.buckets, in.move, in.turn, *in.mbinfo), c_(c), tables_(tables) {}
  const pes::SpeedRateTable& SpeedRateTableView() const override { return tables_.speedRate; }
  float RecordF2468() const override { return F32(Le<uint32_t>(c_.probe.at("f2468"))); }  // P+0x2468, read directly (0x1406bdf4x)
  // P+0xbec (the yaw lock) is written outside the request words; the rows carry no memory image after the call, so it is accepted.
  void SetRecordF0bec(float) override {}
  const pes::movestep::MoveTurnConstants& MoveTurnConstantsView() const override { return tables_.turn; }
  // FUN_141eb50b0(P, out, count): the bone ring's sample `count` ticks back, written at the out pointer.
  void PositionAt(int index, float out[3]) const override {
    for (const Call* call : All(kRing))
      if (static_cast<int32_t>(call->reg[2]) == index) {
        for (int i = 0; i < 3; ++i) out[i] = F32(Le<uint32_t>(call->after[1], 4 * i));
        return;
      }
    throw movestep_test::UnboundRead("no ring call for index " + std::to_string(index));
  }
  bool FacingAdjusterBusy() const override { return (Need(kAdjusterBusy).rax & 0xff) != 0; }
  // P+0x245d, the state before the current one: the probe the rows carry (the handler reads it directly, 0x1406be573).
  int MoveStatePrevious() const override { return Le<uint8_t>(c_.probe.at("prev")); }
  // vt+0x78 of +0xac0 (FUN_1406e80b0, 0x1406bec86): the foot band; vt+0x?? 1406eadf0 is movss xmm0,[P+0x2cbc], the rate ceiling.
  int FootBand() const override { return static_cast<int32_t>(Need(kFootBand).rax); }
  float RateCeiling() const override { return F32(Need(kRateCeiling).xmm0); }
  bool Delegates() const {
    for (const uint64_t target : kDelegates)
      if (First(target) != nullptr) return true;
    return false;
  }
  // A bool entry (FUN_1406d01d0, 1406ccd00, 1406d38d0, 1406d2f60) that answered false hands the tick back: the handler goes on.
  bool TookADelegate() const {
    for (const uint64_t target : kDelegates) {
      const bool boolEntry = target == 0x1406d01d0 || target == 0x1406ccd00 || target == 0x1406d38d0 || target == 0x1406d2f60;
      for (const Call* call : All(target))
        if (!boolEntry || (call->rax & 0xff) != 0) return true;
    }
    return false;
  }

 private:
  const Case& c_;
  const GotoRunTables& tables_;
};

}  // namespace

TEST(GotoRunRows, GotoRunWritesPesRequestWordsOnItsRows) {
  const char* rows = std::getenv("GF_PES_GOTORUN_ORACLE");
  if (!rows) GTEST_SKIP() << "GF_PES_GOTORUN_ORACLE unset";
  Install in;
  std::string skip;
  if (!LoadInstall(in, &skip)) GTEST_SKIP() << skip;
  std::string err;
  auto exe = pes::PeImage::Open(std::string(std::getenv("PES21_ROOT")) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  const auto turnConstants = pes::movestep::LoadMoveTurnConstants(*exe, &err);
  ASSERT_TRUE(turnConstants) << err;
  const auto speedRate = pes::LoadSpeedRateTable(*exe, &err);
  ASSERT_TRUE(speedRate) << err;
  const GotoRunTables tables{*turnConstants, *speedRate};
  const pes::PesClipReaders readers(*in.mbinfo);
  in.extra.ticks = [&readers](pes::ClipReader reader, int motion, float rate, int index) {
    return readers.Read(reader, motion, rate, index, 54.0f);
  };
  const std::vector<Case> cases = LoadCases(rows);
  ASSERT_FALSE(cases.empty());
  int real = 0, proven = 0, delegated = 0;
  for (const Case& c : cases) {
    SCOPED_TRACE(c.label + " " + c.mode);
    GotoRunHost host(c, in, tables);
    host.extra = &in.extra;
    pes::MoveReq req;
    for (size_t i = 0; i < c.reqIn.size(); ++i) req.SetU(static_cast<int>(i), c.reqIn[i]);
    bool reachedDelegate = false;
    try {
      pes::movestep::GotoRun(host, req);
    } catch (const movestep_test::UnboundRead& e) {
      if (!host.Delegates()) {  // PES finished in the body; the port read what PES did not
        ADD_FAILURE() << c.label << ": " << e.what();
        continue;
      }
      reachedDelegate = true;
    }
    if (reachedDelegate) {
      ++delegated;
    } else {
      EXPECT_FALSE(host.TookADelegate()) << "PES called a ported handler the port never reached";
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
