// Runs the penalty shootout during e_MatchPhase_Penalties: picks takers, aims each
// kick by PES's penalty scatter, narrates the outcome and decides the match. The
// verdict is what the ball does (ObserveKick). See TECHNICAL_ROADMAP.md section 3C.

#ifndef _HPP_PENALTY_SHOOTOUT_CONTROLLER
#define _HPP_PENALTY_SHOOTOUT_CONTROLLER

#include <string>

#include "base/math/vector3.hpp"
#include "penaltyshootout.hpp"

class Match;
class Player;

class PenaltyShootoutController {
public:
  PenaltyShootoutController(Match* match);

  // Sets up a fresh shootout. Safe to call repeatedly; only the first call for a
  // given shootout takes effect.
  void Start();
  void Reset();

  // Called once per match tick while the match is in the penalties phase.
  void Process();

  bool IsStarted() const { return started; }
  bool IsFinished() const { return state.phase == PenaltyShootout::e_Phase_Finished; }
  int GetWinner() const { return PenaltyShootout::GetWinner(state); }
  const PenaltyShootout::State& GetState() const { return state; }
  // Kicks converted so far, for the scoreboard.
  int GetScore(int teamID) const { return state.score[teamID == 1 ? 1 : 0]; }

  // Placement the current taker is going for, used by his controller so the
  // shot animation is aimed where his stats put it.
  bool IsKickLive() const { return state.phase == PenaltyShootout::e_Phase_Execution; }
  // The defending keeper of a live kick is driven by the penalty branch of his
  // controller (docs/re/pes-re-penalty-keeper.md 4), since the kick is a set piece.
  bool IsKeepingKick(int teamID) const { return IsKickLive() && state.shootingTeam != teamID; }
  float GetAimY() const { return aimY; }
  float GetAimPower() const { return aimPower; }
  // The kick's height aim (m). ponytail: no strike reads it yet. The taker's touch direction
  // is built with z = 0 in elizacontroller.cpp (shootout branch); that is where it goes.
  float GetAimHeight() const { return aimHeight; }
  // Every kick in the shootout is taken at the same end, as in a real shootout.
  float GetGoalX() const;

protected:
  Player* SelectTaker(int teamID);
  // Places ball, taker, keeper and the waiting players for the next kick.
  void SetUpKick();
  void BeginKick();
  // Puts the defending keeper on the line of the shootout goal.
  void AnchorKeeper();
  // Taker in the area, defending keeper in goal, everyone else on halfway.
  void PositionPlayers();
  PenaltyShootout::KickObservation ObserveBall();
  void FinishKick(PenaltyShootout::e_Outcome outcome);
  void AnnounceOutcome();
  std::string GetScoreLine() const;
  blunted::Vector3 GetPenaltySpot() const;
  blunted::Vector3 GetGoalCentre() const;

  Match* match;
  PenaltyShootout::State state;

  bool started;
  unsigned long nextEventTime_ms;
  // Which players have already taken a kick this round, per team.
  unsigned int takenMask[2];
  unsigned long kickStartTime_ms;
  // Aim of the current kick (goal-line lateral and height, metres) and its weight.
  float aimY;
  float aimHeight;
  float aimPower;
  // Match score at the start of the shootout; shootout kicks do not change it.
  int baseScore[2];
  bool keeperTouchedBall;
  // Whether the keeper has already been hauled back to this goal for this kick.
  bool keeperHauledBack;
  // Whether the taker has actually struck the ball yet. Until he has, the ball
  // sitting still on the spot must not be read as a kick that has finished.
  bool ballStruck;
  // Side of the pitch the shootout is held at (-1 or 1), drawn at the start.
  int shootoutEndSide;
  Player* currentTaker;
};

#endif
