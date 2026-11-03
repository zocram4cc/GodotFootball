// The movestep test hosts' ClipTicks: the clip-record reader contract (src/pes/movestep.hpp MoveHost::ClipTicks) computed
// from a decoded MotionRecord. The translated readers themselves are proven bit-exact on live calls and tested on the install
// table in pes_clip_readers_test; these hosts only need the same answer for a synthetic record.
#pragma once

#include "pes/mbinfo.hpp"

namespace pes {

// int(2 v / rate * (tps / 60) + 0.5); a rate that is not positive is 1.0; a missing record reads as zero words.
inline int FakeClipTicks(const MotionRecord& rec, ClipReader reader, float rate, int index, float ticksPerSecond) {
  int value = 0;
  switch (reader) {
    case ClipReader::kBlend: value = rec.BlendStart(); break;
    case ClipReader::kCancel: value = rec.CancelFrames(index); break;
    case ClipReader::kRotStart: value = rec.Rot(index).start; break;
    case ClipReader::kRotEnd: value = rec.Rot(index).end; break;
  }
  if (rate <= 0.0f) rate = 1.0f;
  const float v = static_cast<float>(value);
  return static_cast<int>((v + v) / rate * (ticksPerSecond / 60.0f) + 0.5f);
}

}  // namespace pes
