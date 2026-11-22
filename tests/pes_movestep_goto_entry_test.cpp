// FUN_1406ce820 (pes::movestep::EnterGotoRun) against PES's own bytes. GF_PES_GOTO_ORACLE names rows made by
// tools/dev/translate/handler_oracle.py from the real calls of the capture hook mv_1406CE820 (and perturbed variants of their
// request words and of the memory the handler reads directly, labelled SYNTHETIC; the callees still run PES's bytes, so every
// answer is PES's own for that input): per case the request words at the entry and at the return, the entry registers, the memory
// probes and the handler's own calls in execution order. The port runs on a host that answers every read from those rows and
// stops on any read the rows do not contain; it must write PES's request words, make the same random draws, count the same
// analyser event. The clip's hit seconds and distance are computed by the port from the install's motion table and PES's
// translated clip readers (the rows do not list the issue's inner calls) and checked through the request words the issue wrote.
// Without the rows (PES-derived, never committed) the cases skip. PES21_ROOT names the install.
#include "movestep_row_host.hpp"
#include "onthepitch/pesclipreaders.hpp"

TEST(GotoEntryRows, EnterGotoRunWritesPesRequestWordsAndDrawsLikePesOnItsRows) {
  const char* rows = std::getenv("GF_PES_GOTO_ORACLE");
  if (!rows) GTEST_SKIP() << "GF_PES_GOTO_ORACLE unset";
  Install in;
  std::string skip;
  if (!LoadInstall(in, &skip)) GTEST_SKIP() << skip;
  const pes::PesClipReaders readers(*in.mbinfo);
  in.extra.ticks = [&readers](pes::ClipReader reader, int motion, float rate, int index) {
    return readers.Read(reader, motion, rate, index, 54.0f);
  };
  const std::vector<Case> cases = LoadCases(rows);
  ASSERT_FALSE(cases.empty());
  int real = 0, precise = 0;
  for (const Case& c : cases) {
    SCOPED_TRACE(c.label + " " + c.mode);
    RowHost host(c, in.run, in.tables, in.playback, in.buckets, in.move, in.turn, *in.mbinfo);
    host.extra = &in.extra;
    pes::MoveReq req;
    for (size_t i = 0; i < c.reqIn.size(); ++i) req.SetU(static_cast<int>(i), c.reqIn[i]);
    pes::movestep::EnterGotoRun(host, req, (c.regs[2] & 0xff) != 0);
    for (size_t i = 0; i < c.reqOut.size(); ++i)
      EXPECT_EQ(req.U(static_cast<int>(i)), c.reqOut[i]) << "request word " << std::hex << i;
    EXPECT_EQ(static_cast<size_t>(host.percentDrawn), host.All(kPercent).size()) << "random draws";
    EXPECT_EQ(static_cast<size_t>(host.analystCalls), host.All(kAnalyser).size()) << "analyser events";
    if (c.mode == "real") ++real;
    if (c.reqIn.size() > 0xe && c.reqIn[0xe] != 0) ++precise;
  }
  RecordProperty("cases", static_cast<int>(cases.size()));
  RecordProperty("real", real);
  RecordProperty("precise", precise);
}
