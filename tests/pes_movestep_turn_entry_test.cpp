// FUN_1406d01d0 (pes::EnterGotoTurn) against PES's own bytes. GF_PES_STEP_ORACLE names rows made by
// tools/dev/translate/handler_oracle.py from the real calls of the capture hook stepaudit_1406D01D0 (and perturbed
// variants of their request words, labelled SYNTHETIC): per case the request words at the entry and at the return, the
// return value, and the handler's own calls (target, arguments, results) in execution order. The port runs on a host that
// answers every read from those rows and stops on any read the rows do not contain; it must write PES's request words,
// return PES's value, and draw the same random numbers. Without the rows (PES-derived, never committed) the cases skip.
// PES21_ROOT names the install for the constants.
#include "movestep_row_host.hpp"

class TurnEntryRows : public ::testing::Test {};

TEST(TurnEntryRows, TheTurnEntryWritesPesRequestWordsAndAnswersLikePesOnItsRows) {
  const char* rows = std::getenv("GF_PES_STEP_ORACLE");
  if (!rows) GTEST_SKIP() << "GF_PES_STEP_ORACLE unset";
  Install in;
  std::string skip;
  if (!LoadInstall(in, &skip)) GTEST_SKIP() << skip;
  const std::vector<Case> cases = LoadCases(rows);
  ASSERT_FALSE(cases.empty());
  int real = 0, taken = 0;
  for (const Case& c : cases) {
    SCOPED_TRACE(c.label + " " + c.mode);
    RowHost host(c, in.run, in.tables, in.playback, in.buckets, in.move, in.turn, *in.mbinfo);
    pes::MoveReq req;
    for (size_t i = 0; i < c.reqIn.size(); ++i) req.SetU(static_cast<int>(i), c.reqIn[i]);
    const bool entered = pes::EnterGotoTurn(host, req);
    EXPECT_EQ(entered, (c.rax & 0xff) != 0);
    for (size_t i = 0; i < c.reqOut.size(); ++i)
      EXPECT_EQ(req.U(static_cast<int>(i)), c.reqOut[i]) << "request word " << std::hex << i;
    // Every effectful call PES made, the port made.
    EXPECT_EQ(static_cast<size_t>(host.percentDrawn), host.All(kPercent).size());
    EXPECT_EQ(static_cast<size_t>(host.analystCalls), host.All(kAnalyser).size());
    if (c.mode == "real") ++real;
    if (entered) ++taken;
  }
  RecordProperty("cases", static_cast<int>(cases.size()));
  RecordProperty("real", real);
  RecordProperty("entered", taken);
}
