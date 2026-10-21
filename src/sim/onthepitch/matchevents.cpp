// The match's PES event stage (A2): the detectors of pesevents.hpp run on every
// tick over the GF state, and the records they write go into the match's event
// store. Tick placement and the open orders are in docs/re/pes-re-gaps-a2.md and
// tasks/15-10-26.md (A2).

#include <algorithm>

#include "onthepitch/match.hpp"

namespace {

// PES's ball-event family: the ball state the detector is fed each touch. GF's ball
// state word (G+0x40+0x291c) has no counterpart, so a touch is the in-play state.
constexpr int kBallStateInPlay = 1;

// PES's team roster: 11 starters per side carry the ids team*11 + index
// (pesevents.hpp ActorIdOf). GF's squad index is the store key's slot; the id is the
// starter ordinal, capped so a substitute keeps his side.
constexpr int kStartersPerSide = 11;

// PES's phase word (MI+0x1168) has no GF counterpart: play is phase 1, otherwise 0.
// ponytail: A6 (the period record, FUN_140a3c050) replaces this.
constexpr int kPhasePlay = 1;
constexpr int kPhaseStopped = 0;

// The state word FUN_140a3c050 reads (MI+0x1168 block). ponytail: GF has no period
// record yet (A6), so every record's state word is zero.
constexpr float kStateWord = 0.0f;

// The ball's next PES record, 1/54 s, floored to GF's millisecond grid.
constexpr int kNextRecordMs = 18;

// PES's K=10 and K=9 gate: MI+0x11b4 == 0 is open (C8 ruling, docs/re/pes-re-gaps-a2.md c).
// ponytail: GF has no closer for the byte (the live byte is 0 in every sample), so the
// gate is always open.
constexpr bool kGateOpen = true;

}  // namespace

float Match::GetPesEventAge_s(int eventFrame) const {
  return static_cast<float>(GetPesFrame() - eventFrame) / GetPesClockConstants().gameSpeed;
}

void Match::QueuePesTouch(Player* player, uint32_t actionKind) {
  pesPendingTouches.push_back(PendingPesTouch{player, actionKind, GetPesFrame()});
}

pes::ActorKey Match::PesActorKeyOf(const Player* player) {
  pes::ActorKey key;
  key.team = static_cast<uint32_t>(player->GetTeamID());
  const auto& squad = GetTeam(player->GetTeamID())->GetAllPlayers();
  for (size_t i = 0; i < squad.size(); ++i) {
    if (squad[i].get() == player) {
      key.slot = static_cast<uint32_t>(i);
      return key;
    }
  }
  return pes::NoActorKey();
}

Player* Match::GetPesActorPlayer(const pes::ActorKey& key) {
  if (!key.Valid()) return nullptr;
  const auto& squad = GetTeam(static_cast<int>(key.team))->GetAllPlayers();
  if (key.slot >= squad.size()) return nullptr;
  return squad[key.slot].get();
}

void Match::ProcessPesEventStage() {
  tickStages.push_back(TickStage::Events);
  const int frame = GetPesFrame();
  const int phase = IsInPlay() ? kPhasePlay : kPhaseStopped;
  const RefereeBuffer& buffer = GetRefereeBuffer();
  // PES's restart state: 3 while a restart is awarded (team.cpp's rule R), else 1 (play).
  // ponytail: PES's 2 (the foul stoppage) has no GF counterpart.
  const int restartState = buffer.active ? 3 : 1;
  const int restartTeam = buffer.active ? buffer.teamID : -1;
  const Vector3 ballNow = GetBall()->Predict(0);
  const Vector3 ballNext = GetBall()->Predict(kNextRecordMs);

  // K=10 (docs/re/pes-re-out-side.md): fires when the pitch-zone code changes. The code is 0 while the
  // restart is awarded (PES's p+0x298 == 3; the restart state 3 above), else the classifier of the ball.
  // The judge reads the event one tick later: the code leads the judge state by one frame.
  if (PesLoop()) {
    pesDetectors.outConsumable = pesDetectors.outRaised;
    pesDetectors.outRaised = false;
    pes::OutOfPlayInput outIn;
    outIn.code = buffer.active ? 0
                               : pes::OutOfPlayCode(ballNow.coords[0], ballNow.coords[1],
                                                    GetPesBallConstants().image.ballRadius);
    outIn.frame = frame;
    outIn.stored = pesDetectors.outCode;
    pes::EventRecord outRec;
    if (pes::DetectOutOfPlay(&outIn, &outRec)) {
      pesEvents.Route(pesEventRings, outRec);
      pesDetectors.outRaised = true;
    }
    pesDetectors.outCode = outIn.stored;
  }

  // The ball-event family: one record per queued touch, in touch order. Each record is
  // then followed by the snapshot detector (K=11), which reads the touch just routed.
  for (const PendingPesTouch& touch : pesPendingTouches) {
    const pes::ActorKey key = PesActorKeyOf(touch.player);
    pes::BallEventInput in;
    in.ballState = kBallStateInPlay;
    in.kickGate = true;  // ponytail: GF's ball state word has no counterpart; a touch is in play
    in.actionKind = static_cast<int>(touch.actionKind);
    in.lastToucher = static_cast<int>(key.team) * kStartersPerSide +
                     std::min(static_cast<int>(key.slot), kStartersPerSide - 1);
    in.lastToucherSlot = static_cast<int>(key.slot);
    in.frame = touch.frame;
    in.phase = phase;
    in.state = kStateWord;
    in.restartState = restartState;
    in.store = &pesEvents;
    in.constants = &pesEventConstants;
    pes::EventRecord touchRecord;
    if (!pes::DetectBallEvent(in, &touchRecord)) continue;
    pesEvents.Route(pesEventRings, touchRecord);

    pes::SnapshotInput snap;
    snap.store = &pesEvents;
    snap.current = &touchRecord;
    snap.frame = touch.frame;
    snap.phase = phase;
    snap.state = kStateWord;
    pes::EventRecord snapshot;
    if (pes::DetectSnapshot(snap, &snapshot)) pesEvents.Route(pesEventRings, snapshot);
  }
  pesPendingTouches.clear();

  // K=30, the foul ring. GF has no stoppage state 2, so its triggers do not fire.
  {
    pes::FoulInput in;
    in.frame = frame;
    in.phase = phase;
    in.state = kStateWord;
    in.restartState = restartState;
    in.stored = pesDetectors.foul;
    in.constants = &pesEventConstants;
    pes::EventRecord rec;
    if (pes::DetectFoul(&in, &rec)) pesEvents.Route(pesEventRings, rec);
    pesDetectors.foul = in.stored;
  }

  // K=1, the phase word changing.
  {
    pes::PhaseInput in;
    in.phase = phase;
    in.frame = frame;
    in.state = kStateWord;
    in.stored = pesDetectors.phase;
    pes::EventRecord rec;
    if (pes::DetectPhase(&in, &rec)) pesEvents.Route(pesEventRings, rec);
    pesDetectors.phase = in.stored;
  }

  // K=16, the restart word turning nonzero. ponytail: MI+0x11b2 has no GF counterpart;
  // the armed referee buffer stands in for it.
  {
    pes::RestartInput in;
    in.restartByte = buffer.active ? 1 : 0;
    in.frame = frame;
    in.phase = phase;
    in.state = kStateWord;
    in.stored = pesDetectors.restart;
    pes::EventRecord rec;
    if (pes::DetectRestart(&in, &rec)) pesEvents.Route(pesEventRings, rec);
    pesDetectors.restart = in.stored;
  }

  // K=12, team 0's possession latch flipping. The team tick evaluates the latch after
  // this stage, so a flip is seen on the tick after the latch changes (PES's soft edge).
  {
    pes::PossessionFlipInput in;
    in.frame = frame;
    in.phase = phase;
    in.state = kStateWord;
    in.team0InPossession = GetTeam(0)->IsPesPossession();
    in.ballX = ballNow.coords[0];  // ponytail: GF x as PES x (INF: FUN_140a57800's +0 word)
    in.ballZ = ballNow.coords[1];  // ponytail: GF y as PES z (INF: FUN_140a57800's +8 word)
    in.stored = pesDetectors.possessionFlip;
    pes::EventRecord rec;
    if (pes::DetectPossessionFlip(&in, &rec, pesEventConstants)) pesEvents.Route(pesEventRings, rec);
    pesDetectors.possessionFlip = in.stored;
  }

  // K=13, the possession sequence. ponytail: PES's direction word (FUN_140a48a30 +0x254)
  // and the team stoppage flag (+0xb6e0) have no GF counterpart: a fixed attacking
  // direction and no stoppage flag.
  {
    pes::PossessionSequenceInput in;
    in.frame = frame;
    in.phase = phase;
    in.state = kStateWord;
    in.restartState = restartState;
    in.ballDirX = 1;
    in.ballSample0 = ballNow.coords[0];
    in.zoneHalfX = pitchHalfW;
    in.zoneHalfZ = pitchHalfH;
    in.ballZ = ballNow.coords[1];
    in.teamFlag6e0 = false;
    in.store = &pesEvents;
    pes::EventRecord rec;
    if (pes::DetectPossessionSequence(&in, &pesDetectors.sequence, &rec))
      pesEvents.Route(pesEventRings, rec);
  }

  // K=9, the ball's second prediction sample at the deflection constant. ponytail: the
  // sample's +4 word is the record's height (INF: WriteClampedShorts writes a sample as
  // {x, y, z}, y at +4); GF's height is the ball prediction's z.
  {
    pes::DeflectionInput in;
    in.frame = frame;
    in.phase = phase;
    in.state = kStateWord;
    in.gateOpen = kGateOpen;
    in.sample0 = ballNow.coords[2];
    in.sample1 = ballNext.coords[2];
    in.sampleConst = pesEventConstants.deflectionSample;
    pes::EventRecord rec;
    if (pes::DetectDeflection(in, &rec)) pesEvents.Route(pesEventRings, rec);
  }
}
