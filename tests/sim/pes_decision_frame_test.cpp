// PES's AI frame builder FUN_1409F1F20 on the window's own objects (PesWorld::RunFrameBuilder), against 14 REAL calls (live2 fbF captures:
// kick-off and open play). The file GF_PES_FBUILDER_STATES names (PES-derived runtime memory, kept out of the repo like every other oracle's
// captures; tools/dev/translate/fbuilder_states.py regenerates it from the live2 captures) holds, per call, every byte of PES's state the replay read before writing
// it (the live-in set, measured with a read log on the snapshot replay) and the frame F the real call left. The first test loads ALL of it:
// the window's builder over PES's real state must reproduce F bit-for-bit. The later tests take the objects from the window's own writers
// one at a time (see their comments), each held to the same frames.
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "onthepitch/pesworldrecord.hpp"

namespace {

struct ReadRun {
  std::string object;
  std::size_t offset;
  std::vector<uint8_t> bytes;
};
struct Call {
  std::string name;
  int slot = 0;
  uint32_t out[12] = {};
  std::vector<uint8_t> full;  // F after the call, 0x880 bytes
  std::vector<ReadRun> runs;
};

std::vector<Call> LoadCalls(const char* path) {
  std::vector<Call> calls;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ls(line);
    std::string head;
    ls >> head;
    if (head == "ROW") {
      Call c;
      std::string seq, slotWord;
      ls >> c.name >> seq >> slotWord >> c.slot;
      c.name += "/" + seq;
      calls.push_back(c);
    } else if (head == "FULL") {
      std::string hex;
      ls >> hex;
      for (std::size_t i = 0; i + 1 < hex.size(); i += 2) calls.back().full.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    } else if (head == "OUT") {
      for (uint32_t& w : calls.back().out) {
        std::string hex;
        ls >> hex;
        w = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
      }
    } else {
      ReadRun r;
      r.object = head;
      std::string off, hex;
      ls >> off >> hex;
      r.offset = std::stoull(off, nullptr, 16);
      for (std::size_t i = 0; i + 1 < hex.size(); i += 2) r.bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
      calls.back().runs.push_back(std::move(r));
    }
  }
  return calls;
}

// The destination of a fixture object in the window, or null when the window has no home for it yet.
uint8_t* Home(pes::PesWorld& w, const std::string& object, int slot) {
  const int side = slot / pes::kSlotsPerSide;
  const std::size_t hash = object.find('#');
  if (hash != std::string::npos) {  // another slot's record
    const std::string kind = object.substr(0, hash);
    const int other = std::atoi(object.c_str() + hash + 1);
    if (kind == "human") return w.AnimeRecord(other);
    if (kind == "man") return w.ManRecord(other);
    if (kind == "ai") return w.SlotRecord(other);
    return nullptr;
  }
  if (object == "T") return w.DecisionTeam(slot);
  if (object == "Q") return w.DecisionPlan(slot);
  if (object == "human") return w.AnimeRecord(slot);
  if (object == "man") return w.ManRecord(slot);
  if (object == "ai") return w.SlotRecord(slot);
  if (object == "MI") return w.MatchInfo();
  if (object == "pitch") return w.PitchRecord();
  if (object == "ball") return w.BallRecord();
  if (object == "P") return w.P(slot);
  if (object == "ring") return w.EventBytes();
  if (object == "obj18") return w.Ma(side);
  if (object == "rec1928") return w.SideRecord(0);
  if (object == "rec1950") return w.SideRecord(1);
  return nullptr;
}

constexpr std::size_t kOutOffsets[12] = {0, 4, 0x5C, 0x5E4, 0x14, 0x18, 0x1C, 0x2C, 0x30, 0x34, 0x54, 0x58};

// Every word of F the real call left that differs from the window's (diagnostic: unwritten words hold the caller's stack garbage in the real call).
std::string FullDiff(const pes::PesWorld& world, const Call& c) {
  const uint8_t* f = world.TickFrameConst();
  std::ostringstream o;
  for (std::size_t off = 0; off + 4 <= c.full.size(); off += 4) {
    uint32_t a, b;
    std::memcpy(&a, f + off, 4);
    std::memcpy(&b, c.full.data() + off, 4);
    if (a != b) o << " +0x" << std::hex << off << ":" << a << "/" << b;
  }
  return o.str();
}

int Mismatches(const pes::PesWorld& world, const Call& c) {
  const uint8_t* f = world.TickFrameConst();
  int bad = 0;
  for (int k = 0; k < 12; ++k) {
    uint32_t v;
    std::memcpy(&v, f + kOutOffsets[k], 4);
    if (v != c.out[k]) {
      ++bad;
      ADD_FAILURE() << c.name << " F+0x" << std::hex << kOutOffsets[k] << ": got " << v << " want " << c.out[k];
    }
  }
  return bad;
}

// The running game's memory dump (PES_EXE), mapped as a match maps it: the image's own tables (the weight table FUN_140A756C0 indexes) are read from it.
struct Image {
  uint8_t* map = nullptr;
  uint64_t bytes = 0;
  Image() {
    const char* exe = std::getenv("PES_EXE");
    if (exe == nullptr || *exe == '\0') return;
    struct stat st;
    const int fd = ::open(exe, O_RDONLY);
    if (fd < 0 || ::fstat(fd, &st) != 0) return;
    void* m = ::mmap(nullptr, static_cast<std::size_t>(st.st_size), PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    ::close(fd);
    if (m == MAP_FAILED) return;
    map = static_cast<uint8_t*>(m);
    bytes = static_cast<uint64_t>(st.st_size);
  }
  ~Image() {
    if (map != nullptr) ::munmap(map, bytes);
  }
};

// One call's world: every object the call read, loaded as the real call held it, except what `skip` names (the callers then supply it).
struct Loaded {
  pes::PesWorld world;
  pes::PesTrapExternals ext;
  uint8_t aiFlag = 0;
};

void LoadCall(Loaded& l, const Call& c, const Image& image) {
  pes::PesWorld& world = l.world;
  for (int k = 0; k < pes::kSlotCount; ++k) world.BindSlot(k);
  l.ext.image = image.map;
  l.ext.imageBase = pes::kImageBaseVa;
  l.ext.imageBytes = image.bytes;
  const int side = c.slot / pes::kSlotsPerSide;
  uint8_t* p = world.P(c.slot);
  const uint64_t ma = pes::PesWorld::MaVa(side), events = pes::PesWorld::EventsVa();
  std::memcpy(p + 0x18, &ma, 8);
  std::memcpy(p + 0xE0, &events, 8);
  // G's tables the window has no object for yet: each gets an allocator block its header slot names.
  const std::map<std::string, std::pair<std::size_t, std::size_t>> kHeapObjects = {{"G180", {0x180, 0xB00}}, {"G3b0", {0x3B0, 0x100}}, {"rec1978", {0x1978, 0xA300}}};
  std::map<std::string, uint8_t*> heapHome;
  for (const auto& [name, slotAndBytes] : kHeapObjects) {
    const uint64_t va = world.HeapAlloc(slotAndBytes.second, 16);
    std::memcpy(world.Bytes() + slotAndBytes.first, &va, 8);
    heapHome[name] = world.HeapHost(va, slotAndBytes.second);
  }
  for (const ReadRun& r : c.runs) {
    // The builder's stack arguments sit just below F (F - 0x40: Q, the param_6 vector, then the seventh argument, the AI object's byte at +0x95C).
    if (r.object == "va" && r.bytes.size() == 17) l.aiFlag = r.bytes[16];
    if (r.object == "va") {  // any other address: lent as the call's own bytes
      pes::PesTrapSegment seg;
      seg.va = r.offset;
      seg.bytes = r.bytes;
      l.ext.segments.push_back(std::move(seg));
      continue;
    }
    uint8_t* home = heapHome.count(r.object) ? heapHome[r.object] : Home(world, r.object, c.slot);
    if (home == nullptr) continue;
    for (std::size_t k = 0; k < r.bytes.size(); ++k) {
      const std::size_t off = r.offset + k;
      home[off] = r.bytes[k];
    }
  }
  // The plan's man words are the man record's (RunFrameBuilder copies the facing and the position in): the record's facing is not in the read set,
  // the plan's own copy is, so the record takes it.
  uint8_t* man = world.ManRecord(c.slot);
  std::memcpy(man + pes::kManFacingOffset, world.DecisionPlan(c.slot) + pes::kPlanFacingOffset, 4);
}

TEST(PesFrameBuilder, OverPesRealStateItReproducesTheRealFrame) {
  Image image;
  if (image.map == nullptr) GTEST_SKIP() << "PES_EXE unset (the running game's memory dump, not committed)";
  const char* states = std::getenv("GF_PES_FBUILDER_STATES");
  if (states == nullptr || *states == '\0') GTEST_SKIP() << "GF_PES_FBUILDER_STATES unset (PES-derived captures, not committed: tools/dev/translate/fbuilder_states.py)";
  const std::vector<Call> calls = LoadCalls(states);
  ASSERT_FALSE(calls.empty());
  for (const Call& c : calls) {
    Loaded l;
    LoadCall(l, c, image);
    l.world.RunFrameBuilder(c.slot, l.ext, l.aiFlag);
    EXPECT_EQ(Mismatches(l.world, c), 0) << c.name << " full-frame word diffs (window/real):" << FullDiff(l.world, c);
  }
}

// The dribbler: GF's AI command maps to PES's decision as the seam does it (a plan whose row holds the command's speed, the run-to point at the ball,
// the dribble's order kind and mode); the builder's frame for the man must carry what the real dribble tick received on 12 REAL dribble-tick entries
// (live2 dribbletick_1407B37D0, GF_PES_DRIBBLE_FRAMES): the order kind, the mode, the speed class and the weight, and the run-to point.
// Only the decision's own fields are held to the real frames; the world around the man (match state, side records) is the window's, as in a match.
TEST(PesFrameBuilder, TheDribblersFrameBuiltFromTheAiCommandCarriesTheRealOrder) {
  const char* path = std::getenv("GF_PES_DRIBBLE_FRAMES");
  if (path == nullptr || *path == '\0') GTEST_SKIP() << "GF_PES_DRIBBLE_FRAMES unset (PES-derived captures, not committed: tools/dev/translate/dribble_frames.py)";
  Image image;
  if (image.map == nullptr) GTEST_SKIP() << "PES_EXE unset (the running game's memory dump, not committed)";
  std::ifstream in(path);
  std::string line;
  int frames = 0;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string name;
    ls >> name;
    uint32_t w[14];
    for (uint32_t& v : w) {
      std::string hex;
      ls >> hex;
      v = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
    }
    ++frames;
    const int slot = static_cast<int>(w[0]);
    pes::PesWorld world;
    for (int k = 0; k < pes::kSlotCount; ++k) world.BindSlot(k);
    pes::PesTrapExternals ext;
    ext.image = image.map;
    ext.imageBase = pes::kImageBaseVa;
    ext.imageBytes = image.bytes;
    uint8_t* man = world.ManRecord(slot);
    std::memcpy(man + pes::kManPosXOffset, &w[1], 4);
    std::memcpy(man + pes::kManPosXOffset + 8, &w[2], 4);
    const uint32_t realClass = w[9];
    pes::AiFrameDecision d;
    d.active = true;
    d.speedKmh = pes::kSpeedClassBandsKmh[realClass - 1];  // the command's speed: the band's entry speed names the class
    d.kind = w[6];
    d.mode = w[7];
    std::memcpy(d.target, &w[3], sizeof d.target);  // the ball's sample 0
    world.RunDecisionFrame(slot, d, ext);
    const uint8_t* f = world.TickFrameConst();
    auto word = [&](std::size_t off) { uint32_t v; std::memcpy(&v, f + off, 4); return v; };
    EXPECT_EQ(word(0), w[6]) << name << " order kind";
    EXPECT_EQ(word(4), w[7]) << name << " mode";
    EXPECT_EQ(word(0x5C), w[9]) << name << " speed class";
    EXPECT_EQ(word(0x5E4), w[10]) << name << " weight";
  }
  EXPECT_GT(frames, 0);
}

}  // namespace
