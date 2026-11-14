// FUN_1406d01d0 (pes::EnterGotoTurn) against PES's own bytes. GF_PES_STEP_ORACLE names rows made by
// tools/dev/translate/handler_oracle.py from the real calls of the capture hook stepaudit_1406D01D0 (and perturbed
// variants of their request words, labelled SYNTHETIC): per case the request words at the entry and at the return, the
// return value, and the handler's own calls (target, arguments, results) in execution order. The port runs on a host that
// answers every read from those rows and stops on any read the rows do not contain; it must write PES's request words,
// return PES's value, and draw the same random numbers. Without the rows (PES-derived, never committed) the cases skip.
// PES21_ROOT names the install for the constants.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "movestep_unbound_host.hpp"
#include "pes/exe.hpp"
#include "pes/mbinfo.hpp"
#include "pes/movestep.hpp"
#include "pes/movestep_run.hpp"
#include "pes/movestep_turn_entry.hpp"

namespace {

using Bytes = std::vector<uint8_t>;

// The targets of the calls the handler makes (PES VAs), by the PES function each answers.
constexpr uint64_t kMotionPtr = 0x1406e6bd0;    // +0xab8 vt+0x18: the current motion's pointer
constexpr uint64_t kAnimeSlot = 0x1406ea810;    // +0xab8 vt+0x08: the own slot
constexpr uint64_t kStatePtr = 0x1406eade0;     // +0xab8 vt+0x20: the state byte
constexpr uint64_t kAnimeState = 0x1406e66b0;   // +0xab8 vt+0x28
constexpr uint64_t kClipFrames = 0x1406e6960;   // +0xab8 vt+0x38
constexpr uint64_t kHitSeconds = 0x1406e7920;   // +0xab8 vt+0x50
constexpr uint64_t kHitDistance = 0x1406e7730;  // +0xab8 vt+0x58
constexpr uint64_t kAbilityWeight = 0x1406e9f90;  // +0xac0 vt+0x30
constexpr uint64_t kMatchUp = 0x140a62960;
constexpr uint64_t kCarrier = 0x140a627e0;
constexpr uint64_t kBallSample = 0x140a6cf70;
constexpr uint64_t kHeading = 0x140439e50;
constexpr uint64_t kManRecord = 0x140a48a10;
constexpr uint64_t kAnimeRecord = 0x140a48910;
constexpr uint64_t kStamina = 0x140a58550;
constexpr uint64_t kRecordTicks = 0x14044e020;
constexpr uint64_t kPercent = 0x1406ebf30;
constexpr uint64_t kCellMotion = 0x1406abb30;
constexpr uint64_t kCellMirror = 0x1406abd60;
constexpr uint64_t kAnalyser = 0x1406cba60;
constexpr uint64_t kIssue = 0x1406d8620;

struct Call {
  uint64_t site = 0, target = 0, rax = 0, xmm0 = 0;
  uint64_t reg[4] = {};
  uint64_t xmm[4] = {};
  uint64_t stack0 = 0;
  Bytes in[4], after[4], retmem, extra;
};

struct Case {
  std::string label, mode;
  std::vector<uint32_t> reqIn, reqOut;
  uint64_t rax = 0;
  Bytes ctx;
  std::map<std::string, Bytes> probe;
  std::vector<Call> calls;
};

Bytes FromHex(const std::string& s) {
  Bytes out;
  if (s == "-") return out;
  for (size_t i = 0; i + 1 < s.size(); i += 2) out.push_back(static_cast<uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
  return out;
}

std::vector<Case> LoadCases(const std::string& path) {
  std::vector<Case> cases;
  std::ifstream f(path);
  std::string line;
  Case cur;
  while (std::getline(f, line)) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag == "case") {
      cur = Case{};
      in >> cur.label >> cur.mode;
    } else if (tag == "req_in" || tag == "req_out") {
      size_t n;
      in >> n;
      std::vector<uint32_t>& v = tag == "req_in" ? cur.reqIn : cur.reqOut;
      v.resize(n);
      for (uint32_t& w : v) in >> w;
    } else if (tag == "rax") {
      in >> cur.rax;
    } else if (tag == "ctx") {
      std::string h;
      in >> h;
      cur.ctx = FromHex(h);
    } else if (tag == "probe") {
      std::string name, h;
      in >> name >> h;
      cur.probe[name] = FromHex(h);
    } else if (tag == "call") {
      Call c;
      std::string a, b;
      in >> std::hex >> c.site >> c.target >> c.rax >> c.xmm0;
      for (uint64_t& r : c.reg) in >> r;
      for (uint64_t& x : c.xmm) in >> x;
      in >> c.stack0 >> std::dec;
      std::string h;
      for (Bytes& m : c.in) { in >> h; m = FromHex(h); }
      for (Bytes& m : c.after) { in >> h; m = FromHex(h); }
      in >> h;
      c.retmem = FromHex(h);
      in >> h;
      c.extra = FromHex(h);
      cur.calls.push_back(std::move(c));
    } else if (tag == "end") {
      cases.push_back(cur);
    }
  }
  return cases;
}

template <typename T>
T Le(const Bytes& b, size_t at = 0) {
  T v{};
  if (b.size() >= at + sizeof v) std::memcpy(&v, b.data() + at, sizeof v);
  return v;
}
float F32(uint64_t bits) {
  const uint32_t u = static_cast<uint32_t>(bits);
  float f;
  std::memcpy(&f, &u, sizeof f);
  return f;
}
uint32_t Bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, sizeof u);
  return u;
}

// The host PES's call rows define. Reads are answered by target; the effectful calls (the random draws, the analyser
// counter) are consumed in order and checked against their arguments.
class RowHost : public movestep_test::UnboundHost {
 public:
  RowHost(const Case& c, const pes::RunConstants& run, const pes::RunTables& tables, const pes::PlaybackConstants& playback,
          const pes::AngleBucketConstants& buckets, const pes::MoveConstants& move, const pes::TurnEntryTables& turn)
      : c_(c), run_(run), tables_(tables), playback_(playback), buckets_(buckets), move_(move), turn_(turn), team_(*this) {}

  int percentDrawn = 0, analystCalls = 0, percentNext = 0;
  mutable int headingNext = 0, ballNext = 0;
  bool analyserExpected = false;

  const Call* First(uint64_t target) const {
    for (const Call& call : c_.calls)
      if (call.target == target) return &call;
    return nullptr;
  }
  const Call& Need(uint64_t target) const {
    const Call* call = First(target);
    EXPECT_NE(call, nullptr) << "the port reads a call PES did not make: " << std::hex << target;
    if (call == nullptr) std::abort();
    return *call;
  }
  std::vector<const Call*> All(uint64_t target) const {
    std::vector<const Call*> out;
    for (const Call& call : c_.calls)
      if (call.target == target) out.push_back(&call);
    return out;
  }
  const Call& Nth(uint64_t target, int n) const {
    const auto all = All(target);
    EXPECT_LT(static_cast<size_t>(n), all.size()) << "the port makes more calls than PES did: " << std::hex << target;
    if (static_cast<size_t>(n) >= all.size()) std::abort();
    return *all[n];
  }

  int CurrentMotion() const override { return Le<uint16_t>(Need(kMotionPtr).retmem); }
  int ActionIndex() const override { return static_cast<int32_t>(Need(kAnimeSlot).rax); }
  float PhysiqueG() const override { return F32(Le<uint32_t>(c_.probe.at("g"))); }
  float TicksPerSecond() const override { return 54.0f; }
  int AnimeState() const override { return static_cast<int32_t>(Need(kAnimeState).rax); }
  int CurrentState() const override { return Le<uint8_t>(Need(kStatePtr).retmem); }
  int CurrentClipFrames() const override { return static_cast<int32_t>(Need(kClipFrames).rax); }
  int AbilityByte(int) const override {
    // The weight PES computed (FUN_1406e9f90) with the byte solved for it: the formula is checked bit for bit by the test.
    const float p = F32(Need(kAbilityWeight).xmm0);
    return static_cast<int>(std::lround(p * 59.0f + 40.0f));
  }
  void OwnPosition(float out[3]) const override {
    out[0] = F32(Le<uint32_t>(c_.ctx, 8));
    out[1] = F32(Le<uint32_t>(c_.ctx, 0xc));
    out[2] = F32(Le<uint32_t>(c_.ctx, 0x10));
  }
  float FacingDeg() const override { return F32(Le<uint32_t>(c_.ctx, 0x20)); }
  int MatchPhase() const override { return Le<int32_t>(c_.probe.at("phase")); }
  bool MatchObjectFlag2c0() const override {
    const uint8_t type = Le<uint8_t>(c_.probe.at("m2c0type"));
    return static_cast<uint8_t>(type - 1) < 2 && Le<uint8_t>(c_.probe.at("m2c0flag")) != 0;
  }
  const pes::RunConstants& RunConstantsView() const override { return run_; }
  const pes::RunTables& RunTablesView() const override { return tables_; }
  const pes::PlaybackConstants& PlaybackConstantsView() const override { return playback_; }
  const pes::AngleBucketConstants& AngleBuckets() const override { return buckets_; }
  const pes::MoveConstants& MoveConstantsView() const override { return move_; }
  const pes::TurnEntryTables& TurnEntryTablesView() const override { return turn_; }
  const pes::TeamAi& Team() const override { return team_; }

  int CarrierSlot() const override { return static_cast<int32_t>(Need(kCarrier).rax); }
  int StaminaPercent() const override { return static_cast<uint8_t>(Need(kStamina).rax); }
  int AnimeRecordClass(int slot) const override {
    for (const Call* call : All(kAnimeRecord))
      if (static_cast<int32_t>(call->reg[1]) == slot) return Le<uint8_t>(call->retmem, 1);
    ADD_FAILURE() << "no anime record call for slot " << slot;
    std::abort();
  }
  int RecordTicks(int slot) const override {
    for (const Call* call : All(kRecordTicks))
      if (static_cast<int32_t>(call->reg[1]) == slot) return static_cast<int32_t>(call->rax);
    ADD_FAILURE() << "no record ticks call for slot " << slot;
    std::abort();
  }
  void RecordPosition(int slot, float out[3]) const override {
    for (const Call* call : All(kManRecord))
      if (static_cast<int32_t>(call->reg[1]) == slot) {
        for (int i = 0; i < 3; ++i) out[i] = F32(Le<uint32_t>(call->extra, 4 * i));
        return;
      }
    ADD_FAILURE() << "no man record call for slot " << slot;
    std::abort();
  }
  void BallSample(int index, float out[3]) const override {
    const Call& call = Nth(kBallSample, ballNext++);
    EXPECT_EQ(index, static_cast<int32_t>(call.reg[1]));
    for (int i = 0; i < 3; ++i) out[i] = F32(Le<uint32_t>(call.retmem, 4 * i));
  }
  float HeadingToPointDeg(const float point[3]) const override {
    const Call& call = Nth(kHeading, headingNext++);
    for (int i = 0; i < 3; ++i) EXPECT_EQ(Bits(point[i]), Le<uint32_t>(call.in[2], 4 * i)) << "point " << i;
    return F32(Le<uint32_t>(call.retmem));
  }
  bool Percent(int pct) override {
    const Call& call = Nth(kPercent, percentNext++);
    EXPECT_EQ(pct, static_cast<int32_t>(call.reg[1]));
    ++percentDrawn;
    return (call.rax & 0xff) != 0;
  }
  pes::MoveCell Cell(int row, int column, bool walk) override {
    EXPECT_FALSE(walk);
    pes::MoveCell cell;
    cell.found = true;
    bool motion = false, mirror = false;
    for (const Call* call : All(kCellMotion))
      if (Le<int32_t>(call->in[1]) == row && Le<int32_t>(call->in[2]) == column) {
        cell.motionNo = static_cast<int16_t>(call->rax);
        motion = true;
        break;
      }
    for (const Call* call : All(kCellMirror))
      if (Le<int32_t>(call->in[1]) == row && Le<int32_t>(call->in[2]) == column) {
        cell.mirror = (call->rax & 0xff) != 0;
        mirror = true;
        break;
      }
    EXPECT_TRUE(motion && mirror) << "no cell call for row " << std::hex << row << " column " << column;
    return cell;
  }
  float ClipHitDistance(int motion, float g, int index) const override {
    const Call& call = Need(kHitDistance);
    EXPECT_EQ(motion, static_cast<int32_t>(call.reg[1]));
    EXPECT_EQ(Bits(g), static_cast<uint32_t>(call.xmm[2]));
    EXPECT_EQ(index, static_cast<int32_t>(call.reg[3]));
    return F32(call.xmm0);
  }
  // FUN_1406e7920's blend ticks, solved for the seconds PES returned: ClipHitSeconds divides by the clock and the playback,
  // so the solved integer reproduces PES's value exactly when the formula is right.
  int ClipTicks(pes::ClipReader, int motion, float rate, int) const override {
    const Call& call = Need(kHitSeconds);
    EXPECT_EQ(motion, static_cast<int32_t>(call.reg[1]));
    const float playback = F32(call.xmm[2]);
    return static_cast<int>(std::lround(F32(call.xmm0) * playback * 54.0f));
  }
  const pes::MotionRecord& Motion(int) const override { return empty_; }
  void CountAnalyserEvent(bool flag, int motion) override {
    const Call& call = Need(kAnalyser);
    EXPECT_EQ(flag, (call.reg[1] & 0xff) != 0);
    EXPECT_EQ(motion, static_cast<uint16_t>(call.reg[2]));
    ++analystCalls;
  }

  // The match-up read: FUN_140a62960's answer is a slot; the port asks whether it is the own one.
  struct RowTeam : pes::TeamAi {
    explicit RowTeam(const RowHost& h) : host(h) {}
    const RowHost& host;
    int NearestOpponent(int) const override { std::abort(); }
    int Ability(int, int) const override { std::abort(); }
    int Condition(int) const override { return 100; }  // above 10: no penalty, the byte is solved without one
    void Position(int, float[3]) const override { std::abort(); }
    void Velocity(int, float[3]) const override { std::abort(); }
    float Heading(int) const override { std::abort(); }
    int SubState(int) const override { std::abort(); }
    bool ActionBit27(int) const override { std::abort(); }
    bool IsSideKeeper(int) const override { std::abort(); }
    bool SideTackled(int) const override { std::abort(); }
    bool PossessionLatched(int) const override { std::abort(); }
    bool Carries(int) const override { std::abort(); }
    bool IsSideSlot78(int) const override { std::abort(); }
    bool HasRouteRow(int) const override { std::abort(); }
    bool IsMatchUpMan(int slot) const override { return static_cast<int32_t>(host.Need(kMatchUp).rax) == slot; }
    bool IsQueuedCover(int) const override { std::abort(); }
    bool SideSlotMarked(int) const override { std::abort(); }
    bool SetPieceAnswer(int) const override { std::abort(); }
    int BallOwnerSlot() const override { std::abort(); }
    bool MaFlag25930() const override { std::abort(); }
  };

 private:
  const Case& c_;
  const pes::RunConstants& run_;
  const pes::RunTables& tables_;
  const pes::PlaybackConstants& playback_;
  const pes::AngleBucketConstants& buckets_;
  const pes::MoveConstants& move_;
  const pes::TurnEntryTables& turn_;
  RowTeam team_;
  pes::MotionRecord empty_;
};

struct Install {
  pes::RunConstants run;
  pes::RunTables tables;
  pes::PlaybackConstants playback;
  pes::AngleBucketConstants buckets;
  pes::MoveConstants move;
  pes::TurnEntryTables turn;
};

bool LoadInstall(Install& in, std::string* skip) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) { *skip = "PES21_ROOT unset"; return false; }
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  if (!exe) { *skip = err; return false; }
  auto run = pes::LoadRunConstants(*exe, &err);
  auto tables = pes::LoadRunTables(*exe, &err);
  auto playback = pes::LoadPlaybackConstants(*exe, &err);
  auto buckets = pes::LoadAngleBucketConstants(*exe, &err);
  auto move = pes::LoadMoveConstants(*exe, &err);
  auto turn = pes::LoadTurnEntryTables(*exe, &err);
  if (!run || !tables || !playback || !buckets || !move || !turn) { *skip = err; return false; }
  in = {*run, *tables, *playback, *buckets, *move, *turn};
  return true;
}

class TurnEntryRows : public ::testing::Test {};

}  // namespace

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
    RowHost host(c, in.run, in.tables, in.playback, in.buckets, in.move, in.turn);
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
