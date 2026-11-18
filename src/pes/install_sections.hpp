// The install's .rdata and .data bytes: the sections the translated PES code reads its constants and
// globals from. The VAs are the image's section bounds (the same ones the FixDemo camera rig maps).
#pragma once

#include <cstdint>
#include <vector>

namespace pes {

class PeImage;

constexpr uint64_t kRdataVa = 0x14252F000, kRdataBytes = 0xE6C000;
constexpr uint64_t kDataVa = 0x14339B000, kDataBytes = 0x72A000;

// Reads n bytes at va into out (zero-filled first), sector by sector: a chunk the image does not map stays
// zero. Returns the number of bytes the image maps.
uint64_t ReadSection(const PeImage& exe, uint64_t va, uint64_t n, std::vector<uint8_t>* out);

}  // namespace pes
