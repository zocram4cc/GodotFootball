#include "pes/install_sections.hpp"

#include <algorithm>

#include "pes/exe.hpp"

namespace pes {

namespace {
constexpr uint64_t kReadChunk = 0x10000;
}  // namespace

uint64_t ReadSection(const PeImage& exe, uint64_t va, uint64_t n, std::vector<uint8_t>* out) {
  out->assign(n, 0);
  uint64_t mapped = 0;
  for (uint64_t off = 0; off < n; off += kReadChunk) {
    const uint64_t len = std::min<uint64_t>(kReadChunk, n - off);
    if (exe.ReadVa(va + off, len, out->data() + off)) mapped += len;
  }
  return mapped;
}

}  // namespace pes
