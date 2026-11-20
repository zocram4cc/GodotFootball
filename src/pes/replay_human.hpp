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
#include <vector>
#include <string>

namespace pes {

class PeImage;

constexpr uint32_t kPaletteEntryBytes = 0xA0;

struct ReplayFrameInputs {
  uint8_t active = 1;       // R+0x18: the record is live
  uint32_t human = 0;       // R+0x28: the human's index in the frame (the stride is 300 bytes per human)
  uint32_t guard = 0;       // the frame's word at +0x30C for this human: 0x7FFF marks the human absent
  uint8_t paletteIndex = 0; // the frame's palette byte at +0x36B for this human (0..0x26 are valid)
  uint16_t frameIndex = 0;  // the low word of the frame key (the byte-pair at R8): the unit frame (stride 0x1FB0) the palette byte is read from
  std::array<uint8_t, kPaletteEntryBytes> entry{};  // the palette entry the index selects (unit palette +0x80)
};

struct ReplayHumanRecord {
  std::array<uint8_t, kPaletteEntryBytes> bytes{};  // R+0x50..+0xF0: the copied appearance entry
  uint32_t word1c = 0;                              // R+0x1C
  uint8_t byte21 = 0;                               // R+0x21
};
// One 4 KB page of the frame's pre-state beyond the install, as a capture holds it at the frame's entry (the copy tests read
// the capture's pages; the frame reads them before it writes them).
struct PreStatePage {
  uint64_t va = 0;
  std::vector<uint8_t> bytes;  // 4096 bytes
};
// One allocator answer of the frame's capture: the outermost allocation the frame made (its requested size) and the block
// the allocator handed out (PES's address). The frame answers its allocations with these in order, so the blocks keep
// their contents as PES left them (a recycled block carries its previous content, which the copy reads).
struct AllocAnswer {
  uint64_t size = 0;
  uint64_t va = 0;
};

class ReplayHumanFrame {
 public:
  // The install supplies the constants and globals the translated code reads. Returns null and sets error
  // when the image does not map them. pre is the frame's pre-state beyond the install (a capture's pages, see
  // PreStatePage); it is mapped after the frame's own windows, which keep precedence. answers are the capture's outermost
  // allocator answers (AllocAnswer). record is the frame's record as PES holds it (its first 0x1000 bytes, from the
  // record's address): it seeds the record window (sub-objects the frame reads, such as the one at +0x148, are not written
  // by the frame), and Read's inputs overwrite the fields it sets.
  static std::unique_ptr<ReplayHumanFrame> Create(const PeImage& exe, std::string* error,
                                                  const std::vector<PreStatePage>& pre = {},
                                                  const std::vector<AllocAnswer>& answers = {},
                                                  const std::vector<uint8_t>& record = {});
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
