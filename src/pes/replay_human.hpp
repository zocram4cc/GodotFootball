// PES's ReplayHuman frame read FUN_14217DEF0 (0x14217DEF0), run from the translated layer on PES-layout
// state. When a recorded frame has the human present and its palette entry differs from the one the record holds, the
// function copies the 0xA0-byte entry into the ReplayHuman record (+0x50) and releases the record's old state through
// its virtual slot +0x28 (FUN_14217DE20: frees the object at +0x48 when set, and the one at +0x118). The record persists
// between reads, as it does in PES. The object at +0x48 is not created by this read: the five real destructor-case
// captures hold it set at entry, and the port starts with it unset (the first copy then skips the +0x48 free; the
// +0x118 release and the virtual slot still run).
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace pes {

class PeImage;

constexpr uint32_t kPaletteEntryBytes = 0xA0;

struct ReplayFrameInputs {
  uint8_t active = 1;       // R+0x18: the record is live
  uint32_t human = 0;       // R+0x28: the human's index in the frame (the stride is 300 bytes per human)
  uint32_t guard = 0;       // the frame's word at +0x30C for this human: 0x7FFF marks the human absent
  uint8_t paletteIndex = 0; // the frame's palette byte at +0x36B for this human (0..0x26 are valid)
  std::array<uint8_t, kPaletteEntryBytes> entry{};  // the palette entry the index selects (unit palette +0x80)
};

struct ReplayHumanRecord {
  std::array<uint8_t, kPaletteEntryBytes> bytes{};  // R+0x50..+0xF0: the copied appearance entry
  uint32_t word1c = 0;                              // R+0x1C
  uint8_t byte21 = 0;                               // R+0x21
};

class ReplayHumanFrame {
 public:
  // The install supplies the constants and globals the translated code reads. Returns null and sets error
  // when the image does not map them.
  static std::unique_ptr<ReplayHumanFrame> Create(const PeImage& exe, std::string* error);
  ~ReplayHumanFrame();
  ReplayHumanFrame(const ReplayHumanFrame&) = delete;
  ReplayHumanFrame& operator=(const ReplayHumanFrame&) = delete;

  // One frame read for one human. The record persists across reads.
  ReplayHumanRecord Read(const ReplayFrameInputs& in);

 private:
  ReplayHumanFrame();
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace pes
