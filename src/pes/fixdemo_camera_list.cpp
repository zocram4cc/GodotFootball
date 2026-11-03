#include "pes/fixdemo_camera_list.hpp"

#include <algorithm>
#include <cstdio>
#include <initializer_list>

namespace pes {
namespace {

// {VA, expected opcode bytes, immediate offset}: the opcode is checked before the immediate is trusted.
struct Imm {
  uint64_t va;
  std::initializer_list<uint8_t> opcode;
  int offset;
};
constexpr Imm PICK_SCALE{0x141f7a112, {0xba}, 1};                      // mov edx, 0x2ac
constexpr Imm PICK_ADD{0x141f7a123, {0x41, 0x81, 0xc0}, 3};            // add r8d, 0xaa
constexpr Imm PICK_MASK{0x141f7a12d, {0x25}, 1};                       // and eax, 0x3ff
constexpr Imm PICK_SHIFT{0x141f7a13d, {0xc1, 0xe8}, 2};                // shr eax, 0xa
constexpr Imm WORD_INIT{0x141f5521e, {0xba}, 1};                       // mov edx, 0x400 (scene constructor)
constexpr Imm SEED_RANGE{0x141f552b9, {0xba}, 1};                      // mov edx, 0x4e20
constexpr Imm SET_RANGE{0x141f5d308, {0xba}, 1};                       // mov edx, 0x64 (the set word writer)
constexpr Imm WARMUP_DRAWS{0x141f52025, {0xbf}, 1};                    // mov edi, 0xb
constexpr Imm WARMUP_RANGE{0x141f52030, {0xba}, 1};                    // mov edx, 0x10

template <typename T>
bool ReadImm(const PeImage& exe, const Imm& imm, T* out, std::string* error) {
  uint8_t code[8] = {};
  if (!exe.ReadVa(imm.va, imm.opcode.size(), code) || !std::equal(imm.opcode.begin(), imm.opcode.end(), code) ||
      !exe.ReadVa(imm.va + imm.offset, sizeof *out, reinterpret_cast<uint8_t*>(out))) {
    char va[32];
    std::snprintf(va, sizeof va, "%#llx", static_cast<unsigned long long>(imm.va));
    *error = std::string("fixdemo random constant: unexpected instruction at ") + va;
    return false;
  }
  return true;
}

}  // namespace

std::optional<FixDemoRandomConstants> LoadFixDemoRandomConstants(const PeImage& exe, std::string* error) {
  auto rng = LoadRngConstants(exe, error);
  if (!rng) return std::nullopt;
  FixDemoRandomConstants k{};
  k.rng = *rng;
  uint8_t shift = 0;
  if (!ReadImm(exe, PICK_SCALE, &k.pickScale, error) || !ReadImm(exe, PICK_ADD, &k.pickAdd, error) ||
      !ReadImm(exe, PICK_MASK, &k.pickMask, error) || !ReadImm(exe, PICK_SHIFT, &shift, error) ||
      !ReadImm(exe, WORD_INIT, &k.wordInit, error) || !ReadImm(exe, SEED_RANGE, &k.seedRange, error) ||
      !ReadImm(exe, SET_RANGE, &k.setRange, error) || !ReadImm(exe, WARMUP_DRAWS, &k.warmupDraws, error) ||
      !ReadImm(exe, WARMUP_RANGE, &k.warmupRange, error))
    return std::nullopt;
  k.pickShift = shift;
  return k;
}

FixDemoGenerator::FixDemoGenerator(const FixDemoRandomConstants& k, uint32_t slot2State) : k_(k), rng_(k.rng), word_(0) {
  rng_.SetSeed(kSlot, slot2State);
  word_ = static_cast<uint32_t>(rng_.RangeRaw(kSlot, k.wordInit));
}

FixDemoGenerator::FixDemoGenerator(const FixDemoRandomConstants& k, uint32_t slot2State, uint32_t word)
    : k_(k), rng_(k.rng), word_(word) {
  rng_.SetSeed(kSlot, slot2State);
}

FixDemoGenerator FixDemoGenerator::Restore(const FixDemoRandomConstants& k, uint32_t slot2State, uint32_t word) {
  return FixDemoGenerator(k, slot2State, word);
}

uint32_t FixDemoGenerator::Pick(uint32_t n) {
  const uint32_t draw = static_cast<uint32_t>(rng_.RangeRaw(kSlot, k_.pickScale));
  word_ = (draw + word_ + static_cast<uint32_t>(k_.pickAdd)) & k_.pickMask;
  return (word_ * n) >> k_.pickShift;
}

namespace {
// 0x141F51FD0: the global generator's seed is the argument (1 for 0) plus one; its word and 11 warm-up draws follow.
FixDemoGenerator MakeGlobal(const FixDemoRandomConstants& k, uint32_t seed) {
  FixDemoGenerator g(k, (seed ? seed : 1u) + 1u);
  for (int i = 0; i < k.warmupDraws; ++i) g.Raw(k.warmupRange);
  return g;
}
}  // namespace

// The constructor 0x141F55110 in order: A takes the global generator's draw + 1; R1 and R2 take A's next two draws + 1; R1's
// first two picks are the scene's set word (+0x1A0) and the previous-pick cell (+0x1A4).
FixDemoScene::FixDemoScene(const FixDemoRandomConstants& k, uint32_t globalSeed)
    : a_([&] {
        FixDemoGenerator g = MakeGlobal(k, globalSeed);
        return FixDemoGenerator(k, static_cast<uint32_t>(g.Raw(k.seedRange)) + 1u);
      }()),
      r1_(k, static_cast<uint32_t>(a_.Raw(k.seedRange)) + 1u),
      r2_(k, static_cast<uint32_t>(a_.Raw(k.seedRange)) + 1u) {
  sceneWord_ = r1_.Pick(k.setRange);
  previousPick_ = r1_.Pick(k.setRange);
  setRange_ = k.setRange;
}

uint32_t FixDemoScene::LayerSetWord(uint32_t mode, uint32_t setCount) {
  uint32_t word = r1_.Pick(setRange_);
  if (mode == 1) {
    word = sceneWord_;
  } else if (mode == 2) {
    if (previousPick_ < setCount && setCount >= 2) {
      const uint32_t m = word % (setCount - 1);
      word = m < previousPick_ ? m : m + 1;
    } else {
      word %= setCount;
    }
    previousPick_ = word;
  }
  return word;
}

uint32_t CameraSetCount(const std::vector<fdc::CameraRecord>& records) {
  bool tensZero[10] = {};
  for (const fdc::CameraRecord& r : records) {
    const uint32_t set = r.key / 100;
    if (set < 10 && (r.key % 100) / 10 == 0) tensZero[set] = true;
  }
  uint32_t n = 0;
  while (n < 10 && tensZero[n]) ++n;
  return n;
}

std::vector<size_t> BuildCameraList(const std::vector<fdc::CameraRecord>& records, uint32_t setWord, FixDemoGenerator& r2) {
  std::vector<size_t> list;
  const uint32_t sets = CameraSetCount(records);
  if (sets == 0) return list;
  const uint32_t h = setWord % sets;
  std::vector<size_t> byKey(records.size());
  for (size_t i = 0; i < byKey.size(); ++i) byKey[i] = i;
  std::stable_sort(byKey.begin(), byKey.end(), [&](size_t a, size_t b) { return records[a].key < records[b].key; });  // std::map order
  for (uint32_t tens = 0; tens < 10; ++tens) {
    std::vector<size_t> group;
    for (size_t i : byKey)
      if (records[i].key / 100 == h && (records[i].key % 100) / 10 == tens) group.push_back(i);
    if (group.empty()) continue;
    const size_t pick = group[r2.Pick(static_cast<uint32_t>(group.size()))];
    if (list.size() < 2 || records[pick].startTime > records[list.back()].startTime) list.push_back(pick);
  }
  return list;
}

size_t CameraListIndex(const std::vector<fdc::CameraRecord>& records, const std::vector<size_t>& list, float clock) {
  size_t i = 0;
  while (i + 1 < list.size() && clock >= records[list[i + 1]].startTime) ++i;
  return i;
}

}  // namespace pes
