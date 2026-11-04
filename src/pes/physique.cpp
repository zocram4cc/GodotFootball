#include "pes/physique.hpp"

#include <cstring>
#include <initializer_list>

namespace pes {
namespace {

constexpr uint64_t kImageBase = 0x140000000ULL;

// A rip-relative load: the instruction's bytes at `va` start with `opcode`, its disp32 sits `dispOffset` bytes in, and it is
// `length` bytes long (the target is relative to its end).
struct Site {
  uint64_t va;
  std::initializer_list<uint8_t> opcode;
  int dispOffset;
  int length;
  float* out;
};

bool Load(const PeImage& exe, const Site& s) {
  uint8_t got[16];
  const std::size_t n = s.opcode.size();
  if (n > sizeof got || !exe.ReadVa(s.va, n, got) || std::memcmp(got, s.opcode.begin(), n) != 0) return false;
  int32_t disp = 0;
  if (!exe.ReadVa(s.va + static_cast<uint64_t>(s.dispOffset), sizeof disp, reinterpret_cast<uint8_t*>(&disp))) return false;
  const uint64_t at = s.va + static_cast<uint64_t>(s.length) + static_cast<uint64_t>(static_cast<int64_t>(disp));
  return at >= kImageBase && exe.ReadVa(at, sizeof *s.out, reinterpret_cast<uint8_t*>(s.out));
}

constexpr std::initializer_list<uint8_t> kMovss1D{0xF3, 0x0F, 0x10, 0x1D}, kMovss05{0xF3, 0x0F, 0x10, 0x05},
    kMovss25{0xF3, 0x0F, 0x10, 0x25}, kMovss0D{0xF3, 0x0F, 0x10, 0x0D}, kMovss15{0xF3, 0x0F, 0x10, 0x15},
    kMovss3D{0xF3, 0x0F, 0x10, 0x3D}, kMovssXmm8{0xF3, 0x44, 0x0F, 0x10, 0x05}, kDivss0D{0xF3, 0x0F, 0x5E, 0x0D},
    kSubss0D{0xF3, 0x0F, 0x5C, 0x0D}, kMulss05{0xF3, 0x0F, 0x59, 0x05};

}  // namespace

std::optional<PhysiqueConstants> LoadPhysiqueConstants(const PeImage& exe, std::string* error) {
  PhysiqueConstants k;
  float oneAgain = 0.0f, oneSub = 0.0f;
  const Site sites[] = {
      // FUN_14195C4B0 (the height deviation)
      {0x14195C4B7, kMovss1D, 4, 8, &k.heightFloorCm},
      {0x14195C4D3, kMovss05, 4, 8, &k.heightCeilCm},
      {0x14195C4DB, kMovss25, 4, 8, &k.heightAnchorCm},
      {0x14195C4F5, kDivss0D, 4, 8, &k.lowerSpanCm},
      {0x14195C4FD, kSubss0D, 4, 8, &oneSub},
      {0x14195C50D, kDivss0D, 4, 8, &k.upperSpanCm},
      // FUN_14195CA70 (the block scale)
      {0x14195CA7F, kMovss05, 4, 8, &oneAgain},
      {0x14195CA9A, kMovss0D, 4, 8, &k.tallGain[1]},
      {0x14195CAA4, kMovss0D, 4, 8, &k.tallGain[0]},
      {0x14195CAAC, kMovss15, 4, 8, &k.one},
      {0x14195CAC6, kMulss05, 4, 8, &k.legGain[1]},
      {0x14195CADC, kMulss05, 4, 8, &k.legGain[0]},
      // FUN_141EB5810 (the root-bone clamp)
      {0x141EB588E, kMovss3D, 4, 8, &k.scaleHi},
      {0x141EB589C, kMovssXmm8, 5, 9, &k.scaleLo},
  };
  for (const Site& s : sites) {
    if (!Load(exe, s)) {
      if (error) *error = "PES2021.exe: the physique-scale constant loaded at VA " + std::to_string(s.va) + " failed its opcode check";
      return std::nullopt;
    }
  }
  if (oneAgain != k.one || oneSub != k.one) {  // the three loads of 1.0 name one constant
    if (error) *error = "PES2021.exe: the physique scale's three 1.0 loads disagree";
    return std::nullopt;
  }
  return k;
}

float HeightDeviation(const PhysiqueConstants& k, float heightCm) {
  // 0x14195C4C2..: x2 = (h - floor < 0) ? floor : h; (ceil - x2 < 0) or (anchor <= x2) takes the upper segment.
  const float x2 = (heightCm - k.heightFloorCm < 0.0f) ? k.heightFloorCm : heightCm;
  if (k.heightCeilCm - x2 < 0.0f || k.heightAnchorCm <= x2) return (heightCm - k.heightAnchorCm) / k.upperSpanCm;
  return (heightCm - k.heightFloorCm) / k.lowerSpanCm - k.one;
}

float BlockScale(const PhysiqueConstants& k, const PhysiqueBlock& b) {
  if (b.unscaled) return k.one;
  const float dev = HeightDeviation(k, b.heightCm);
  const float tall = k.tallGain[0.0f <= dev ? 0 : 1] * dev + k.one;
  const float leg = b.legSlider * k.legGain[0.0f <= b.legSlider ? 0 : 1] + k.one;
  return leg * tall;
}

float PhysiqueG(const PhysiqueConstants& k, const PhysiqueBlock& b) {
  const float s = BlockScale(k, b);
  // 0x141EB5956..: above hi -> hi, else max(lo, s). ponytail: PES's NaN answer (max(lo, NaN) = NaN) is not ported; a finite block never gives NaN.
  if (s > k.scaleHi) return k.scaleHi;
  return k.scaleLo > s ? k.scaleLo : s;
}

}  // namespace pes
