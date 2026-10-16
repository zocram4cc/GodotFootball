#pragma once

// PES 2021's MOVESTEP install bundle: the constants the MOVESTEP handlers read, loaded once from
// the install by the caller (gfsim_run, PesLibrary, the test helper) and handed to the Match
// through MatchSetup, like every other PES table. The Match owns a copy; the sim adapter reads it
// from there, never from the process environment.

#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/locomotion.hpp"
#include "pes/movestep.hpp"
#include "pes/movestep_run.hpp"
#include "pes/movestep_turn.hpp"
#include "pes/vfs.hpp"

namespace pes {

// Every install constant the MOVESTEP handlers read through the MoveHost.
struct MoveStepConstants {
  RunConstants run{};
  RunTables runTables{};
  StepScalars stepScalars{};
  StepTables stepTables{};
  PlaybackConstants playback{};
  SpeedRateTable speedRate{};
  MoveConstants move{};
  IdleRowTables idleRows{};
  AngleBucketConstants buckets{};
  SpeedClassConstants speedClass{};
  movestep::MoveTurnConstants turn{};
  ParametricGroups parametric{};
  StyleConstants style{};
  movestep::MotionStartConstants motionStart{};
  DampingConstants damping{};
  DampingRecord dampingRecord{};
  // The parametric blend table (Parametricblendtable.bin): the members of each blend group, which the
  // humanoid plays for a parametric issue.
  std::vector<BlendMember> blend;
};

// Loads the whole bundle from the install. Nullopt when any part is unreadable (the caller
// reports which via `error`).
std::optional<MoveStepConstants> LoadMoveStepConstants(const PeImage& exe, const PesVfs& vfs,
                                                       std::string* error);

}  // namespace pes
