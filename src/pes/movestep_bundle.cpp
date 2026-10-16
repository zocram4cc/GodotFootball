// PES 2021's MOVESTEP install bundle loader: every table the MOVESTEP
// handlers read, from the install, in one place. The Match owns the bundle;
// the sim adapter reads it from there, never from the process environment.
#include "pes/movestep_bundle.hpp"

namespace pes {

std::optional<MoveStepConstants> LoadMoveStepConstants(const PeImage& exe, const PesVfs& vfs,
                                                       std::string* error) {
  MoveStepConstants c;
  if (auto k = LoadRunConstants(exe, error))
    c.run = *k;
  else
    return std::nullopt;
  if (auto t = LoadRunTables(exe, error))
    c.runTables = *t;
  else
    return std::nullopt;
  if (auto s = LoadStepScalars(exe, error))
    c.stepScalars = *s;
  else
    return std::nullopt;
  if (auto t = LoadStepTables(exe, error))
    c.stepTables = *t;
  else
    return std::nullopt;
  if (auto p = LoadPlaybackConstants(exe, error))
    c.playback = *p;
  else
    return std::nullopt;
  if (auto t = LoadSpeedRateTable(exe, error))
    c.speedRate = *t;
  else
    return std::nullopt;
  if (auto m = LoadMoveConstants(exe, error))
    c.move = *m;
  else
    return std::nullopt;
  if (auto t = LoadIdleRowTables(exe, error))
    c.idleRows = *t;
  else
    return std::nullopt;
  if (auto b = LoadAngleBucketConstants(exe, error))
    c.buckets = *b;
  else
    return std::nullopt;
  if (auto k = LoadSpeedClassConstants(exe, error))
    c.speedClass = *k;
  else
    return std::nullopt;
  if (auto t = movestep::LoadMoveTurnConstants(exe, error))
    c.turn = *t;
  else
    return std::nullopt;
  if (auto m = movestep::LoadMotionStartConstants(exe, error))
    c.motionStart = *m;
  else
    return std::nullopt;
  if (auto s = LoadStyleConstants(exe, error))
    c.style = *s;
  else
    return std::nullopt;
  if (auto d = LoadDampingConstants(exe, error))
    c.damping = *d;
  else
    return std::nullopt;
  if (auto d = LoadDampingRecord(vfs, error))
    c.dampingRecord = *d;
  else
    return std::nullopt;
  if (auto g = LoadParametricGroups(exe, error))
    c.parametric = *g;
  else
    return std::nullopt;
  return c;
}

}  // namespace pes
