// PES's clip-record readers, run as PES's own translated code over a motion table laid out as PES lays it
// (docs/re/pes-re-portmap-motion.md M2; proven bit-exact on live calls, tools/dev/translate/results_motion.json):
//   0x1406AF620  blend / fixed start   (Animation record, kind 0: u32[1] bits 10..19)
//   0x1406AF8E0  cancel frame          (CancelData record, kind 5: u32[0] bits 13..22)
//   0x1406B1F00  RotData start         (kind 6: u32[1] bits 10..19, minus 0x205)
//   0x1406B1D00  RotData end           (kind 6: u32[1] bits 0..9, plus 1)
// Each answers int(2 v / rate * (ticks per second / 60) + 0.5); a rate that is not positive is 1.0, a motion with no
// record at the index reads PES's zero-word fallback record.
//
// The table is `Mbinfo`'s raw words filed the way FUN_1406B20E0 allocates it: 0x1125 entries of 0x390 bytes, a count byte
// per kind at entry+kind and a list of record pointers at the kind's list offset (kind 0 +0x18, kind 5 +0xA0, kind 6 +0xC8).

#pragma once

#include <cstdint>
#include <vector>

#include "pes/mbinfo.hpp"

namespace pes {

// The table's geometry (FUN_1406B20E0: 0x1125 motions of 0x390 bytes; a motion at or past 0x1125 reads the default entry).
inline constexpr uint32_t kClipTableSlots = 0x1125;
inline constexpr uint64_t kClipTableEntryBytes = 0x390;
// An entry's count bytes are at +kind; each kind's list of 8-byte record pointers starts at its list offset (the readers'
// own displacements: 0x1406AF620 +0x18, 0x1406AF8E0 +0xA0, 0x1406B1D00 and 0x1406B1F00 +0xC8).
inline constexpr uint64_t kClipBlendKind = 0, kClipCancelKind = 5, kClipRotKind = 6;
inline constexpr uint64_t kClipBlendList = 0x18, kClipCancelList = 0xA0, kClipRotList = 0xC8;
// Where the translated code addresses the table (PesMem::window): above the world window and the trap loader data.
inline constexpr uint64_t kClipTableVa = 0x2200000000ULL;

class PesClipReaders {
 public:
  // Files every motion's raw words (Mbinfo::MotionRecord::blendWords / cancelWords / rotWords) into the table.
  explicit PesClipReaders(const Mbinfo& mbinfo);

  // One reader call on the table: the answer in ticks. `index` is the record index (ignored by kBlend, whose list holds one
  // record); `ticksPerSecond` is PES's frame-rate cell 0x14351AE54 (the sim's rate).
  int Read(ClipReader reader, int motion, float rate, int index, float ticksPerSecond) const;

 private:
  std::vector<uint8_t> table_;  // the table object, the entries, the default entry and the records, from kClipTableVa
};

}  // namespace pes
