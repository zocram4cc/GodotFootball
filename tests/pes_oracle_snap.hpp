// A captured call's oracle directory as the PES-derived oracle tests read it (PES-derived, never committed): mem.bin (the
// capture), maps.txt (its VA runs), ok_delta.bin + pages.bin (the call-time delta pages over the capture), corpus.jsonl (the
// harness's reference row: rax and the call's write log), manifest.tsv (the call's live-in set) and the entry registers.
// Made by tools/dev/translate/trap_oracle.py and danger_oracle.py. Header-only, one include per test binary.
#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "pes_state.hpp"

namespace pes_oracle {

constexpr uint64_t kPage = 4096;

struct Oracle {
  SnapView snap;
  int pagesFd = -1;
  std::vector<uint64_t> deltas;
  std::string row;  // corpus.jsonl's one reference row
  std::map<std::string, uint64_t> regs;
  std::vector<std::vector<std::string>> manifest;
};

inline std::vector<uint8_t> Slurp(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

inline bool Load(const std::string& dir, Oracle& o) {
  o.snap.fd = open((dir + "/mem.bin").c_str(), O_RDONLY);
  o.pagesFd = open((dir + "/pages.bin").c_str(), O_RDONLY);
  if (o.snap.fd < 0 || o.pagesFd < 0) return false;
  FILE* mp = fopen((dir + "/maps.txt").c_str(), "r");
  if (mp == nullptr) return false;
  unsigned long long rva, rsize, roff;
  while (fscanf(mp, "%llx %llx %llx", &rva, &rsize, &roff) == 3) o.snap.runs.push_back({rva, rsize, roff});
  fclose(mp);
  const std::vector<uint8_t> delta = Slurp(dir + "/ok_delta.bin");
  o.deltas.resize(delta.size() / sizeof(uint64_t));
  std::memcpy(o.deltas.data(), delta.data(), o.deltas.size() * sizeof(uint64_t));
  std::ifstream corpus(dir + "/corpus.jsonl");
  std::getline(corpus, o.row);
  std::ifstream manifest(dir + "/manifest.tsv");
  for (std::string line; std::getline(manifest, line);) {
    std::vector<std::string> cols;
    std::stringstream ss(line);
    for (std::string col; std::getline(ss, col, '\t');) cols.push_back(col);
    if (cols.empty() || cols[0][0] == '#') continue;
    if (cols[0] == "R") o.regs[cols[1]] = std::strtoull(cols[2].c_str(), nullptr, 16);
    o.manifest.push_back(cols);
  }
  return !o.row.empty() && !o.manifest.empty();
}

inline uint64_t Read(const Oracle& o, uint64_t va, unsigned n) {
  uint64_t v = 0;
  for (unsigned i = 0; i < n; ++i) {
    const uint8_t* b = o.snap.at(va + i);
    if (b == nullptr) return 0;
    v |= static_cast<uint64_t>(*b) << (8 * i);
  }
  return v;
}

// The call's delta pages over the snapshot (the case's memory as the capture saw it at entry).
inline void SetOverlay(Oracle& o) {
  o.snap.over.clear();
  size_t pos = 0;
  const uint64_t np = o.deltas.at(pos++);
  for (uint64_t j = 0; j < np; ++j, pos += 2) {
    SnapView::Block b{o.deltas.at(pos), std::vector<uint8_t>(kPage)};
    if (pread(o.pagesFd, b.bytes.data(), kPage, static_cast<off_t>(o.deltas.at(pos + 1))) != static_cast<ssize_t>(kPage)) return;
    o.snap.over.push_back(std::move(b));
  }
  std::sort(o.snap.over.begin(), o.snap.over.end(),
            [](const SnapView::Block& a, const SnapView::Block& b) { return a.va < b.va; });
}

inline std::string Field(const std::string& row, const char* key) {
  const std::string pat = std::string("\"") + key + "\": \"";
  const size_t at = row.find(pat);
  if (at == std::string::npos) return {};
  const size_t from = at + pat.size();
  return row.substr(from, row.find('"', from) - from);
}

}  // namespace pes_oracle
