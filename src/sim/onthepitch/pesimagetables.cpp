#include "onthepitch/pesimagetables.hpp"

#include <algorithm>
#include <initializer_list>
#include <cstring>

namespace pes {

namespace {

bool Read(const PeImage& exe, uint64_t va, uint64_t bytes, PesImageTables* out, std::string* error) {
  PesTrapSegment seg;
  seg.va = va;
  seg.bytes.resize(bytes);
  if (!exe.ReadVa(va, bytes, seg.bytes.data())) {
    if (error != nullptr) *error = "image table unreadable at VA 0x" + std::to_string(va);
    return false;
  }
  out->segments.push_back(std::move(seg));
  return true;
}

// The 32-bit immediate of the `MOV dword ptr [..],imm32` at `va` (`opcode` first, then the immediate).
bool ReadStoreImmediate(const PeImage& exe, uint64_t va, std::initializer_list<uint8_t> opcode, uint32_t* imm) {
  uint8_t bytes[sizeof(uint32_t) + 4];
  const std::size_t n = opcode.size();
  if (!exe.ReadVa(va, n + sizeof(uint32_t), bytes) || !std::equal(opcode.begin(), opcode.end(), bytes)) return false;
  std::memcpy(imm, bytes + n, sizeof *imm);
  return true;
}

// The none record the event record's constructor leaves: zeros, kind word, word +0x10.
bool EventNoneRecord(const PeImage& exe, PesImageTables* out, std::string* error) {
  uint32_t kind = 0, word = 0;
  if (!ReadStoreImmediate(exe, kEventCtorKindStoreVa, {0xC7, 0x03}, &kind) ||
      !ReadStoreImmediate(exe, kEventCtorWordStoreVa, {0xC7, 0x43, kEventCtorWordOffset}, &word)) {
    if (error != nullptr) *error = "event record constructor 0x140A3CDD0 is not the one this table was found in";
    return false;
  }
  PesTrapSegment seg;
  seg.va = kEventNoneRecordVa;
  seg.bytes.assign(kEventNoneRecordBytes, 0);
  std::memcpy(seg.bytes.data(), &kind, sizeof kind);
  std::memcpy(seg.bytes.data() + kEventCtorWordOffset, &word, sizeof word);
  out->segments.push_back(std::move(seg));
  return true;
}

}  // namespace

std::optional<PesImageTables> LoadPesImageTables(const PeImage& exe, std::string* error) {
  PesImageTables tables;
  const uint64_t classBytes = sizeof(uint32_t) * (kAnimeClassBack + kClassTableEntries);
  constexpr std::size_t kFrameCellsSegment = 3;  // the fourth Read below
  if (!Read(exe, kAnimeClassTableVa - sizeof(uint32_t) * kAnimeClassBack, classBytes, &tables, error) ||
      !Read(exe, kQuarterSineVa, sizeof(float) * kQuarterSineCount, &tables, error) ||
      !Read(exe, kTierTableVa, kTierRows * kTierRowBytes, &tables, error) ||
      !Read(exe, kFrameCellsVa, kFrameCellsBytes, &tables, error) ||
      !EventNoneRecord(exe, &tables, error))
    return std::nullopt;
  uint8_t source[sizeof(float)];
  if (!exe.ReadVa(kFrameFloatSourceVa, sizeof source, source) ||
      std::memcmp(source, tables.segments[kFrameCellsSegment].bytes.data() + sizeof(uint32_t), sizeof source) != 0) {
    if (error != nullptr) *error = "frame-rate cells differ from 0x14259D624, the float 0x141E532F0 copies into them";
    return std::nullopt;
  }
  return tables;
}

}  // namespace pes
