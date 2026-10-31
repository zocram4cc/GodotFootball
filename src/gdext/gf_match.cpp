#include "gdext/gf_match.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <array>

#include <godot_cpp/core/math.hpp>
#include "data/matchdata.hpp"
#include "gametypes.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/pesdesignation.hpp"
#include "onthepitch/pesoffball.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesgoalscene.hpp"
#include "onthepitch/referee.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/player/playerofficial.hpp"
#include "onthepitch/player/controller/elizacontroller.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

namespace godot {
namespace {

constexpr int TEAMS = 2;
// PES's logic frame (Milestone A1): GfMatch.step() is one 1/54 s tick. The render
// side interpolates between the two ticks a frame straddles, as match.gd does.
constexpr float TICK_S = 1.0f / 54.0f;

// GF is Z up with the pitch in XY; Fox/Godot is Y up: (x, y, z) -> (x, z, -y).
Vector3 ToFox(const blunted::Vector3& v) { return Vector3(v.coords[0], v.coords[2], -v.coords[1]); }

}  // namespace

GfMatch::GfMatch() = default;
GfMatch::~GfMatch() = default;

void GfMatch::_bind_methods() {
  ClassDB::bind_method(D_METHOD("start", "library", "db_sql", "anims_dir", "home", "away", "seed", "minutes",
                                "pes_loop"),
                       &GfMatch::start, DEFVAL(false));
  ClassDB::bind_method(D_METHOD("step"), &GfMatch::step);
  ClassDB::bind_method(D_METHOD("is_over"), &GfMatch::is_over);
  ClassDB::bind_method(D_METHOD("player_count"), &GfMatch::player_count);
  ClassDB::bind_method(D_METHOD("player_position", "index"), &GfMatch::player_position);
  ClassDB::bind_method(D_METHOD("player_position_previous", "index"),
                       &GfMatch::player_position_previous);
  ClassDB::bind_method(D_METHOD("ball_position_previous"), &GfMatch::ball_position_previous);
  ClassDB::bind_method(D_METHOD("player_facing", "index"), &GfMatch::player_facing);
  ClassDB::bind_method(D_METHOD("player_velocity", "index"), &GfMatch::player_velocity);
  ClassDB::bind_method(D_METHOD("player_team", "index"), &GfMatch::player_team);
  ClassDB::bind_method(D_METHOD("player_clip", "index"), &GfMatch::player_clip);
  ClassDB::bind_method(D_METHOD("player_database_id", "index"), &GfMatch::player_database_id);
  ClassDB::bind_method(D_METHOD("ball_position"), &GfMatch::ball_position);
  ClassDB::bind_method(D_METHOD("match_time_ms"), &GfMatch::match_time_ms);
  ClassDB::bind_method(D_METHOD("score", "team"), &GfMatch::score);
  ClassDB::bind_method(D_METHOD("field_half_width"), &GfMatch::field_half_width);
  ClassDB::bind_method(D_METHOD("field_half_length"), &GfMatch::field_half_length);
  ClassDB::bind_method(D_METHOD("set_cutscene_environment", "stadium", "time_of_day", "weather"),
                       &GfMatch::set_cutscene_environment);
  ClassDB::bind_method(D_METHOD("take_cutscene"), &GfMatch::take_cutscene);
  ClassDB::bind_method(D_METHOD("cutscene_state"), &GfMatch::cutscene_state);
}

Error GfMatch::start(const Ref<PesLibrary>& library, const String& db_sql, const String& anims_dir, int home,
                     int away, int seed, int minutes, bool pes_loop) {
  if (library.is_null() || !library->rig()) {
    UtilityFunctions::push_error("GfMatch: library is not open");
    return ERR_UNCONFIGURED;
  }
  library_ = library;
  match_.reset();
  db_ = std::make_unique<blunted::Database>();
  if (!db_->LoadSqlScript(FromGd(db_sql))) {
    UtilityFunctions::push_error("GfMatch: cannot load ", db_sql);
    return ERR_FILE_CANT_OPEN;
  }
  SetDB(db_.get());
  MatchSetup setup;
  setup.homeTeamDbId = home;
  setup.awayTeamDbId = away;
  setup.seed = unsigned(seed);
  setup.durationMinutes = minutes;
  setup.animationDir = FromGd(anims_dir);
  setup.rig = library->rig();
  // PES's match clock: its constants come from the install's own exe.
  setup.pesClockConstants = library->clock_constants();
  setup.pesBallConstants = library->ball_constants();
  setup.pesJudgeConstants = library->judge_constants();
  setup.pesRngConstants = library->rng_constants();
  setup.pesKeeperConstants = library->keeper_constants();
  setup.pesStaminaConstants = library->stamina_constants();
  setup.pesPlayerDatabase = library->player_database();
  setup.pesFormations = library->formation_tables();
  setup.pesSheetConstants = library->sheet_constants();
  setup.pesStoppageConstants = library->stoppage_constants();
  setup.pesForfeitConstants = library->forfeit_constants();
  setup.pesRestartSpotConstants = library->restart_spot_constants();
  setup.pesKickErrorConstants = library->kick_error_constants();
  setup.pesCarrierConstants = library->carrier_constants();
  setup.pesSetPieceTakerConstants = library->setpiece_taker_constants();
  setup.pesSetPieceTeamConstants = library->setpiece_team_constants();
  setup.pesDefensiveShapeConstants = library->defensive_shape_constants();
  setup.pesDefenderDecisionConstants = library->defender_decision_constants();
  setup.pesContactConstants = library->contact_constants();
  setup.pesEventRingTable = library->event_ring_table();
  setup.pesEventConstants = library->event_constants();
  setup.pesClips = library->pes_clips();
  setup.pesDodgeConstants = library->dodge_constants();
  setup.pesAnimetable = library->animetable();
  setup.pesMbinfo = library->mbinfo();
  setup.pesTackleConstants = library->tackle_constants();
  setup.pesStrategyConstants = library->strategy_constants();
  setup.pesTeamConceptConstants = library->team_concept_constants();
  setup.pesSubstitutionConstants = library->substitution_constants();
  setup.pesOffBallConstants = library->off_ball_constants();
  setup.pesMoveStepConstants = library->movestep_constants();
  setup.pesPressConstants = library->press_constants();
  setup.pesMarkTableConstants = library->mark_table_constants();
  setup.pesImageTables = library->image_tables();
  Properties config;
  config.SetBool("pes_loop", pes_loop);  // the tick-order switch (SW)
  match_ = std::make_unique<Match>(new MatchData(home, away), setup, config);  // owns the MatchData
  // PES's FixDemo selection (foul and goal tables) on GF's arena. Without the install's tables there is no cutscene.
  fixdemo_.reset();
  goalTable_.reset();
  pendingCutscene_.reset();
  timeline_.reset();
  tick_ = 0;
  lastScore_[0] = lastScore_[1] = 0;
  foulStopSeen_ = false;
  std::map<int, pes::FixDemoTable> tables;
  for (int category : {pes::kFixDemoFoul, pes::kFixDemoGoal}) {
    std::string err;
    auto bytes = library->vfs().Read(pes::FixDemoTablePath(category), &err);
    auto table = bytes ? pes::DecodeFixDemoTable(*bytes, category, &err) : std::nullopt;
    if (!table) {
      UtilityFunctions::push_warning(String("GfMatch: no FixDemo table for category ") + String::num_int64(category) + ": " +
                                     err.c_str());
      tables.clear();
      break;
    }
    tables[category] = std::move(*table);
  }
  if (!tables.empty() && library->rng_constants() && library->exe()) {
    goalTable_ = tables.at(pes::kFixDemoGoal);
    std::string err;
    fixdemo_ = pes::FixDemoSelector::Create(std::move(tables), *library->exe(), &err);
    if (!fixdemo_) UtilityFunctions::push_warning(String("GfMatch: FixDemo selection unavailable: ") + err.c_str());
  }
  if (fixdemo_) {
    // ponytail: PES seeds both Random1000 sets from its clock modulo 10000 (0x141B34AD0 in the ctx constructor 0x141F4C950)
    // and the goal object's generator from its own constructor; GF derives all from the match seed.
    fixdemo_->Seed(unsigned(seed) % 10000u, (unsigned(seed) / 10000u) % 10000u);
    goalRng_ = std::make_unique<pes::Random>(*library->rng_constants());
    for (int slot = 0; slot < 3; ++slot) goalRng_->SetSeed(slot, unsigned(seed) + unsigned(slot));
  }
  RefreshPlayers();
  return OK;
}

void GfMatch::RefreshPlayers() {
  players_.clear();
  teamOf_.clear();
  for (int t = 0; t < TEAMS; ++t) {
    std::vector<Player*> team;
    match_->GetActiveTeamPlayers(t, team);
    for (Player* p : team) players_.push_back(p), teamOf_.push_back(t);
  }
}

Vector3 GfMatch::player_position_previous(int index) const {
  // The position at the previous logic frame, from the snapshot the previous step()
  // left. The render side lerps towards player_position().
  if (index < 0 || index >= player_count()) return Vector3();
  if (previousValid_ && index < previousPlayers_.size()) return ToFox(previousPlayers_[index]);
  return player_position(index);
}

Dictionary GfMatch::player_clip(int i) const {
  Dictionary d;
  if (i < 0 || i >= player_count()) return d;
  const pes::IssuedClip& clip = players_[i]->GetIssuedClip();
  d["serial"] = int(clip.serial);
  d["motion"] = clip.request.motion;
  d["group"] = clip.request.group;
  d["mirror"] = clip.request.mirror;
  d["start_time"] = clip.request.startTime;
  d["start_rate"] = clip.request.startRate;
  d["rate"] = clip.request.rate;
  d["duration"] = clip.request.duration;
  d["playback"] = clip.playback;
  return d;
}

Vector3 GfMatch::ball_position_previous() const {
  return match_ ? ToFox(previousBall_) : Vector3();
}

void GfMatch::step() {
  if (!match_ || match_->IsGameOver()) return;
  SetDB(db_.get());  // the engine reads the global database; another match may have set it
  // The snapshot the render side interpolates from: taken before the step, so it is
  // the previous logic frame's pose (Milestone A1).
  previousPlayers_.clear();
  for (Player* p : players_) previousPlayers_.push_back(p->GetPosition());
  previousBall_ = match_->GetBall()->GetPositionBuffer();
  previousValid_ = true;
  match_->Process();
  RefreshPlayers();  // substitutions change the active set
  PollCutscenes();
  ++tick_;
  // PES's cutscene clock: one update per logic frame once DemoInfo exists. A finished timeline is dropped on the frame after
  // its `ended` flag was read (cutscene_state).
  if (timeline_ && timeline_->Done()) timeline_.reset();
  if (timeline_) {
    // PES's gate function, once a frame while DemoInfo state 2 waits (it latches: the run object's ready byte stays set). The
    // first call is the frame before the timeline's first Tick (the DemoInfo creation frame): Tick n sees call n + 1, which puts
    // the timer-decided gate of the logged g4 at Tick 215 (timer 216 - 1: pes-port-CutsceneTiming3.md section 2).
    if (!gateOpen_ && tick_ + 1 >= timelineStartTick_) gateOpen_ = RunGate();
    if (tick_ >= timelineStartTick_) {
      clockPrevTicks_ = timeline_->State().clock;
      timeline_->Tick(gateOpen_);
      if (timeline_->State().ended && fixdemo_) fixdemo_->EndScene(timelineScene_);  // 0x141F58FC0: the rotation word advances
    }
  }
}

// ponytail: PES's sanction tree 0x14047F170 returns the foul scene id from the foul record (kind, fouler, period, injury and
// excite flags: docs/re/pes-re-sanction-switch.md); it is proven (snapdiff 14/14) but not in the committed layer and GF's foul
// state does not feed its inputs. 0x20008 (FOUL_WARN_CARD_N) is the id every ordinary foul stop of the live captures returned
// (live2 seq090, case 0). Card, injury and penalty stops play it too until the tree is translated.
constexpr uint32_t kOrdinaryFoulScene = 0x20008;
// ponytail: the goal level (goal ctx [0]) and stage gate of the generic pick 0x140477400 come from a goal context GF does not
// have (pesgoalscene.hpp); an ordinary goal is level 0.
constexpr pes::GoalSceneRule kGoalRule{0, false};
// ponytail: the scorer's move word man+0x5B0 (PES's per-frame man update), which picks velocity or facing for the anchor's
// rotation: GF has no such word; 0 is what two captured goals held (the velocity's heading). The anchor's run target and
// radius are no longer stand-ins: the translated 0x140476C50 and 0x140474CF0 compute them from the facts below.
constexpr uint32_t kScorerMoveMode = 0;

void GfMatch::set_cutscene_environment(int stadium, int time_of_day, int weather) {
  cutsceneEnvironment_.stadium = uint32_t(stadium);
  cutsceneEnvironment_.timeOfDay = uint32_t(time_of_day);
  cutsceneEnvironment_.weather = uint32_t(weather);
}

void GfMatch::PollCutscenes() {
  if (!fixdemo_ || !goalRng_ || !goalTable_) return;
  pes::FixDemoFacts facts = cutsceneEnvironment_;
  switch (match_->GetMatchPhase()) {  // 0x141F51630's switch on the period, as GF's phases
    case e_MatchPhase_2ndHalf: facts.matchPhase = 1; break;
    case e_MatchPhase_1stExtraTime: facts.matchPhase = 2; break;
    case e_MatchPhase_2ndExtraTime: facts.matchPhase = 3; break;
    case e_MatchPhase_Penalties: facts.matchPhase = 4; break;
    default: facts.matchPhase = 0; break;
  }
  facts.playersHome = facts.playersAway = 0;
  for (int t : teamOf_) ++(t == 0 ? facts.playersHome : facts.playersAway);
  facts.goalDiff = match_->GetScore(0) - match_->GetScore(1);
  const int scores[2] = {match_->GetScore(0), match_->GetScore(1)};
  const bool scored = scores[0] + scores[1] > lastScore_[0] + lastScore_[1];
  const int scorer = scores[0] > lastScore_[0] ? 0 : 1;
  lastScore_[0] = scores[0];
  lastScore_[1] = scores[1];
  const RefereeBuffer& stop = match_->GetRefereeBuffer();
  const bool foul = stop.active && (stop.desiredSetPiece == e_SetPiece_FreeKick || stop.desiredSetPiece == e_SetPiece_Penalty) &&
                    (!foulStopSeen_ || stop.stopTime != lastFoulStop_ms_);
  if (timeline_) return;  // one scene at a time: PES's DemoInfo is a singleton (a stop during a scene is not asked)
  if (scored) {
    // The goal scene: PES's generic pick (pesgoalscene), then the variant of each of its cuts.
    const auto scene = pes::PickGoalScene(*goalTable_, kGoalRule, *goalRng_);
    if (!scene) return;
    const Vector3 ball = ToFox(match_->GetBall()->Predict(0));
    facts.subjectSide = uint32_t(scorer);
    facts.eventX = ball.x;
    facts.eventY = ball.y;
    facts.eventZ = ball.z;
    fixdemo_->BeginEvent(facts);
    pendingCutscene_ = fixdemo_->Request(*scene);
    gateScorer_ = match_->GetLastGoalScorer();
    gateIsGoal_ = true;
    pendingAnchor_ = PlaceGoalScene(*scene, ball);
    // The gate runs on the run object the anchor's PlaceGoal built: a goal whose scorer is not in the active set (no anchor, no
    // run object) plays no scene.
    if (!pendingAnchor_) pendingCutscene_.reset();
    if (pendingCutscene_) StartTimeline(*pendingCutscene_);
  } else if (foul) {
    foulStopSeen_ = true;
    lastFoulStop_ms_ = stop.stopTime;
    const Vector3 at = ToFox(stop.restartPos);
    facts.eventX = at.x;
    facts.eventY = at.y;
    facts.eventZ = at.z;
    fixdemo_->BeginEvent(facts);
    pendingCutscene_ = fixdemo_->Request(kOrdinaryFoulScene);
    pendingAnchor_.reset();
    // ponytail: an offside stop reaches here with no fouler (PES's offside DemoInfo is not a foul object); slot 0 stands in for
    // both men until the offside scene's own gate is decoded.
    gateFouler_ = match_->GetReferee()->GetCurrentFoulPlayer();
    gateVictim_ = match_->GetReferee()->GetCurrentFoulVictim();
    gateIsGoal_ = false;
    if (pendingCutscene_) {
      pes::FoulGateStart start;
      start.sceneId = kOrdinaryFoulScene;
      start.foulerSlot = uint32_t(std::max(PesSlot(gateFouler_), 0));
      start.victimSlot = uint32_t(std::max(PesSlot(gateVictim_), 0));
      start.fps = pes::offball::kPesFps;
      fixdemo_->BeginFoulGate(start);
      StartTimeline(*pendingCutscene_);
    }
  }
}

// The goal scene's anchor (the scorer's celebration spot PES places the cut's player at: writer 0x14047D280 over the calc
// 0x140474F00) from GF's goal: the scorer's pose, the side and squad index, the pitch and PES's tick rate. The scorer is
// the last touch of the scoring team (Match::GetLastGoalScorer), as the goal celebration finds him.
std::optional<pes::GoalAnchor> GfMatch::PlaceGoalScene(uint32_t sceneId, const Vector3& event) {
  Player* scorer = match_->GetLastGoalScorer();
  const int slot = PesSlot(scorer);
  if (!scorer || slot < 0) return std::nullopt;
  const int side = slot / pes::designation::kSideSlots;
  const Vector3 pos = ToFox(scorer->GetPosition());
  const Vector3 vel = ToFox(scorer->GetMovement()) / TICK_S / pes::offball::kPesFps;  // metres per PES frame
  const Vector3 dir = ToFox(scorer->GetBodyDirectionVec());
  pes::GoalAnchorFacts f;
  f.sceneId = sceneId;
  f.scorerSlot = uint32_t(slot);
  f.pos = {pos.x, pos.y, pos.z};
  f.vel = {vel.x, vel.y, vel.z};
  f.facing = float(Math::fposmod(Math::rad_to_deg(Math::atan2(dir.x, dir.z)), 360.0));  // PES's bearing: atan2(dx, dz)
  f.moveMode = kScorerMoveMode;
  // PES's FieldInfo F0..F17 is GF's one pitch table (gametypes.hpp).
  f.field = {pesFieldHalfLength, pesFieldHalfWidth,  pesGoalInnerHalfWidth, pesGoalInnerHeight,    pesGoalAreaLineX,
             pesGoalAreaHalfWidth, pesPenaltyAreaLineX, pesPenaltyAreaHalfWidth, pesPenaltySpotX,  pesNetHeight,
             pesFieldF10,        pesFieldF11,        pesFieldF12,          pesNetRoofBackX,        pesNetFloorBackX,
             pesCrossbarThickness, pesPostThickness, pesCircleRadius};
  f.fps = pes::offball::kPesFps;
  // The scoring side attacked the goal the ball is in: its bearing points along +x (90) or -x (270); the other side's is the
  // opposite (the second half swaps them).
  f.attackDeg[side] = event.x > 0 ? 90.0f : 270.0f;
  f.attackDeg[1 - side] = event.x > 0 ? 270.0f : 90.0f;
  // ponytail: the scenes the builder 0x140477BC0 lists around the picked one are not built (27 functions of closure); the
  // radius sums over the scene GF plays.
  f.cuts = {sceneId};
  // ponytail: PES seeds the DemoInfo generator (A0+0x108) from its clock where the DemoInfo is built (0x14047DF00); GF lends
  // it the goal pick's own generator state, unadvanced.
  f.rngState = goalRng_->State(pes::kRange0Slot);
  return fixdemo_->PlaceGoal(f);
}

int GfMatch::PesSlot(Player* p) const {
  const auto at = std::find(players_.begin(), players_.end(), p);
  if (!p || at == players_.end()) return -1;
  const size_t index = size_t(at - players_.begin());
  const int side = teamOf_[index];
  int place = 0;  // his index among his side's players
  for (size_t k = 0; k < index; ++k) place += teamOf_[k] == side;
  return side * pes::designation::kSideSlots + place;
}

// ponytail: the anime record's +0x54 (what the goal gate's tail compares with 13.5, [0x1425A1FFC]) is written by PES's per-tick
// movement writer, which is not decoded. Measured on 3549 man-frames of a live goal (live_f.jsonl): it follows the man's ground
// speed at about 3.5-3.9 times its value in m/s (a km/h-like unit), so GF feeds 3.6 x speed; removed with the writer.
constexpr float kAnimeSpeedPerMps = 3.6f;
// ponytail: the fouled man's anime record +8 bit 10 (read by the foul gate's else arm) has no writer decoded; false until the
// live `Fo.R8` rows (live_timeline_poll.py) say which state sets it.

bool GfMatch::RunGate() {
  if (!fixdemo_) return false;
  const auto at = [](auto* p) {
    const Vector3 v = ToFox(p->GetPosition());
    return std::array<float, 3>{v.x, v.y, v.z};
  };
  if (gateIsGoal_) {
    if (!gateScorer_) return false;
    pes::GoalGateFacts g;
    g.pos = at(gateScorer_);
    const Vector3 dir = ToFox(gateScorer_->GetBodyDirectionVec());
    g.facing = float(Math::fposmod(Math::rad_to_deg(Math::atan2(dir.x, dir.z)), 360.0));  // PES's bearing: atan2(dx, dz)
    g.animeClass = uint8_t(AnimeClassOf(gateScorer_));
    // Ground speed from the last tick's displacement (the pose taken before the step): Player::GetMovement() is not a world velocity
    // (measured 160..1141 on a man walking at 2 m/s).
    const size_t index = size_t(std::find(players_.begin(), players_.end(), gateScorer_) - players_.begin());
    const Vector3 stepped = index < previousPlayers_.size() ? ToFox(gateScorer_->GetPosition()) - ToFox(previousPlayers_[index]) : Vector3();
    const Vector3 v = stepped / TICK_S;  // m/s, Y up: the ground speed is x and z
    g.animeSpeed = kAnimeSpeedPerMps * float(Math::sqrt(double(v.x) * v.x + double(v.z) * v.z));
    return fixdemo_->GoalGateReady(g);
  }
  pes::FoulGateFacts f;
  if (gateFouler_) f.foulerPos = at(gateFouler_);
  if (gateVictim_) f.victimPos = at(gateVictim_);
  if (PlayerOfficial* ref = match_->GetOfficials()->GetReferee()) f.refereePos = at(ref);
  return fixdemo_->FoulGateReady(f);
}

Dictionary GfMatch::take_cutscene() {
  Dictionary out;
  if (!pendingCutscene_) return out;
  Array steps;
  for (const pes::FixDemoStepRequest& step : pendingCutscene_->steps) {
    Dictionary d;
    d["cut"] = int(step.cutId);
    d["row"] = int(step.row);
    PackedStringArray fdc;
    for (const std::string& name : step.fdc) fdc.push_back(String(name.c_str()));
    d["fdc"] = fdc;
    steps.push_back(d);
  }
  out["scene"] = int(pendingCutscene_->sceneId);
  out["category"] = int(pendingCutscene_->sceneId >> 16);
  out["steps"] = steps;
  if (pendingAnchor_) {
    Dictionary anchor;
    anchor["pos"] = Vector3(pendingAnchor_->pos[0], pendingAnchor_->pos[1], pendingAnchor_->pos[2]);
    anchor["rot_y"] = double(pendingAnchor_->rotYDeg);
    anchor["flip"] = pendingAnchor_->flip;
    out["anchor"] = anchor;
  }
  pendingCutscene_.reset();
  pendingAnchor_.reset();
  return out;
}

// ponytail: the frames between PES's goal pick and the DemoInfo that runs the scene: four logged goals show the DemoInfo
// (and its manager) alive two frames after the pick frame (the first poll row with a scene id). The creating call (the
// DemoInfo builder 0x14047DF00 behind the pick's caller) is not decoded, so the lag is the measured one; removed with it.
constexpr uint32_t kPickToDemoInfoTicks = 2;

// The scene's timeline from its request: every step's player makes its length, start and speed from the first Cut record of
// the variant's layers (cut start 0x141F2DD80, pes::fdc::StepFromCut); the DemoInfo's state machine and the manager step
// them (pes::CutsceneTimeline).
void GfMatch::StartTimeline(const pes::FixDemoRequest& request) {
  const pes::fdc::PoseConstants* k = library_.is_valid() ? library_->cutscene_camera_constants() : nullptr;
  if (!k) return;
  const int category = int(request.sceneId >> 16);
  std::vector<pes::TimelineStep> steps;
  for (const pes::FixDemoStepRequest& step : request.steps) {
    pes::fdc::File cuts;  // the player's cut map: the Cut records of every layer
    for (const std::string& name : step.fdc) {
      const std::string path = pes::FixDemoCutDataPath(category, name);
      std::string err;
      auto bytes = path.empty() ? std::nullopt : library_->vfs().Read(path, &err);
      pes::fdc::File layer;
      if (bytes && pes::fdc::Parse(*bytes, &layer, &err)) cuts.cuts.insert(cuts.cuts.end(), layer.cuts.begin(), layer.cuts.end());
    }
    // ponytail: PES's player of a step whose layers hold no Cut record keeps length -1 and never finishes; the manager's
    // 3600-frame wait (0x141F59380 case 6, [0x142598590]) ends the scene. GF gives such a step the shortest length (1 tick)
    // so the list still runs out; no installed variant is without a Cut record.
    // ponytail: the raw length draw 0x141F7A150(scene+0x128, 100) of a Cut with min < max is not exposed by FixDemoSelector
    // (its Random1000 is the arena's); 0 gives the minimum. Every Cut of the goal scenes captured so far has min == max.
    const pes::fdc::CutRecord* first = pes::fdc::FirstCut(cuts);
    steps.push_back(first ? pes::fdc::StepFromCut(*first, 0, *k) : pes::TimelineStep{1.0f, 0.0f, 1.0f});
  }
  if (steps.empty()) {  // a request with no step at all plays nothing
    fixdemo_->EndScene(request.sceneId);
    pendingCutscene_.reset();
    pendingAnchor_.reset();
    return;
  }
  pes::TimelineConfig config;
  config.sixty = k->sixty;
  config.fps = pes::offball::kPesFps;
  gateOpen_ = false;
  timeline_.emplace(config, std::move(steps));
  timelineScene_ = request.sceneId;
  timelineSixty_ = k->sixty;
  clockPrevTicks_ = 0;
  // The pick step has run this far: DemoInfo exists kPickToDemoInfoTicks frames after it, the timeline's first Tick the next.
  timelineStartTick_ = tick_ + 1 + kPickToDemoInfoTicks + 1;
}

Dictionary GfMatch::cutscene_state() const {
  Dictionary out;
  if (!timeline_) return out;
  const pes::TimelineState& s = timeline_->State();
  out["scene"] = int(timelineScene_);
  out["step"] = s.stepPhase >= 5 ? s.step : -1;
  out["step_started"] = s.stepStarted;
  out["clock_prev_s"] = double(clockPrevTicks_ / timelineSixty_);
  out["clock_s"] = double(s.clock / timelineSixty_);
  out["ended"] = s.ended;
  return out;
}

bool GfMatch::is_over() const { return !match_ || match_->IsGameOver(); }
int GfMatch::player_count() const { return int(players_.size()); }

Vector3 GfMatch::player_position(int i) const {
  return i >= 0 && i < player_count() ? ToFox(players_[i]->GetPosition()) : Vector3();
}

Vector3 GfMatch::player_facing(int i) const {
  return i >= 0 && i < player_count() ? ToFox(players_[i]->GetBodyDirectionVec()).normalized() : Vector3();
}

Vector3 GfMatch::player_velocity(int i) const {
  // GetMovement() is the displacement per tick.
  return i >= 0 && i < player_count() ? ToFox(players_[i]->GetMovement()) / TICK_S : Vector3();
}

int GfMatch::player_team(int i) const { return i >= 0 && i < player_count() ? teamOf_[i] : -1; }

int GfMatch::player_database_id(int i) const {
  return i >= 0 && i < player_count() ? players_[i]->GetPlayerData()->GetDatabaseID() : -1;
}

Vector3 GfMatch::ball_position() const { return match_ ? ToFox(match_->GetBall()->Predict(0)) : Vector3(); }
int GfMatch::match_time_ms() const { return match_ ? int(match_->GetMatchTime_ms()) : 0; }
int GfMatch::score(int team) const { return match_ && (team == 0 || team == 1) ? match_->GetScore(team) : 0; }

double GfMatch::field_half_width() const { return pesFieldHalfWidth; }
double GfMatch::field_half_length() const { return pesFieldHalfLength; }

}  // namespace godot
