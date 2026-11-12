// FUN_1406d1390 (pes::movestep::EnterIdleTurn, the idle family's turn exit into IDLE_TURN) against PES's own bytes.
// GF_PES_IDLETURN_ORACLE names rows made by tools/dev/translate/handler_oracle.py from the 12 real calls of the capture hook
// mv_1406D1390 (pid3597187 seq005 ... seq039) and perturbed variants of the request words and of the memory the handler reads
// directly (the match phase match+0x116c, the route flag G+0x25930, the anime state P+0xac9, the facing P+0x20), labelled
// SYNTHETIC; the callees still run PES's bytes, so every host answer is PES's own for that input. Per case: the request words
// at the entry and at the return, the memory probes and the handler's own calls. The port runs on a host that answers every read
// from those rows and must write PES's request words. Without the rows (PES-derived, never committed) the cases skip.
// PES21_ROOT names the install.
#include "movestep_row_host.hpp"
#include "onthepitch/pesclipreaders.hpp"
#include "pes/vfs.hpp"

namespace {

class IdleTurnHost : public RowHost {
 public:
  IdleTurnHost(const Case& c, const Install& in)
      : RowHost(c, in.run, in.tables, in.playback, in.buckets, in.move, in.turn, *in.mbinfo), c_(c) {}
  // *(byte*)(G + 0x25930): the probe the rows carry.
  bool RouteFlag() const override { return Le<uint8_t>(c_.probe.at("flag")) != 0; }

 private:
  const Case& c_;
};

}  // namespace

TEST(IdleTurnRows, EnterIdleTurnWritesPesRequestWordsOnItsRows) {
  const char* rows = std::getenv("GF_PES_IDLETURN_ORACLE");
  if (!rows) GTEST_SKIP() << "GF_PES_IDLETURN_ORACLE unset";
  Install in;
  std::string skip;
  if (!LoadInstall(in, &skip)) GTEST_SKIP() << skip;
  const pes::PesClipReaders readers(*in.mbinfo);
  in.extra.ticks = [&readers](pes::ClipReader reader, int motion, float rate, int index) {
    return readers.Read(reader, motion, rate, index, 54.0f);
  };
  const std::vector<Case> cases = LoadCases(rows);
  ASSERT_FALSE(cases.empty());
  int real = 0, preStep = 0, replayRow = 0;
  for (const Case& c : cases) {
    SCOPED_TRACE(c.label + " " + c.mode);
    IdleTurnHost host(c, in);
    host.extra = &in.extra;
    pes::MoveReq req;
    for (size_t i = 0; i < c.reqIn.size(); ++i) req.SetU(static_cast<int>(i), c.reqIn[i]);
    pes::movestep::EnterIdleTurn(host, req);
    for (size_t i = 0; i < c.reqOut.size(); ++i)
      EXPECT_EQ(req.U(static_cast<int>(i)), c.reqOut[i]) << "request word " << std::hex << i;
    if (c.mode == "real") ++real;
    if (host.First(0x140a6cf70) != nullptr) ++preStep;
    if (!host.All(0x1406abb30).empty() && Le<int32_t>(host.All(0x1406abb30)[0]->in[1]) == 0x54c) ++replayRow;
  }
  RecordProperty("cases", static_cast<int>(cases.size()));
  RecordProperty("real", real);
  RecordProperty("preStep", preStep);
  RecordProperty("replayRow", replayRow);
}
