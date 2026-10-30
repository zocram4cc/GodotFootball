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

// The DB holds two arrays per category, each {groups pointer, group count at +0x10, rows pointer at +0x18} at a 0x20 stride:
// the cut library (0x78-byte variant rows, at DB+0x168; the matcher's) and the scene table (0x38-byte step rows, at DB+8;
// 0x140A7C800's flag test and the list expansion 0x141F616F0's), pes-re-cutscenes.md section 2.2.
constexpr uint64_t kDbBytes = 0x460;  // 0x140A7CC00 allocates the singleton with this size
constexpr uint64_t kDbCategoryStride = 0x20, kDbGroupsOff = 0x168, kDbGroupCountOff = 0x178, kDbRowsOff = 0x180;
constexpr uint64_t kDbSceneTableOff = 8;
constexpr uint64_t kRowBytes = 0x78;  // key u16[10] at +0, then the fdc strings and b0/f/b1 (+0x68/+0x6C/+0x70)
constexpr uint64_t kRowB0 = 0x68, kRowF = 0x6C, kRowB1 = 0x70;
constexpr unsigned kKeyColumns = 10;
// A scene-table step row (0x141F616F0's reads): kind u8 +0, fade f32 +4, cut u32 +8, flags u32 +0xC, c0 u8 +0x10, c1 u8 +0x11,
// w0 u16 +0x12, w1 i16 +0x14, ten {u8,u8,u8} from +0x16, w2 i16 +0x34.
constexpr uint64_t kStepBytes = 0x38, kStepFade = 4, kStepCut = 8, kStepFlags = 0xC, kStepC0 = 0x10, kStepC1 = 0x11,
                   kStepW0 = 0x12, kStepW1 = 0x14, kStepTrip = 0x16, kStepW2 = 0x34;
constexpr unsigned kStepTrips = 10;
// The pick cache: a std::map<u32 cut id, u32 row> at ctx+0x30 (head node pointer, size), nodes keyed at +0x1C, valued at +0x20.
constexpr uint64_t kCtxPickMap = 0x30, kMapNodeBytes = 0x30, kMapNodeKey = 0x1C, kMapNodeValue = 0x20;
constexpr uint64_t kMapTagVa = 0x142BA554B;  // the length-error text 0x141F4FDA0 passes to the node allocator

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
// The table PES's math routines read through memory (0x140A92530's 2^-31 scale at 0x1426550D8, 0x140A92C80's sin/cos: the
// 513-entry quarter wave at 0x142655950, its 512.0 scale at 0x1426550E8, and the atan polynomial's pi/4 at 0x142656158), as
// one run read from the install.
constexpr uint64_t kMathTablesVa = 0x1426550D0, kMathTablesBytes = 0x1090;
// The FixDemo list object's vtable (0x141F5EC60 stores its VA in a stack object; 0x141F61CC0 jumps through slot 3 and
// 0x141F616F0 calls slot 4), read from the install.
constexpr uint64_t kListVtableVa = 0x142BA67D0, kListVtableBytes = 0x40;
// The run object (G+0x21E8 table): 0x140A73E30 reads its flag byte at +0x6C and 0x1419E0900 its target at +0x18, so the object
// must reach past +0x6C (a 0x40 object let the flag read the neighbouring allocation: a nonzero byte sent the side-1 goal's
// 0x140476190 down the "run object has a target" arm and left T.x unassigned). INF: PES's own object size is not decoded.
constexpr uint64_t kAnchorWorldBytes = 0x2E58, kMatchInfoBytes = 0x1600, kManBytes = 0x700, kRunBytes = 0x70, kA0Bytes = 0x200;
constexpr uint64_t kWorldMatchInfo = 0x270, kWorldFieldInfo = 0x2AA8, kWorldManTable = 0x1DD8, kWorldRunTable = 0x21E8;
constexpr uint64_t kWorldTableStride = 0x28;
constexpr uint64_t kWorldTeamInfoTable = 0xA1 * kWorldTableStride, kWorldSquadTable = 0xA3 * kWorldTableStride;
constexpr unsigned kPitchSlots = 22, kSideSlots = 11;  // 2 sides * 11
// The gates' world (0x140472640, 0x14047EB00): the anime record table 0x140A48910 reads (G+0x19C8; record +1 class byte, +8 flag
// word, +0x54 speed), the referee's man record (table index 0x16, which 0x140A48A10 answers), the object behind G+0x2C0 whose
// +0x5C is the time scale, the run object's ready byte A0+0xEC, the first cut id the DemoInfo holds (+0x10: the scene flag test
// 0x140A7C800 takes it), the integer frame rate 0x14351AE50 (0x141E532E0 returns it) and the foul object (0x14047FEB0's layout).
constexpr uint64_t kWorldAnimeTable = 0x19C8, kAnimeBytes = 0x60, kAnimeClass = 1, kAnimeFlags = 8, kAnimeSpeed = 0x54;
constexpr uint64_t kWorldTimeScale = 0x2C0, kTimeScaleBytes = 0x60, kTimeScaleValue = 0x5C;
constexpr unsigned kRefereeSlot = 0x16;
constexpr uint64_t kA0Ready = 0xEC, kDemoInfoBytes = 0x40, kDemoInfoCut = 0x10, kFpsIntVa = 0x14351AE50;
constexpr uint64_t kFoulBytes = 0x20, kFoulScene = 0, kFoulCountdown = 8, kFoulReady = 0xC, kMiFouler = 0x1590, kMiVictim = 0x1594;
constexpr uint32_t kFoulCountdownSeconds = 3;  // 0x14047FEB0: `fps * 3 / scale`
constexpr uint32_t kAnimeFlagBit10 = 1u << 10;  // 0x14047EB00: `*(u32*)(anime + 8) >> 10 & 1`
constexpr uint64_t kSquadBytes = 0x30, kTeamInfoBytes = 0x300, kTeamInfoAttackSign = 0x254;
constexpr uint64_t kFieldInfoBytes = 18 * sizeof(float);
constexpr uint64_t kMiSlotMode = 0x1588, kMiSlot = 0x1590;  // MI+0x1588 == 1: the scorer's slot is MI+0x1590 (0x140474F00)
constexpr uint64_t kMiScorerSide = 0x15C8, kMiScorerPlace = 0x15CC;  // the pair 0x140A62EA0 maps to a slot (0x140476C50)
constexpr uint64_t kManFacing = 0x554, kManPos = 0x55C, kManVel = 0x574, kManMoveMode = 0x5B0, kRunTarget = 0x18;
constexpr uint64_t kA0SceneId = 8, kA0Radius = 0xC, kA0Target = 0x30, kA0Rng = 0x108, kA0RngState = 0x114;
// The goal event record 0x140476C50 takes as its third argument: +0x18 points at the object holding each side's attack bearing.
constexpr uint64_t kEventBytes = 0x100, kEventMatchRecord = 0x18, kMatchRecordBytes = 0x22900, kAttackBearingSide0 = 0x228D8,
                   kAttackBearingSide1 = 0x228D4;
constexpr uint64_t kAnchorOutVa = kScratchVa + 0x100, kAnchorOutRotY = 0x10;  // pos (3 f32) then rotY, inside the scratch segment
constexpr uint64_t kVectorVa = kScratchVa + 0x200;  // the std::vector<u32> of cut ids 0x140474CF0 takes
constexpr uint64_t kMapScratchVa = kScratchVa + 0x300;  // the node allocator's key pointer and result slots
constexpr uint64_t kDbSceneRows = 0x448, kDbSceneRowsEnd = 0x450;  // a std::vector's begin/end: the goal scene rows (0x140A7CBA0)
constexpr uint64_t kSceneRowBytes = 0x3C, kSceneRowFloats = 8, kSceneRowTail = 0x38;

}  // namespace

struct FixDemoSelector::Impl {
  std::map<int, FixDemoTable> tables;
  std::unique_ptr<PesHeap> heap = std::make_unique<PesHeap>();
  PesState s;
  uint64_t ctx = 0, db = 0;
  uint64_t world = 0, matchInfo = 0, fieldInfo = 0, a0 = 0, event = 0, matchRecord = 0;
  std::array<uint64_t, kPitchSlots> anime{};
  uint64_t referee = 0, demo = 0, foul = 0, timeScale = 0;
  uint32_t gateSlot = 0, foulerSlot = 0, victimSlot = 0;
  bool goalPlaced = false;
  std::array<uint64_t, kPitchSlots> man{}, run{};
  std::array<uint64_t, 2> squad{}, teamInfo{};  // per side: the squad key table and the team info (+0x254: attack sign)
  float fullTurnDeg = 0;

  Impl(std::map<int, FixDemoTable> t, std::vector<uint8_t> imageTables, std::vector<uint8_t> mathTables,
       std::vector<uint8_t> listVtable, float fullTurn)
      : tables(std::move(t)), fullTurnDeg(fullTurn) {
    heap->Attach(s);
    s.mem.img.push_back({kImageTablesVa, std::move(imageTables)});
    s.mem.img.push_back({kMathTablesVa, std::move(mathTables)});
    s.mem.img.push_back({kListVtableVa, std::move(listVtable)});
    for (auto [va, bytes] : {std::pair<uint64_t, uint64_t>{kStackVa, kStackBytes}, {kScratchVa, kScratchBytes},
                             {kStaticsVa, kStaticsBytes}, {kEnvGlobalsVa, kEnvGlobalsBytes}, {kDbPageVa, kDbPageBytes},
                             {kWorldSlotPageVa, kWorldSlotPageBytes}})
      s.mem.segs.push_back({va, std::vector<uint8_t>(bytes)});
    s.gpr[4] = kStackVa + kStackBytes - 0x100;
    ctx = heap->Alloc(kCtxBytes, 0x10);
    s.wr(kFixDemoCtxSlotVa, ctx, 8);
    InitPickMap();
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

  // The pick cache is a std::map at ctx+0x30: a sentinel head (left = parent = right = self, black, isnil) and a size.
  void InitPickMap() {
    const uint64_t head = heap->Alloc(kMapNodeBytes, 0x10);
    s.wr(head, head, 8);
    s.wr(head + 8, head, 8);
    s.wr(head + 0x10, head, 8);
    s.wr(head + 0x18, 1, 1);
    s.wr(head + 0x19, 1, 1);
    s.wr(ctx + kCtxPickMap, head, 8);
    s.wr(ctx + kCtxPickMap + 8, 0, 8);
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
        for (size_t ri = 0; ri < g.rows.size(); ++ri) {
          const uint64_t row = rows + (g.firstRow + ri) * kRowBytes;
          for (size_t k = 0; k < kKeyColumns && k < g.rows[ri].keys.size(); ++k) s.wr(row + k * 2, g.rows[ri].keys[k], 2);
          s.wr(row + kRowB0, g.rows[ri].b0, 4);
          s.wrf(row + kRowF, g.rows[ri].f);
          s.wr(row + kRowB1, g.rows[ri].b1, 4);
        }
      }
      s.wr(base + kDbGroupsOff, groups, 8);
      s.wr(base + kDbGroupCountOff, table.cutGroups.size(), 4);
      s.wr(base + kDbRowsOff, rows, 8);
      BuildSceneTable(db + kDbSceneTableOff + static_cast<uint64_t>(cat) * kDbCategoryStride, table);
    }
  }

  // The scene table (T2): a {start, count} group per scene into one array of step rows.
  void BuildSceneTable(uint64_t base, const FixDemoTable& table) {
    uint32_t stepCount = 0;
    for (const FixDemoScene& scene : table.scenes) stepCount += static_cast<uint32_t>(scene.steps.size());
    const uint64_t groups = heap->Alloc(std::max<size_t>(table.scenes.size(), 1) * 4, 0x10);
    const uint64_t rows = Zalloc(std::max<uint32_t>(stepCount, 1) * kStepBytes);
    uint32_t next = 0;
    for (size_t si = 0; si < table.scenes.size(); ++si) {
      const auto& steps = table.scenes[si].steps;
      s.wr(groups + si * 4, next, 2);
      s.wr(groups + si * 4 + 2, steps.size(), 2);
      for (const FixDemoStep& step : steps) {
        const uint64_t row = rows + next++ * kStepBytes;
        s.wr(row, step.kind, 1);
        s.wrf(row + kStepFade, step.fade);
        s.wr(row + kStepCut, step.cut, 4);
        s.wr(row + kStepFlags, step.flags, 4);
        s.wr(row + kStepC0, step.c0, 1);
        s.wr(row + kStepC1, step.c1, 1);
        s.wr(row + kStepW0, step.w0, 2);
        s.wr(row + kStepW1, static_cast<uint16_t>(step.w1), 2);
        for (size_t k = 0; k < kStepTrips && k < step.trip.size(); ++k)
          for (size_t b = 0; b < 3; ++b) s.wr(row + kStepTrip + 3 * k + b, step.trip[k][b], 1);
        s.wr(row + kStepW2, static_cast<uint16_t>(step.w2), 2);
      }
    }
    s.wr(base, groups, 8);
    s.wr(base + 0x10, table.scenes.size(), 4);
    s.wr(base + 0x18, rows, 8);
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
    fieldInfo = Zalloc(kFieldInfoBytes);
    a0 = Zalloc(kA0Bytes);
    event = Zalloc(kEventBytes);
    matchRecord = Zalloc(kMatchRecordBytes);
    s.wr(event + kEventMatchRecord, matchRecord, 8);
    s.wr(world + kWorldMatchInfo, matchInfo, 8);
    s.wr(world + kWorldFieldInfo, fieldInfo, 8);
    for (unsigned slot = 0; slot < kPitchSlots; ++slot) {
      man[slot] = Zalloc(kManBytes);
      run[slot] = Zalloc(kRunBytes);
      anime[slot] = Zalloc(kAnimeBytes);
      s.wr(world + kWorldAnimeTable + slot * kWorldTableStride, anime[slot], 8);
      s.wr(world + kWorldManTable + slot * kWorldTableStride, man[slot], 8);
      s.wr(world + kWorldRunTable + slot * kWorldTableStride, run[slot], 8);
    }
    referee = Zalloc(kManBytes);
    s.wr(world + kWorldManTable + kRefereeSlot * kWorldTableStride, referee, 8);
    demo = Zalloc(kDemoInfoBytes);
    foul = Zalloc(kFoulBytes);
    // ponytail: the time scale object behind G+0x2C0 (+0x5C, read live as 1.0 on every capture) has no GF counterpart: the
    // match speed setting that PES's [[G+0x2C0]+0x5C] carries; removed with it.
    timeScale = Zalloc(kTimeScaleBytes);
    s.wrf(timeScale + kTimeScaleValue, 1.0f);
    s.wr(world + kWorldTimeScale, timeScale, 8);
    // Per side: the squad table 0x140A62EA0 maps a squad key through (word 0 = the side, word 1+i = slot i's key) and the
    // team info (G+(0xA1+side)*0x28; +0x254 = the attack sign 0x140476190 reads: 1 for side 0, 0xFF for side 1).
    // GF keys its squad by the index on the pitch, so the key of slot i is i.
    for (unsigned side = 0; side < 2; ++side) {
      squad[side] = Zalloc(kSquadBytes);
      teamInfo[side] = Zalloc(kTeamInfoBytes);
      s.wr(squad[side], side, 4);
      for (unsigned i = 0; i < kSideSlots; ++i) s.wr(squad[side] + 4 + 4 * i, i, 4);
      s.wr(teamInfo[side] + kTeamInfoAttackSign, side == 0 ? 1 : 0xFF, 1);
      s.wr(world + kWorldSquadTable + side * kWorldTableStride, squad[side], 8);
      s.wr(world + kWorldTeamInfoTable + side * kWorldTableStride, teamInfo[side], 8);
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

  // The goal's anchor and the two inputs PES computes for it before the anchor (the builder 0x140478190): the scene's run
  // radius A0+0xC by 0x140474CF0, then the celebration spot A0+0x30 by 0x140476C50, which the scorer's run object keeps as
  // his target; the scene anchor 0x140474F00 and the flip 0x140478FF0 follow, as the DemoInfo's first tick runs them.
  std::optional<GoalAnchor> PlaceGoal(const GoalAnchorFacts& f) {
    const auto goal = tables.find(kFixDemoGoal);
    const uint32_t index = (f.sceneId & 0xFFFF) - 1;
    if (goal == tables.end() || (f.sceneId >> 16) != static_cast<uint32_t>(kFixDemoGoal) ||
        index >= goal->second.goalSceneRows.size() || f.scorerSlot >= kPitchSlots)
      return std::nullopt;
    const uint32_t side = f.scorerSlot / kSideSlots, place = f.scorerSlot % kSideSlots;
    s.wr(matchInfo + kMiSlotMode, 1, 4);
    s.wr(matchInfo + kMiSlot, f.scorerSlot, 4);
    s.wr(matchInfo + kMiScorerSide, side, 4);
    s.wr(matchInfo + kMiScorerPlace, place, 4);
    for (size_t k = 0; k < f.field.size(); ++k) s.wrf(fieldInfo + 4 * k, f.field[k]);
    s.wrf(matchRecord + kAttackBearingSide0, f.attackDeg[0]);
    s.wrf(matchRecord + kAttackBearingSide1, f.attackDeg[1]);
    s.wrf(kFpsVa, f.fps);
    const uint64_t m = man[f.scorerSlot], r = run[f.scorerSlot];
    s.wrf(m + kManFacing, f.facing);
    s.wr(m + kManMoveMode, f.moveMode, 4);
    for (unsigned i = 0; i < 3; ++i) {
      s.wrf(m + kManPos + 4 * i, f.pos[i]);
      s.wrf(m + kManVel + 4 * i, f.vel[i]);
      s.wrf(r + kRunTarget + 4 * i, 0.0f);  // the target is what 0x140476C50 is about to give him
    }
    // A0: the DemoInfo's defaults (0x140478090), then the fields the builder sets before the calls.
    for (uint64_t off = 0; off < kA0Bytes; off += 8) s.wr(a0 + off, 0, 8);
    s.gpr[1] = a0;
    Call(&pes_fn_140478090);
    s.wr(a0 + kA0SceneId, f.sceneId, 4);
    s.wr(a0 + kA0RngState, f.rngState, 4);
    // 0x140474CF0(A0, std::vector<u32>* of the scenes): {begin, end, capacity} over the ids.
    const uint64_t ids = Zalloc(std::max<size_t>(f.cuts.size(), 1) * 4);
    for (size_t i = 0; i < f.cuts.size(); ++i) s.wr(ids + 4 * i, f.cuts[i], 4);
    s.wr(kVectorVa, ids, 8);
    s.wr(kVectorVa + 8, ids + 4 * f.cuts.size(), 8);
    s.wr(kVectorVa + 0x10, ids + 4 * f.cuts.size(), 8);
    s.gpr[1] = a0;
    s.gpr[2] = kVectorVa;
    Call(&pes_fn_140474CF0);
    GoalAnchor out;
    out.radius = ReturnedFloat();
    heap->Free(ids);
    s.wrf(a0 + kA0Radius, out.radius);
    // 0x140476C50(A0, &out, event record, 0): the celebration spot.
    s.gpr[1] = a0;
    s.gpr[2] = kAnchorOutVa + 0x20;
    s.gpr[8] = event;
    s.gpr[9] = 0;
    Call(&pes_fn_140476C50);
    for (unsigned i = 0; i < 3; ++i) {
      out.target[i] = s.rdf(kAnchorOutVa + 0x20 + 4 * i);
      s.wrf(a0 + kA0Target + 4 * i, out.target[i]);
      // ponytail: the writer of the run object's +0x18 is not decoded; it equals A0+0x30 on all 7 captured goals.
      s.wrf(r + kRunTarget + 4 * i, out.target[i]);
    }
    // 0x140474F00(A0, arg2 (not read), &pos, &rotY)
    s.gpr[1] = a0;
    s.gpr[2] = 0;
    s.gpr[8] = kAnchorOutVa;
    s.gpr[9] = kAnchorOutVa + kAnchorOutRotY;
    Call(&pes_fn_140474F00);
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
    // The goal builder's last calls: the run object's timers (0x14047A710(A0, event): A0+0x14 = fps x 4 / scale, 10 for
    // 0x3013D..F, A0+0x18 = fps x 30 / scale), and the DemoInfo's first cut id the tail's scene flag test reads (+0x10).
    s.wr(demo + kDemoInfoCut, f.cuts.empty() ? f.sceneId : f.cuts.front(), 4);
    s.gpr[1] = a0;
    s.gpr[2] = event;
    Call(&pes_fn_14047A710);
    gateSlot = f.scorerSlot;
    goalPlaced = true;
    return out;
  }

  // One call of the goal gate 0x140472640(A0, event, DemoInfo) with the scorer as he is this frame; A0+0xEC after it.
  bool GoalGate(const GoalGateFacts& g) {
    if (!goalPlaced) return false;
    const uint64_t m = man[gateSlot], a = anime[gateSlot];
    s.wrf(m + kManFacing, g.facing);
    for (unsigned i = 0; i < 3; ++i) s.wrf(m + kManPos + 4 * i, g.pos[i]);
    s.wr(a + kAnimeClass, g.animeClass, 1);
    s.wrf(a + kAnimeSpeed, g.animeSpeed);
    s.gpr[1] = a0;
    s.gpr[2] = event;
    s.gpr[8] = demo;
    Call(&pes_fn_140472640);
    return s.rd(a0 + kA0Ready, 1) != 0;
  }

  // The foul object as its constructor 0x14047FEB0 leaves it (scene, flags 0, countdown `(int)(fps * 3 / scale)`, ready 0) and
  // the foul in the match info. ponytail: the constructor itself is not translated (its callees build effect objects); the
  // countdown line and the zeroed flags (object +4, +5) are mirrored, +4/+5 are INF zero (the live foul rows' `Fo.b4/b5`).
  void BeginFoul(const FoulGateStart& st) {
    s.wr(kFpsIntVa, static_cast<uint32_t>(st.fps), 4);
    s.wrf(kFpsVa, st.fps);
    for (uint64_t off = 0; off < kFoulBytes; off += 8) s.wr(foul + off, 0, 8);
    s.wr(foul + kFoulScene, st.sceneId, 4);
    const float scale = s.rdf(timeScale + kTimeScaleValue);
    s.wr(foul + kFoulCountdown, static_cast<uint32_t>(static_cast<int>(static_cast<float>(static_cast<uint32_t>(st.fps) * kFoulCountdownSeconds) / scale)), 4);
    s.wr(matchInfo + kMiFouler, st.foulerSlot, 4);
    s.wr(matchInfo + kMiVictim, st.victimSlot, 4);
    foulerSlot = st.foulerSlot;
    victimSlot = st.victimSlot;
  }

  bool FoulGate(const FoulGateFacts& f) {
    if (foulerSlot >= kPitchSlots || victimSlot >= kPitchSlots) return false;
    const auto put = [&](uint64_t record, const std::array<float, 3>& pos) {
      for (unsigned i = 0; i < 3; ++i) s.wrf(record + kManPos + 4 * i, pos[i]);
    };
    put(man[foulerSlot], f.foulerPos);
    put(man[victimSlot], f.victimPos);
    put(referee, f.refereePos);
    const uint64_t flags = anime[victimSlot] + kAnimeFlags;
    s.wr(flags, f.victimFlag10 ? kAnimeFlagBit10 : 0, 4);
    s.gpr[1] = foul;
    Call(&pes_fn_14047EB00);
    return s.rd(foul + kFoulReady, 1) != 0;
  }

  // A translated function is entered as the dispatch enters it: a return address pushed.
  void Call(void (*fn)(PesState&)) {
    s.gpr[4] -= 8;
    s.wr(s.gpr[4], 0, 8);
    fn(s);
    s.gpr[4] += 8;
  }

  // 0x141F4FDA0: the variant row of a cut group, through PES's own wrapper (its pick cache is the std::map at ctx+0x30).
  std::optional<uint32_t> Pick(uint32_t cutId) {
    const uint32_t category = (cutId >> 16) & 0xFF;
    const uint32_t group = (cutId & 0xFFFF) - 1;  // 0x141F4FDB0: movzx ebx, dx; dec ebx
    // 0x140A7CB40(db, &rows, &count, category, group): the group's rows pointer (the wrapper returns row * 0x78 + rows).
    s.gpr[1] = db;
    s.gpr[2] = kOutRowsVa;
    s.gpr[8] = kOutCountVa;
    s.gpr[9] = category;
    s.wr(s.gpr[4] + kShadowBytes, group, 4);
    Call(&pes_fn_140A7CB40);
    const uint64_t rows = s.rd(kOutRowsVa, 8);
    if (rows == 0) return std::nullopt;
    s.gpr[1] = ctx;
    s.gpr[2] = cutId;
    Call(&pes_fn_141F4FDA0);
    if (s.gpr[0] == 0) return std::nullopt;
    return static_cast<uint32_t>((s.gpr[0] - rows) / kRowBytes);
  }

  // Inserts {cutId -> row} into the pick map the way 0x141F4FDA0 does (0x141F4FE87..0x141F4FEEB): find the lower bound, then
  // the node allocator 0x141056DF0 and the tree insert 0x141056F40 with that bound as the hint, then the value at node+0x20.
  void StorePick(uint32_t cutId, uint32_t row) {
    const uint64_t map = ctx + kCtxPickMap, head = s.rd(map, 8);
    uint64_t bound = head, node = s.rd(head + 8, 8);
    while (s.rd(node + 0x19, 1) == 0) {
      if (static_cast<uint32_t>(s.rd(node + kMapNodeKey, 4)) < cutId) {
        node = s.rd(node + 0x10, 8);
      } else {
        bound = node;
        node = s.rd(node, 8);
      }
    }
    if (bound != head && cutId >= static_cast<uint32_t>(s.rd(bound + kMapNodeKey, 4))) {
      s.wr(bound + kMapNodeValue, row, 4);  // already cached: PES keeps the node, here the test's pick wins
      return;
    }
    const uint64_t keyVa = kMapScratchVa, keyPtrVa = kMapScratchVa + 8, outVa = kMapScratchVa + 0x10;
    s.wr(keyVa, cutId, 4);
    s.wr(keyPtrVa, keyVa, 8);
    s.gpr[1] = map;
    s.gpr[2] = kMapTagVa;
    s.gpr[8] = keyPtrVa;
    s.gpr[9] = outVa;
    Call(&pes_fn_141056DF0);
    const uint64_t fresh = s.gpr[0];
    s.gpr[1] = map;
    s.gpr[2] = outVa;
    s.gpr[8] = bound;
    s.gpr[9] = fresh + kMapNodeKey;
    s.wr(s.gpr[4] + kShadowBytes, fresh, 8);
    Call(&pes_fn_141056F40);
    s.wr(s.rd(outVa, 8) + kMapNodeValue, row, 4);
  }

  // The float a function returns in xmm0.
  float ReturnedFloat() const {
    const uint32_t bits = static_cast<uint32_t>(s.xmm[0][0]);
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
  }
};

std::unique_ptr<FixDemoSelector> FixDemoSelector::Create(std::map<int, FixDemoTable> tables, const PeImage& exe,
                                                         std::string* error) {
  std::vector<uint8_t> image(kImageTablesBytes);
  if (!exe.ReadVa(kImageTablesVa, image.size(), image.data())) {
    if (error) *error = "the image does not map the FixDemo evaluator's switch tables";
    return nullptr;
  }
  std::vector<uint8_t> math(kMathTablesBytes);
  if (!exe.ReadVa(kMathTablesVa, math.size(), math.data())) {
    if (error) *error = "the image does not map the math routines' tables";
    return nullptr;
  }
  std::vector<uint8_t> vtable(kListVtableBytes);
  if (!exe.ReadVa(kListVtableVa, vtable.size(), vtable.data())) {
    if (error) *error = "the image does not map the FixDemo list's vtable";
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
      new FixDemoSelector(std::make_unique<Impl>(std::move(tables), std::move(image), std::move(math), std::move(vtable), fullTurn)));
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
  for (uint64_t off = 0; off < kCtxBytes; off += 8) s.wr(m.ctx + off, 0, 8);  // 0x141F50B90 resets; unread fields stay 0
  // ponytail: the reset's map clear (0x141F50B90 frees the nodes) is not run: the old nodes stay in the arena, a few dozen
  // bytes per picked cut and event, against 8 MiB; a long session that fills it needs the clear translated.
  m.InitPickMap();
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

void FixDemoSelector::RestorePick(uint32_t cutId, uint32_t row) { impl_->StorePick(cutId, row); }

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

bool FixDemoSelector::GoalGateReady(const GoalGateFacts& facts) { return impl_->GoalGate(facts); }

void FixDemoSelector::BeginFoulGate(const FoulGateStart& start) { impl_->BeginFoul(start); }

bool FixDemoSelector::FoulGateReady(const FoulGateFacts& facts) { return impl_->FoulGate(facts); }

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
