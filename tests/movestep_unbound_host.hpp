// A pes::MoveHost whose every read stops loudly. A test host derives from it and overrides only what the handler under
// test is allowed to read, so a read the port makes that PES's call log does not contain fails the test.
#pragma once

#include <cstdio>
#include <cstdlib>

#include "pes/movestep.hpp"

namespace movestep_test {

class UnboundHost : public pes::MoveHost {
 public:
  [[noreturn]] static void Unbound(const char* what) {
    std::fprintf(stderr, "UnboundHost::%s was read\n", what);
    std::abort();
  }

  pes::MoveCell Cell(int, int, bool) override { Unbound("Cell"); }
  int PickN(int) override { Unbound("PickN"); }
  bool Percent(int) override { Unbound("Percent"); }
  int CommandSpeedClass() const override { Unbound("CommandSpeedClass"); }
  float RateCeiling() const override { Unbound("RateCeiling"); }
  float CommandSpeedKmh() const override { Unbound("CommandSpeedKmh"); }
  float CurrentSpeedKmh() const override { Unbound("CurrentSpeedKmh"); }
  float PhysiqueG() const override { Unbound("PhysiqueG"); }
  float TicksPerSecond() const override { Unbound("TicksPerSecond"); }
  int MatchTick() const override { Unbound("MatchTick"); }
  int RecentStamp(int) const override { Unbound("RecentStamp"); }
  float MotionStartTime(int) const override { Unbound("MotionStartTime"); }
  float RecordF2468() const override { Unbound("RecordF2468"); }
  int MoveStatePrevious() const override { Unbound("MoveStatePrevious"); }
  float RecordF0bec() const override { Unbound("RecordF0bec"); }
  void SetRecordF0bec(float) override { Unbound("SetRecordF0bec"); }
  const pes::AngleBucketConstants& AngleBuckets() const override { Unbound("AngleBuckets"); }
  const pes::StepScalars& StepConstants() const override { Unbound("StepConstants"); }
  bool MirrorChoice(bool) const override { Unbound("MirrorChoice"); }
  int FootBand() const override { Unbound("FootBand"); }
  const pes::RunConstants& RunConstantsView() const override { Unbound("RunConstantsView"); }
  const pes::RunTables& RunTablesView() const override { Unbound("RunTablesView"); }
  const pes::SpeedRateTable& SpeedRateTableView() const override { Unbound("SpeedRateTableView"); }
  const pes::movestep::MoveTurnConstants& MoveTurnConstantsView() const override { Unbound("MoveTurnConstantsView"); }
  const pes::StepTables& StepTablesView() const override { Unbound("StepTablesView"); }
  bool BucketIsLeft(int) const override { Unbound("BucketIsLeft"); }
  const pes::PlaybackConstants& PlaybackConstantsView() const override { Unbound("PlaybackConstantsView"); }
  int CurrentMotion() const override { Unbound("CurrentMotion"); }
  int ClipTicks(pes::ClipReader, int, float, int) const override { Unbound("ClipTicks"); }
  const pes::MotionRecord& Motion(int) const override { Unbound("Motion"); }
  bool ClipFlag() const override { Unbound("ClipFlag"); }
  bool FacingAdjusterBusy() const override { Unbound("FacingAdjusterBusy"); }
  int AbilityByte(int) const override { Unbound("AbilityByte"); }
  bool EntryAimFlag() const override { Unbound("EntryAimFlag"); }
  const pes::MoveConstants& MoveConstantsView() const override { Unbound("MoveConstantsView"); }
  const pes::SpeedClassConstants& SpeedClassConstantsView() const override { Unbound("SpeedClassConstantsView"); }
  const pes::IdleRowTables& IdleRowTablesView() const override { Unbound("IdleRowTablesView"); }
  float FacingDeg() const override { Unbound("FacingDeg"); }
  float BodyDeg() const override { Unbound("BodyDeg"); }
  float BallBearingDeg() const override { Unbound("BallBearingDeg"); }
  int ActionIndex() const override { Unbound("ActionIndex"); }
  int RelatedSlot() const override { Unbound("RelatedSlot"); }
  void RecordPosition(int, float[3]) const override { Unbound("RecordPosition"); }
  float RecordFacing(int) const override { Unbound("RecordFacing"); }
  int MatchState11d4() const override { Unbound("MatchState11d4"); }
  int MatchState1554() const override { Unbound("MatchState1554"); }
  int PhaseTick() const override { Unbound("PhaseTick"); }
  int MatchModeByte() const override { Unbound("MatchModeByte"); }
  bool MatchObject3b0() const override { Unbound("MatchObject3b0"); }
  bool TrainerFlag1423e7fd0() const override { Unbound("TrainerFlag1423e7fd0"); }
  int RecordConditionByte(int) const override { Unbound("RecordConditionByte"); }
  bool ActionSlotMarked(int) const override { Unbound("ActionSlotMarked"); }
  bool ActionTableOwns(int) const override { Unbound("ActionTableOwns"); }
  int RecordActionA() const override { Unbound("RecordActionA"); }
  int RecordActionB() const override { Unbound("RecordActionB"); }
  int CurrentClipFrames() const override { Unbound("CurrentClipFrames"); }
  int AnimeState() const override { Unbound("AnimeState"); }
  void PositionAt(int, float[3]) const override { Unbound("PositionAt"); }
  void RingSample(int, float[3]) const override { Unbound("RingSample"); }
  void OwnPosition(float[3]) const override { Unbound("OwnPosition"); }
  int MatchPhase() const override { Unbound("MatchPhase"); }
  bool IsReplay() const override { Unbound("IsReplay"); }
  const pes::StyleConstants& StyleConstantsView() const override { Unbound("StyleConstantsView"); }
  bool ScriptedSlotIsOurs() const override { Unbound("ScriptedSlotIsOurs"); }
  int StaminaPercent() const override { Unbound("StaminaPercent"); }
  float PlayerHeightCm() const override { Unbound("PlayerHeightCm"); }
  void BallSample(int, float[3]) const override { Unbound("BallSample"); }
  float BallMotionHeadingDeg() const override { Unbound("BallMotionHeadingDeg"); }
  int CurrentState() const override { Unbound("CurrentState"); }
  const pes::DampingConstants& DampingConstantsView() const override { Unbound("DampingConstantsView"); }
  const pes::DampingRecord& DampingRecordView() const override { Unbound("DampingRecordView"); }
  const pes::TeamAi& Team() const override { Unbound("Team"); }
  void Issue(const pes::MotionRequest&) override { Unbound("Issue"); }
};

}  // namespace movestep_test
