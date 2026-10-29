#include "onthepitch/pesimagetables.hpp"

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

}  // namespace

std::optional<PesImageTables> LoadPesImageTables(const PeImage& exe, std::string* error) {
  PesImageTables tables;
  const uint64_t classBytes = sizeof(uint32_t) * (kAnimeClassBack + kClassTableEntries);
  if (!Read(exe, kAnimeClassTableVa - sizeof(uint32_t) * kAnimeClassBack, classBytes, &tables, error) ||
      !Read(exe, kQuarterSineVa, sizeof(float) * kQuarterSineCount, &tables, error) ||
      !Read(exe, kFrameCellsVa, kFrameCellsBytes, &tables, error))
    return std::nullopt;
  uint8_t source[sizeof(float)];
  if (!exe.ReadVa(kFrameFloatSourceVa, sizeof source, source) ||
      std::memcmp(source, tables.segments.back().bytes.data() + sizeof(uint32_t), sizeof source) != 0) {
    if (error != nullptr) *error = "frame-rate cells differ from 0x14259D624, the float 0x141E532F0 copies into them";
    return std::nullopt;
  }
  return tables;
}

}  // namespace pes
