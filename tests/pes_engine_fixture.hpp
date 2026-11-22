// PesEngineFixture: the memory a captured call may read, and the check that it reads nothing outside it.
//
// A call's windows come from readset.py --windows (one TSV per capture): every access of the call's replayed read and write set,
// tagged with its source and clustered into [lo, hi) windows. The sources the engine maps for a trap call are the image, the match
// (the engine's window and the capture's heap), the thread block, the stack, and PesMemory's OS arena, where the engine's allocator
// puts a player's chain objects. A read that finds no region returns zero silently; ReadMissLog records each one, so a test fails
// naming the address instead (the round-11 bug: the arena was not lent to the call, and every chain field read as zero).
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <istream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "pes_state.hpp"

namespace pes_engine_fixture {

struct Window {
  std::string source;       // "image", "match", "teb", "stack" or "os_arena" (readset.py classify)
  uint64_t lo = 0, hi = 0;  // [lo, hi)
  uint64_t distinct = 0;    // distinct accesses the window covers
};

struct CallWindows {
  std::string label, hook;
  uint64_t rsp = 0, teb = 0;
  std::vector<Window> windows;
};

inline std::vector<std::string> SplitTabs(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  for (const char ch : line) {
    if (ch == '\t') {
      out.push_back(cur);
      cur.clear();
    } else {
      cur += ch;
    }
  }
  out.push_back(cur);
  return out;
}

// The windows file: `call LABEL HOOK ERR RSP TEB` starts a call (hex RSP and TEB); `window SOURCE LO HI DISTINCT` (hex LO, HI) belongs to
// the call before it. Throws std::runtime_error on a malformed line.
inline std::vector<CallWindows> ReadWindows(std::istream& in) {
  std::vector<CallWindows> calls;
  for (std::string line; std::getline(in, line);) {
    if (line.empty()) continue;
    const std::vector<std::string> f = SplitTabs(line);
    if (f[0] == "call") {
      if (f.size() != 6) throw std::runtime_error("windows: a call line needs 6 fields: " + line);
      CallWindows c;
      c.label = f[1];
      c.hook = f[2];
      c.rsp = std::strtoull(f[4].c_str(), nullptr, 16);
      c.teb = std::strtoull(f[5].c_str(), nullptr, 16);
      calls.push_back(std::move(c));
    } else if (f[0] == "window") {
      if (f.size() != 5) throw std::runtime_error("windows: a window line needs 5 fields: " + line);
      if (calls.empty()) throw std::runtime_error("windows: a window before any call line");
      Window w;
      w.source = f[1];
      w.lo = std::strtoull(f[2].c_str(), nullptr, 16);
      w.hi = std::strtoull(f[3].c_str(), nullptr, 16);
      w.distinct = std::strtoull(f[4].c_str(), nullptr, 10);
      calls.back().windows.push_back(std::move(w));
    } else {
      throw std::runtime_error("windows: unknown line: " + line);
    }
  }
  return calls;
}

// The sources the fixture maps. `osArena` false is the mutation: PesMemory's OS arena is not lent to the call.
struct Standard {
  bool osArena = true;
};

inline bool Mapped(const Standard& s, const std::string& source) {
  if (source == "os_arena") return s.osArena;
  return source == "image" || source == "match" || source == "teb" || source == "stack";
}

// The first window of `call` whose source is not mapped, or null.
inline const Window* FirstUnmapped(const CallWindows& call, const Standard& s) {
  for (const Window& w : call.windows)
    if (!Mapped(s, w.source)) return &w;
  return nullptr;
}

// The failure text: the call, the source, and the address the replay reads first in that window.
inline std::string Describe(const CallWindows& call, const Window& w) {
  char addr[32];
  std::snprintf(addr, sizeof addr, "0x%llx", static_cast<unsigned long long>(w.lo));
  return "call " + call.label + " (" + call.hook + "): the replay reads " + addr + " (" + w.source +
         ", window ending " + std::to_string(w.hi) + ") outside every mapped window";
}

// Logs every PesMem read that finds no region while it is alive: (addr, n) in the order the call made them. One at a time.
class ReadMissLog {
 public:
  ReadMissLog() {
    active_ = this;
    PesMem::read_miss = &ReadMissLog::Record;
  }
  ~ReadMissLog() {
    PesMem::read_miss = nullptr;
    active_ = nullptr;
  }
  ReadMissLog(const ReadMissLog&) = delete;
  ReadMissLog& operator=(const ReadMissLog&) = delete;
  const std::vector<std::pair<uint64_t, unsigned>>& Misses() const { return misses_; }

 private:
  static void Record(uint64_t addr, unsigned n) { active_->misses_.push_back({addr, n}); }
  static inline ReadMissLog* active_ = nullptr;
  std::vector<std::pair<uint64_t, unsigned>> misses_;
};

}  // namespace pes_engine_fixture
