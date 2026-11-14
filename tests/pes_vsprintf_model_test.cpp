// The GF model of ucrtbase's __stdio_common_vsprintf_s (pes_stubs.hpp, pes_model_vsprintf_s) against the formats PES's
// code gives it: the DemoInfoRef name "PLAYER_%s%02d" (FUN_1406EB250 formats the player's name with it), %d with a negative
// value, and %%. Every expected string is written out here from the format; the model is not called with a placeholder.
// No install or image is needed: the model runs on a flat state built in the test.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "pes_state.hpp"
#include "pes_stubs.hpp"

namespace {

constexpr uint64_t kFormatVa = 0x3000000000ULL, kArgVa = 0x3000001000ULL, kBufVa = 0x3000002000ULL;
constexpr uint64_t kSlotsVa = 0x3000003000ULL, kStackVa = 0x3000010000ULL;
constexpr std::size_t kBufBytes = 64;

// The model's result: the text the buffer holds (up to its NUL) and the character count the model returns in rax.
struct Formatted {
  std::string text;
  uint64_t count = 0;
};

// Runs the model on `format`. argIsString[i] says whether the i-th conversion takes a string (taken from `strings` in order) or
// an integer (taken from `ints` in order). The va_list is one 8-byte slot per argument, as the model reads it.
Formatted FormatWithModel(const std::string& format, const std::vector<std::string>& strings,
                          const std::vector<int32_t>& ints, const std::vector<bool>& argIsString) {
  PesState s;
  std::vector<uint8_t> fmt(format.begin(), format.end());
  fmt.push_back(0);
  s.mem.segs.push_back(PesMem::Seg{kFormatVa, fmt});
  std::vector<uint8_t> strBytes;
  std::vector<uint64_t> slots;
  std::size_t si = 0, ii = 0;
  for (bool isString : argIsString) {
    if (isString) {
      const std::string& t = strings.at(si++);
      slots.push_back(kArgVa + strBytes.size());
      strBytes.insert(strBytes.end(), t.begin(), t.end());
      strBytes.push_back(0);
    } else {
      slots.push_back(static_cast<uint32_t>(ints.at(ii++)));  // a 4-byte argument sits in the low half of its slot
    }
  }
  if (strBytes.empty()) strBytes.resize(8, 0);
  s.mem.segs.push_back(PesMem::Seg{kArgVa, strBytes});
  std::vector<uint8_t> slotBytes(slots.size() * 8 + 8, 0);
  std::memcpy(slotBytes.data(), slots.data(), slots.size() * 8);
  s.mem.segs.push_back(PesMem::Seg{kSlotsVa, slotBytes});
  s.mem.segs.push_back(PesMem::Seg{kBufVa, std::vector<uint8_t>(kBufBytes, 0xAA)});
  s.mem.segs.push_back(PesMem::Seg{kStackVa - 0x100, std::vector<uint8_t>(0x200, 0)});
  s.wr(kStackVa + 0x30, kSlotsVa, 8);  // [rsp + 0x30] at the model's entry: the va_list pointer
  s.gpr[4] = kStackVa;                 // rsp at the model's entry
  s.gpr[2] = kBufVa;                   // rdx = buffer
  s.gpr[8] = kBufBytes;                // r8 = count
  s.gpr[9] = kFormatVa;                // r9 = format
  s.gpr[1] = 0;                        // rcx = options
  pes_model_vsprintf_s(s);
  Formatted out;
  out.count = s.gpr[0];
  for (std::size_t k = 0; k < kBufBytes; ++k) {
    const uint8_t c = static_cast<uint8_t>(s.rd(kBufVa + k, 1));
    if (c == 0) break;
    out.text.push_back(static_cast<char>(c));
  }
  return out;
}

}  // namespace

// PLAYER_%s%02d with the side's letter and the squad place: the name FUN_1406EB250 gives the player object.
TEST(PesVsprintfModel, PlayerNameFormatWritesTheSideAndTwoDigitPlace) {
  const Formatted f = FormatWithModel("PLAYER_%s%02d", {"A"}, {8}, {true, false});
  EXPECT_EQ(f.text, "PLAYER_A08");
  EXPECT_EQ(f.count, 10u);
}

TEST(PesVsprintfModel, TwoDigitZeroPaddingKeepsTwoDigitsForTheSecondSide) {
  const Formatted f = FormatWithModel("PLAYER_%s%02d", {"B"}, {19}, {true, false});
  EXPECT_EQ(f.text, "PLAYER_B19");
}

TEST(PesVsprintfModel, PercentPercentIsALiteralPercent) {
  const Formatted f = FormatWithModel("100%% %s", {"ok"}, {}, {true});
  EXPECT_EQ(f.text, "100% ok");
}

TEST(PesVsprintfModel, SignedDecimalPrintsItsMinus) {
  const Formatted f = FormatWithModel("%d", {}, {-5}, {false});
  EXPECT_EQ(f.text, "-5");
}
