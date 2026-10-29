#include "pes/fixdemo_select.hpp"

#include <algorithm>
#include <cstring>

#include "pes/exe.hpp"
#include "pes_native.hpp"
#include "translated.hpp"

namespace pes {
namespace {

// Where GF's memory for the translated code sits: host stack and scratch above PES's image and below the heap arena.
constexpr uint64_t kStackVa = 0x10000000, kStackBytes = 0x20000;
constexpr uint64_t kScratchVa = 0x20400000, kScratchBytes = 0x1000;
constexpr uint64_t kOutRowsVa = kScratchVa, kOutCountVa = kScratchVa + 8;
constexpr uint64_t kShadowBytes = 0x20;  // Win64 home area: stack argument 5 sits at entry rsp + 0x28 (return address pushed)

// The PES statics the selection reads, as one window: ctx slot, category table, the two Random1000 objects, the pick
// histories (0x1437F5DC0 + cat*0x40 and the ctrl histories at 0x1437F6080 + cat*0x40). [0x1437F5000, 0x1437F6400).
constexpr uint64_t kStaticsVa = 0x1437F5000, kStaticsBytes = 0x1400;
constexpr uint64_t kCtrlHistoryVa = 0x1437F6080;
constexpr unsigned kCategories = 10;  // categories 0..9 have a history slot each (ent..mode)
// The environment globals the ctx reset (0x141F50B90) and condition 0x03 read: G1s u16 @0x14351AE30, G1 i32 @0x14351AE34.
constexpr uint64_t kEnvGlobalsVa = 0x14351AE00, kEnvGlobalsBytes = 0x100;
constexpr uint64_t kG1sVa = 0x14351AE30, kG1Va = 0x14351AE34;
constexpr uint64_t kDbPageVa = 0x1436F4000, kDbPageBytes = 0x1000;

// ctx layout (docs/re/pes-re-cutscenes.md Appendix A, ctx field map).
constexpr uint64_t kCtxBytes = 0x52E0;  // FixDemoEnv: allocated by 0x141F500D0 with this size
constexpr uint64_t kCtxStadium = 0x50, kCtxTimeOfDay = 0x54, kCtxWeather = 0x58, kCtxEnvCompare = 0x64, kCtxEnvFlag = 0x70;
constexpr uint64_t kSideBase = 0xF8, kSideStride = 0x2830;  // S(s) = ctx + 0xF8 + s * 0x2830
constexpr uint64_t kSidePlayersOnPitch = 0x2818, kSideTeamByte = 0x2819;  // S(s)+0x2818 / +0x2819 (ctx+0x2910.. / 0x2911..)
constexpr uint64_t kCtxGoalDiff = 0x5160, kCtxCompetitionRaw = 0x5170, kCtxParentCompetition = 0x5174, kCtxTieContext = 0x5178,
                   kCtxStage = 0x5179, kCtxFixtureWord = 0x5184, kCtxFixtureInt = 0x5188, kCtxPhase = 0x51F8,
                   kCtxSubject = 0x5204, kCtxEventX = 0x5208, kCtxEventY = 0x520C, kCtxEventZ = 0x5210, kCtxEventSide = 0x5214,
                   kCtxEventSlot = 0x5218, kCtxMode = 0x5228;
constexpr uint32_t kModeMatch = 2;  // 0x141F514D0: ctx+0x5228 = 2 selects the match RNG

// The DB: per category {start,count} u16 pairs at DB+cat*0x20+0x168, count at +0x178, the 0x78-byte rows at +0x180.
constexpr uint64_t kDbBytes = 0x460;  // 0x140A7CC00 allocates the singleton with this size
constexpr uint64_t kDbCategoryStride = 0x20, kDbGroupsOff = 0x168, kDbGroupCountOff = 0x178, kDbRowsOff = 0x180;
constexpr uint64_t kRowBytes = 0x78;  // key u16[10] at +0, then the fdc strings and b0/f/b1 (not read by the matcher)
constexpr unsigned kKeyColumns = 10;

// ponytail: the setter 0x141F51630 reads these from the match setup and the environment record (world+0x35960+0x70), none
// of which GF has. The values are what PES's own exhibition match holds at an event (live captures pid1792161 seq16360 and
// pid1981120 seq8091/9831/9843, ctx read by script): competition 0x20 / raw 0x22 / stage 0 (0x141F51630 via 0x141E554B0),
// no tie context, environment +0x14 = 9 (never 0x21) and +0x20 = 1, no fixture word (0xFFFF / 0), the team byte S+0x2819
// = 23 (teamObj+0x2EEC). The sites that remove them: the setup fields at G+0x2AD0 and the environment owner of FUN_14124DAD0.
constexpr uint32_t kStandInParentCompetition = 0x20, kStandInCompetitionRaw = 0x22, kStandInStage = 0, kStandInTieContext = 0;
constexpr uint32_t kStandInEnvCompare = 9, kStandInEnvFlag = 1;
constexpr uint16_t kStandInFixtureWord = 0xFFFF;
constexpr uint32_t kStandInFixtureInt = 0;
constexpr uint8_t kStandInTeamByte = 23;
// G1s/G1 at their image values (0xFFFF / 0): condition 0x03 then answers only 0x300 (docs Appendix A, 0x03).
constexpr uint32_t kStandInG1 = 0;

// The switch tables the translated evaluator and 0x141F50450 read out of PES's code: byte index tables at 0x141F4F9E0,
// 0x141F5092C, 0x141F50A0C and dword offset tables at 0x141F4F948, 0x141F4FAAC, 0x141F4FC14, 0x141F508B0, 0x141F509DC (the
// `s.rd(0x140000000 + ...)` sequences of pes_fn_141F4D2F0 and pes_fn_141F50450). One run covers them; read from the install.
constexpr uint64_t kImageTablesVa = 0x141F4F900, kImageTablesBytes = 0x1200;

// The goal anchor's world (docs/ports/pes-port-CutAnchor.md §2; the reads of the 7 captured calls, byte by byte): the world
// object G behind [0x1436F3DC8] with the match info MI (G+0x270), the field info (G+0x2AA8), and the man record (G+0x1DD8) and
// run object (G+0x21E8) of each pitch slot, 0x28-byte table entries whose first qword is the record's address.
constexpr uint64_t kWorldSlotVa = 0x1436F3DC8, kWorldSlotPageVa = 0x1436F3000, kWorldSlotPageBytes = 0x1000;
constexpr uint64_t kFpsVa = 0x14351AE54;        // inside the environment globals segment
constexpr uint64_t kFullTurnVa = 0x14259858C;   // 360.0f: the writer 0x14047D280's wrap constant (read from the install)
constexpr uint64_t kAnchorWorldBytes = 0x2E58, kMatchInfoBytes = 0x1600, kManBytes = 0x700, kRunBytes = 0x40, kA0Bytes = 0x40;
constexpr uint64_t kWorldMatchInfo = 0x270, kWorldFieldInfo = 0x2AA8, kWorldManTable = 0x1DD8, kWorldRunTable = 0x21E8;
constexpr uint64_t kWorldTableStride = 0x28;
constexpr unsigned kPitchSlots = 22;  // 2 sides * 11
constexpr uint64_t kMiSlotMode = 0x1588, kMiSlot = 0x1590;  // MI+0x1588 == 1: the scorer's slot is MI+0x1590 (0x140474F00)
constexpr uint64_t kManFacing = 0x554, kManPos = 0x55C, kManVel = 0x574, kManMoveMode = 0x5B0, kRunTarget = 0x18;
constexpr uint64_t kA0SceneId = 8, kA0Radius = 0xC;
constexpr uint64_t kAnchorOutVa = kScratchVa + 0x100, kAnchorOutRotY = 0x10;  // pos (3 f32) then rotY, inside the scratch segment
constexpr uint64_t kDbSceneRows = 0x448, kDbSceneRowsEnd = 0x450;  // a std::vector's begin/end: the goal scene rows (0x140A7CBA0)
constexpr uint64_t kSceneRowBytes = 0x3C, kSceneRowFloats = 8, kSceneRowTail = 0x38;

}  // namespace

struct FixDemoSelector::Impl {
  std::map<int, FixDemoTable> tables;
  std::unique_ptr<PesHeap> heap = std::make_unique<PesHeap>();
  PesState s;
  uint64_t ctx = 0, db = 0;
  uint64_t world = 0, matchInfo = 0, fieldInfo = 0, a0 = 0;
  std::array<uint64_t, kPitchSlots> man{}, run{};
  float fullTurnDeg = 0;
  std::map<uint32_t, uint32_t> cache;  // cut id -> row

  Impl(std::map<int, FixDemoTable> t, std::vector<uint8_t> imageTables, float fullTurn) : tables(std::move(t)), fullTurnDeg(fullTurn) {
    heap->Attach(s);
    s.mem.img.push_back({kImageTablesVa, std::move(imageTables)});
    for (auto [va, bytes] : {std::pair<uint64_t, uint64_t>{kStackVa, kStackBytes}, {kScratchVa, kScratchBytes},
                             {kStaticsVa, kStaticsBytes}, {kEnvGlobalsVa, kEnvGlobalsBytes}, {kDbPageVa, kDbPageBytes},
                             {kWorldSlotPageVa, kWorldSlotPageBytes}})
      s.mem.segs.push_back({va, std::vector<uint8_t>(bytes)});
    s.gpr[4] = kStackVa + kStackBytes - 0x100;
    ctx = heap->Alloc(kCtxBytes, 0x10);
    s.wr(kFixDemoCtxSlotVa, ctx, 8);
    BuildDb();
    // Environment globals at their image values.
    s.wr(kG1sVa, kStandInFixtureWord, 2);
    s.wr(kG1Va, kStandInG1, 4);
    BuildWorld();
    BuildSceneRows();
    // Empty pick histories: PES's static initialisers leave each std::map as a sentinel node (left = parent = right = self,
    // colour black, isnil) and empty vectors; the history objects are 0x40 bytes with the map's head pointer at +0.
    for (unsigned c = 0; c < kCategories; ++c) {
      InitHistory(kFixDemoHistoryVa + c * 0x40);
      InitHistory(kCtrlHistoryVa + c * 0x40);
    }
  }

  void InitHistory(uint64_t va) {
    const uint64_t head = heap->Alloc(0x30, 0x10);
    s.wr(head, head, 8);
    s.wr(head + 8, head, 8);
    s.wr(head + 0x10, head, 8);
    s.wr(head + 0x18, 1, 1);  // color: black
    s.wr(head + 0x19, 1, 1);  // isnil
    s.wr(va, head, 8);
  }

  void BuildDb() {
    db = heap->Alloc(kDbBytes, 0x10);
    s.wr(kFixDemoDbSlotVa, db, 8);
    for (const auto& [cat, table] : tables) {
      const uint64_t base = db + static_cast<uint64_t>(cat) * kDbCategoryStride;
      uint32_t rowCount = 0;
      for (const FixDemoCutGroup& g : table.cutGroups)
        rowCount = std::max<uint32_t>(rowCount, g.firstRow + static_cast<uint32_t>(g.rows.size()));
      const uint64_t groups = heap->Alloc(std::max<size_t>(table.cutGroups.size(), 1) * 4, 0x10);
      const uint64_t rows = heap->Alloc(std::max<uint32_t>(rowCount, 1) * kRowBytes, 0x10);
      for (size_t gi = 0; gi < table.cutGroups.size(); ++gi) {
        const FixDemoCutGroup& g = table.cutGroups[gi];
        s.wr(groups + gi * 4, g.firstRow, 2);
        s.wr(groups + gi * 4 + 2, g.rows.size(), 2);
        for (size_t ri = 0; ri < g.rows.size(); ++ri)
          for (size_t k = 0; k < kKeyColumns && k < g.rows[ri].keys.size(); ++k)
            s.wr(rows + (g.firstRow + ri) * kRowBytes + k * 2, g.rows[ri].keys[k], 2);
      }
      s.wr(base + kDbGroupsOff, groups, 8);
      s.wr(base + kDbGroupCountOff, table.cutGroups.size(), 4);
      s.wr(base + kDbRowsOff, rows, 8);
    }
  }

  uint64_t Zalloc(uint64_t bytes) {
    const uint64_t va = heap->Alloc(bytes, 0x10);
    for (uint64_t off = 0; off < bytes; off += 8) s.wr(va + off, 0, 8);
    return va;
  }

  void BuildWorld() {
    world = Zalloc(kAnchorWorldBytes);
    s.wr(kWorldSlotVa, world, 8);
    matchInfo = Zalloc(kMatchInfoBytes);
    fieldInfo = Zalloc(8);
    a0 = Zalloc(kA0Bytes);
    s.wr(world + kWorldMatchInfo, matchInfo, 8);
    s.wr(world + kWorldFieldInfo, fieldInfo, 8);
    for (unsigned slot = 0; slot < kPitchSlots; ++slot) {
      man[slot] = Zalloc(kManBytes);
      run[slot] = Zalloc(kRunBytes);
      s.wr(world + kWorldManTable + slot * kWorldTableStride, man[slot], 8);
      s.wr(world + kWorldRunTable + slot * kWorldTableStride, run[slot], 8);
    }
  }

  // The goal scene rows (table stream order, laid out as PES's DB holds them): u8 x5 at +0, f32 x12 at +8, u8 x4 at +0x38.
  void BuildSceneRows() {
    const auto goal = tables.find(kFixDemoGoal);
    if (goal == tables.end() || goal->second.goalSceneRows.empty()) return;
    const auto& rows = goal->second.goalSceneRows;
    const uint64_t base = Zalloc(rows.size() * kSceneRowBytes);
    for (size_t i = 0; i < rows.size(); ++i) {
      const uint64_t r = base + i * kSceneRowBytes;
      for (size_t k = 0; k < rows[i].head.size(); ++k) s.wr(r + k, rows[i].head[k], 1);
      for (size_t k = 0; k < rows[i].floats.size(); ++k) s.wrf(r + kSceneRowFloats + 4 * k, rows[i].floats[k]);
      for (size_t k = 0; k < rows[i].tail.size(); ++k) s.wr(r + kSceneRowTail + k, rows[i].tail[k], 1);
    }
    s.wr(db + kDbSceneRows, base, 8);
    s.wr(db + kDbSceneRowsEnd, base + rows.size() * kSceneRowBytes, 8);
  }

  std::optional<GoalAnchor> PlaceGoal(const GoalAnchorFacts& f) {
    const auto goal = tables.find(kFixDemoGoal);
    const uint32_t index = (f.sceneId & 0xFFFF) - 1;
    if (goal == tables.end() || (f.sceneId >> 16) != static_cast<uint32_t>(kFixDemoGoal) ||
        index >= goal->second.goalSceneRows.size() || f.scorerSlot >= kPitchSlots)
      return std::nullopt;
    s.wr(matchInfo + kMiSlotMode, 1, 4);
    s.wr(matchInfo + kMiSlot, f.scorerSlot, 4);
    s.wrf(fieldInfo, f.halfLength);
    s.wrf(fieldInfo + 4, f.halfWidth);
    s.wrf(kFpsVa, f.fps);
    const uint64_t m = man[f.scorerSlot], r = run[f.scorerSlot];
    s.wrf(m + kManFacing, f.facing);
    s.wr(m + kManMoveMode, f.moveMode, 4);
    for (unsigned i = 0; i < 3; ++i) {
      s.wrf(m + kManPos + 4 * i, f.pos[i]);
      s.wrf(m + kManVel + 4 * i, f.vel[i]);
      s.wrf(r + kRunTarget + 4 * i, f.target[i]);
    }
    s.wr(a0 + kA0SceneId, f.sceneId, 4);
    s.wrf(a0 + kA0Radius, f.radius);
    // 0x140474F00(A0, arg2 (not read), &pos, &rotY)
    s.gpr[1] = a0;
    s.gpr[2] = 0;
    s.gpr[8] = kAnchorOutVa;
    s.gpr[9] = kAnchorOutVa + kAnchorOutRotY;
    Call(&pes_fn_140474F00);
    GoalAnchor out;
    for (unsigned i = 0; i < 3; ++i) out.pos[i] = s.rdf(kAnchorOutVa + 4 * i);
    float rot = s.rdf(kAnchorOutVa + kAnchorOutRotY);
    // The writer 0x14047D280 (kind-1 arm) wraps rotY once by [0x14259858C]: 0 <= r ? (r < t ? r : r - t) : r + t.
    if (0.0f <= rot) {
      if (rot >= fullTurnDeg) rot -= fullTurnDeg;
    } else {
      rot += fullTurnDeg;
    }
    out.rotYDeg = rot;
    // 0x140478FF0(A0, arg2 (not read)): the scene row's test of the scorer's spot.
    s.gpr[1] = a0;
    s.gpr[2] = 0;
    Call(&pes_fn_140478FF0);
    out.flip = (s.gpr[0] & 0xFF) != 0;
    return out;
  }

  // A translated function is entered as the dispatch enters it: a return address pushed.
  void Call(void (*fn)(PesState&)) {
    s.gpr[4] -= 8;
    s.wr(s.gpr[4], 0, 8);
    fn(s);
    s.gpr[4] += 8;
  }

  // 0x141F4FDA0.
  std::optional<uint32_t> Pick(uint32_t cutId) {
    if (const auto hit = cache.find(cutId); hit != cache.end()) return hit->second;
    const uint32_t category = (cutId >> 16) & 0xFF;
    const uint32_t group = (cutId & 0xFFFF) - 1;  // 0x141F4FDB0: movzx ebx, dx; dec ebx
    // 0x140A7CB40(db, &rows, &count, category, group): rows pointer, count, and the group's start in rax.
    s.gpr[1] = db;
    s.gpr[2] = kOutRowsVa;
    s.gpr[8] = kOutCountVa;
    s.gpr[9] = category;
    s.wr(s.gpr[4] + kShadowBytes, group, 4);
    Call(&pes_fn_140A7CB40);
    const uint64_t rows = s.rd(kOutRowsVa, 8);
    const uint32_t count = static_cast<uint32_t>(s.rd(kOutCountVa, 4));
    const uint32_t start = static_cast<uint16_t>(s.gpr[0]);
    if (rows == 0) return std::nullopt;
    // 0x141F4B1D0(ctx, category, rows, count, history = 0x1437F5DC0 + category * 0x40, start).
    s.gpr[1] = ctx;
    s.gpr[2] = category;
    s.gpr[8] = rows;
    s.gpr[9] = count;
    s.wr(s.gpr[4] + kShadowBytes, kFixDemoHistoryVa + static_cast<uint64_t>(category) * 0x40, 8);
    s.wr(s.gpr[4] + kShadowBytes + 8, start, 4);
    Call(&pes_fn_141F4B1D0);
    const uint32_t row = static_cast<uint32_t>(s.gpr[0]);
    // ponytail: PES caches the pick in a std::map at ctx+0x30 (0x141056DF0 node alloc, 0x141056F40 insert; neither is in the
    // committed layer). GF keeps the same cut-id -> row table beside the arena until the wrapper is translated.
    cache.emplace(cutId, row);
    return row;
  }
};

std::unique_ptr<FixDemoSelector> FixDemoSelector::Create(std::map<int, FixDemoTable> tables, const PeImage& exe,
                                                         std::string* error) {
  std::vector<uint8_t> image(kImageTablesBytes);
  if (!exe.ReadVa(kImageTablesVa, image.size(), image.data())) {
    if (error) *error = "the image does not map the FixDemo evaluator's switch tables";
    return nullptr;
  }
  uint8_t turn[sizeof(float)];
  float fullTurn;
  if (!exe.ReadVa(kFullTurnVa, sizeof turn, turn)) {
    if (error) *error = "the image does not map the scene writer's turn constant";
    return nullptr;
  }
  std::memcpy(&fullTurn, turn, sizeof fullTurn);
  return std::unique_ptr<FixDemoSelector>(
      new FixDemoSelector(std::make_unique<Impl>(std::move(tables), std::move(image), fullTurn)));
}

FixDemoSelector::FixDemoSelector(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
FixDemoSelector::~FixDemoSelector() = default;

PesState& FixDemoSelector::State() { return impl_->s; }
PesHeap& FixDemoSelector::Heap() { return *impl_->heap; }

void FixDemoSelector::Seed(uint32_t sessionSeed, uint32_t matchSeed) {
  impl_->s.gpr[1] = sessionSeed;
  impl_->Call(&pes_fn_141F51FD0);
  impl_->s.gpr[1] = matchSeed;
  impl_->Call(&pes_fn_141F52060);
}

void FixDemoSelector::BeginEvent(const FixDemoFacts& f) {
  Impl& m = *impl_;
  PesState& s = m.s;
  m.cache.clear();
  for (uint64_t off = 0; off < kCtxBytes; off += 8) s.wr(m.ctx + off, 0, 8);  // 0x141F50B90 resets; unread fields stay 0
  const uint64_t c = m.ctx;
  s.wr(c + kCtxStadium, f.stadium, 4);
  s.wr(c + kCtxTimeOfDay, f.timeOfDay, 4);
  s.wr(c + kCtxWeather, f.weather, 4);
  s.wr(c + kCtxEnvCompare, kStandInEnvCompare, 4);
  s.wr(c + kCtxEnvFlag, kStandInEnvFlag, 4);
  s.wr(c + kCtxParentCompetition, kStandInParentCompetition, 4);
  s.wr(c + kCtxCompetitionRaw, kStandInCompetitionRaw, 4);
  s.wr(c + kCtxStage, kStandInStage, 1);
  s.wr(c + kCtxTieContext, kStandInTieContext, 1);
  s.wr(c + kCtxFixtureWord, kStandInFixtureWord, 2);
  s.wr(c + kCtxFixtureInt, kStandInFixtureInt, 4);
  s.wr(c + kCtxPhase, f.matchPhase, 4);
  s.wr(c + kCtxSubject, f.subjectSide, 4);
  s.wrf(c + kCtxEventX, f.eventX);
  s.wrf(c + kCtxEventY, f.eventY);
  s.wrf(c + kCtxEventZ, f.eventZ);
  s.wr(c + kCtxEventSide, f.eventPlayerSide, 4);
  s.wr(c + kCtxEventSlot, f.eventPlayerSlot, 1);
  s.wr(c + kCtxGoalDiff, static_cast<uint32_t>(f.goalDiff), 4);
  s.wr(c + kSideBase + kSidePlayersOnPitch, f.playersHome, 1);
  s.wr(c + kSideBase + kSideStride + kSidePlayersOnPitch, f.playersAway, 1);
  s.wr(c + kSideBase + kSideTeamByte, kStandInTeamByte, 1);
  s.wr(c + kSideBase + kSideStride + kSideTeamByte, kStandInTeamByte, 1);
  s.wr(c + kCtxMode, kModeMatch, 4);
}

std::optional<uint32_t> FixDemoSelector::PickRow(uint32_t cutId) { return impl_->Pick(cutId); }

std::optional<FixDemoRequest> FixDemoSelector::Request(uint32_t sceneId) {
  Impl& m = *impl_;
  const auto table = m.tables.find(static_cast<int>(sceneId >> 16));
  const uint32_t index = (sceneId & 0xFFFF) - 1;
  if (table == m.tables.end() || index >= table->second.scenes.size()) return std::nullopt;
  FixDemoRequest req;
  req.sceneId = sceneId;
  for (const FixDemoStep& step : table->second.scenes[index].steps) {
    if (step.cut == 0) break;  // the terminator: 0x141F616F0 stops expanding
    const auto row = m.Pick(step.cut);
    const uint32_t group = (step.cut & 0xFFFF) - 1;
    if (!row || group >= table->second.cutGroups.size() || *row >= table->second.cutGroups[group].rows.size())
      return std::nullopt;
    req.steps.push_back({step.cut, *row, table->second.cutGroups[group].rows[*row].fdc});
  }
  return req;
}

std::optional<GoalAnchor> FixDemoSelector::PlaceGoal(const GoalAnchorFacts& facts) { return impl_->PlaceGoal(facts); }

void FixDemoSelector::EndScene(uint32_t sceneId) {
  // 0x141F58FC0: `uVar1 != 0x60001 && 10 < uVar1 - 0x40011` (unsigned) guards the call 0x141F4D070(cat = uVar1 >> 16 & 0xFF).
  constexpr uint32_t kHalfTimeScene = 0x60001, kNoRotationFirst = 0x40011, kNoRotationSpan = 10;
  if (sceneId == kHalfTimeScene || sceneId - kNoRotationFirst <= kNoRotationSpan) return;
  impl_->s.gpr[1] = (sceneId >> 16) & 0xFF;
  impl_->Call(&pes_fn_141F4D070);
}

void FixDemoSelector::RestoreHistoryNode(int category, uint32_t key, uint32_t rangeStart, uint32_t rangeEnd,
                                         const std::vector<uint8_t>& used, uint32_t capacity) {
  Impl& m = *impl_;
  PesState& s = m.s;
  const uint64_t history = kFixDemoHistoryVa + static_cast<uint64_t>(category) * 0x40;
  s.gpr[1] = history;
  s.gpr[2] = key >> 8;     // 0x141F4FFF0(history, key >> 8, key & 0xFF): the node key is param_2 * 0x100 + param_3
  s.gpr[8] = key & 0xFF;
  m.Call(&pes_fn_141F4FFF0);
  const uint64_t payload = s.gpr[0];  // node + 0x28: {u32 start, u32 end, byte vector begin/end/cap}
  s.wr(payload, rangeStart, 4);
  s.wr(payload + 4, rangeEnd, 4);
  const uint64_t bytes = m.heap->Alloc(std::max<uint32_t>(capacity, 1), 0x10);
  for (size_t i = 0; i < used.size(); ++i) s.wr(bytes + i, used[i], 1);
  s.wr(payload + 8, bytes, 8);
  s.wr(payload + 0x10, bytes + used.size(), 8);
  s.wr(payload + 0x18, bytes + capacity, 8);
  // The recorded vectors (the nodes picked this demo, and all picked) were empty between demos: end = begin.
  s.wr(history + 0x18, s.rd(history + 0x10, 8), 8);
  s.wr(history + 0x30, s.rd(history + 0x28, 8), 8);
}

}  // namespace pes
