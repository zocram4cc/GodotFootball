// PES 2021's player database (common/etc/pesdb/Player.bin): the injury tier and the stronger foot read from the
// record bytes at the accessor offsets (docs/re/pes-re-matchlogic.md 1.1-1.2).
#include <gtest/gtest.h>

#include <bit>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/playerdb.hpp"
#include "pes/vfs.hpp"

namespace {

using Blob = std::vector<uint8_t>;

// The offsets the tests pin from the docs, as literals: a changed constant in the header must fail here.
constexpr size_t kRecordBytes = 0x138;  // 312 bytes per record (matchlogic 1.1)
constexpr size_t kIdAt = 8;             // the record's u32 id
constexpr size_t kInjuryBit = 458;      // Injury Resistance, accessor 0x2E (matchlogic 1.2)
constexpr size_t kInjuryBits = 2;
constexpr size_t kFootBit = 514;        // Stronger Foot, accessor 0x0F (matchlogic 1.2)
constexpr size_t kFormBit = 438;        // Form, accessor 0x2D (matchlogic 1.2)
constexpr size_t kFormBits = 3;

// Synthetic player ids: the tests need records, not a PES roster.
constexpr int kFirstId = 9001;
constexpr int kSecondId = 9002;

// Writes `value` into `width` bits from absolute bit `bit` of a record, LSB-first.
void SetBits(Blob& rec, size_t bit, size_t width, uint32_t value) {
  for (size_t i = 0; i < width; ++i) {
    const size_t b = bit + i;
    const uint8_t mask = uint8_t(1u << (b % 8));
    if ((value >> i) & 1u) rec[b / 8] |= mask;
    else rec[b / 8] &= uint8_t(~mask);
  }
}

// One zeroed Player.bin record with the given id (u32 at +8).
Blob MakeRecord(uint32_t id) {
  Blob rec(kRecordBytes, 0);
  std::memcpy(&rec[kIdAt], &id, 4);
  return rec;
}

Blob Concat(const std::vector<Blob>& records) {
  Blob out;
  for (const Blob& r : records) out.insert(out.end(), r.begin(), r.end());
  return out;
}

// The install test mounts the user's PES 2021 once, as pes_stadium_test does.
const pes::PesVfs* Vfs() {
  static pes::PesVfs vfs;
  static bool ready = [] {
    const char* root = std::getenv("PES21_ROOT");
    if (!root) return false;
    std::string err;
    return vfs.Mount(root, &err);
  }();
  return ready ? &vfs : nullptr;
}

TEST(PlayerDatabaseParse, ReadsTheInjuryTierAndStrongerFootOfEachRecord) {
  Blob a = MakeRecord(kFirstId);
  SetBits(a, kInjuryBit, kInjuryBits, 2);  // A
  SetBits(a, kFootBit, 1, 1);              // left
  SetBits(a, kFormBit, kFormBits, 5);      // raw Form 5 (UI 6)
  Blob b = MakeRecord(kSecondId);
  SetBits(b, kInjuryBit, kInjuryBits, 0);  // C
  SetBits(b, kFormBit, kFormBits, 1);      // raw Form 1 (UI 2)
  const Blob file = Concat({a, b});

  std::string err;
  auto db = pes::PlayerDatabase::Parse(file, &err);
  ASSERT_TRUE(db.has_value()) << err;
  EXPECT_EQ(db->Size(), 2u);
  auto first = db->Find(kFirstId);
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->injuryResistance, 2);
  EXPECT_TRUE(first->leftFooted);
  EXPECT_EQ(first->form, 5);
  auto second = db->Find(kSecondId);
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->injuryResistance, 0);
  EXPECT_FALSE(second->leftFooted);
  EXPECT_EQ(second->form, 1);
  EXPECT_FALSE(db->Find(5).has_value());
}

TEST(PlayerDatabaseParse, FormIsTheThreeBitFieldAtBit438AndNothingBeside) {
  // Raw 6 at bits 438..440, with the neighbours 437 and 441 set: a reader that is off by one bit
  // sees a different Form.
  Blob rec = MakeRecord(13);
  SetBits(rec, kFormBit, kFormBits, 6);
  SetBits(rec, kFormBit - 1, 1, 1);
  SetBits(rec, kFormBit + kFormBits, 1, 1);
  std::string err;
  auto db = pes::PlayerDatabase::Parse(rec, &err);
  ASSERT_TRUE(db.has_value()) << err;
  auto traits = db->Find(13);
  ASSERT_TRUE(traits.has_value());
  EXPECT_EQ(traits->form, 6);
}

TEST(PlayerDatabaseParse, ConditionTakesTheDefaultPathUntilTheMixerLands) {
  // ponytail: the WROW weights at 0x142B47B10 are not read yet, so every record takes FUN_1414E2120's
  // 0x46 default path. This test pins the interim behaviour and goes with the mixer port.
  Blob rec = MakeRecord(17);
  SetBits(rec, kFormBit, kFormBits, 7);  // Form at its max must not move the condition byte
  std::string err;
  auto db = pes::PlayerDatabase::Parse(rec, &err);
  ASSERT_TRUE(db.has_value()) << err;
  auto traits = db->Find(17);
  ASSERT_TRUE(traits.has_value());
  EXPECT_EQ(traits->condition, 0x46);
}

TEST(PlayerDatabaseParse, InjuryTierIsTheTwoBitFieldAtBit458AndNothingBeside) {
  // Raw 1 (B) at bits 458..459, with the neighbours 457 and 460 set: a reader that is off by one bit
  // sees a different tier.
  Blob rec = MakeRecord(7);
  SetBits(rec, kInjuryBit, kInjuryBits, 1);
  SetBits(rec, kInjuryBit - 1, 1, 1);
  SetBits(rec, kInjuryBit + kInjuryBits, 1, 1);
  std::string err;
  auto db = pes::PlayerDatabase::Parse(rec, &err);
  ASSERT_TRUE(db.has_value()) << err;
  auto traits = db->Find(7);
  ASSERT_TRUE(traits.has_value());
  EXPECT_EQ(traits->injuryResistance, 1);
}

TEST(PlayerDatabaseParse, StrongerFootIsTheOneBitAtBit514AndNothingBeside) {
  Blob rec = MakeRecord(9);
  SetBits(rec, kFootBit - 1, 1, 1);  // 513 set: the foot must still read right
  SetBits(rec, kFootBit + 1, 1, 1);  // 515 set: likewise
  std::string err;
  auto right = pes::PlayerDatabase::Parse(rec, &err);
  ASSERT_TRUE(right.has_value()) << err;
  ASSERT_TRUE(right->Find(9).has_value());
  EXPECT_FALSE(right->Find(9)->leftFooted);

  SetBits(rec, kFootBit, 1, 1);  // now 514 set: left
  auto left = pes::PlayerDatabase::Parse(rec, &err);
  ASSERT_TRUE(left.has_value()) << err;
  ASSERT_TRUE(left->Find(9).has_value());
  EXPECT_TRUE(left->Find(9)->leftFooted);
}

TEST(PlayerDatabaseParse, InjuryCodeThreeIsNoTierAndHasNoTraits) {
  // The accessor's range is 0..2 (C, B, A); code 3 names no tier, so the player takes the caller's fallback.
  Blob rec = MakeRecord(11);
  SetBits(rec, kInjuryBit, kInjuryBits, 3);
  std::string err;
  auto db = pes::PlayerDatabase::Parse(rec, &err);
  ASSERT_TRUE(db.has_value()) << err;
  EXPECT_FALSE(db->Find(11).has_value());
}

TEST(PlayerDatabaseParse, RejectsABlobThatIsNotWholeRecords) {
  Blob file = MakeRecord(3);
  file.push_back(0);
  std::string err;
  EXPECT_FALSE(pes::PlayerDatabase::Parse(file, &err).has_value());
  EXPECT_FALSE(err.empty());
}

// The install's own file: every record's traits are recomputed from its bytes at the documented offsets. Pinning
// the values would pin a mod's data (the 4cc placeholders are flat), so the test asserts the relation.
TEST(PlayerDatabaseInstall, EveryRecordReadsBackFromItsOwnBytes) {
  const pes::PesVfs* vfs = Vfs();
  if (!vfs) GTEST_SKIP() << "PES21_ROOT not set or mount failed";
  std::string err;
  auto blob = vfs->Read(pes::kPlayerDatabasePath, &err);
  ASSERT_TRUE(blob.has_value()) << err;
  ASSERT_EQ(blob->size() % kRecordBytes, 0u);
  auto db = pes::PlayerDatabase::Parse(*blob, &err);
  ASSERT_TRUE(db.has_value()) << err;
  ASSERT_GT(db->Size(), 0u);

  size_t checked = 0;
  for (size_t at = 0; at + kRecordBytes <= blob->size() && checked < 64; at += kRecordBytes) {
    const uint8_t* rec = blob->data() + at;
    uint32_t id = 0;
    std::memcpy(&id, rec + kIdAt, 4);
    const uint32_t tier = (rec[kInjuryBit / 8] >> (kInjuryBit % 8)) & 3u;
    const uint32_t foot = (rec[kFootBit / 8] >> (kFootBit % 8)) & 1u;
    const uint32_t form = ((rec[kFormBit / 8] >> (kFormBit % 8)) | (rec[kFormBit / 8 + 1] << 2)) & 7u;
    auto traits = db->Find(int(id));
    if (tier > 2) {
      EXPECT_FALSE(traits.has_value()) << "id " << id;
    } else {
      ASSERT_TRUE(traits.has_value()) << "id " << id;
      EXPECT_EQ(traits->injuryResistance, int(tier)) << "id " << id;
      EXPECT_EQ(traits->leftFooted, foot == 1) << "id " << id;
      EXPECT_EQ(traits->form, int(form)) << "id " << id;
    }
    ++checked;
  }
  EXPECT_GT(checked, 0u);
}

// Speed (Player.bin id 0x1C), Jump (0x1F), Height (0x10) and the stronger foot (0x0F) are the bytes the slot records start from
// (block 0x27, 0x2C, 0x31, 0x34). Offsets pinned from the docs as literals: a changed header constant must fail here.
constexpr size_t kSpeedAt = 306, kJumpAt = 294, kAbilityWidth = 6, kHeightAt = 216, kHeightWidth = 8;

TEST(PlayerDatabaseParse, ReadsSpeedJumpAndHeightWithTheirBiasesAndNothingBeside) {
  Blob rec = MakeRecord(21);
  SetBits(rec, kSpeedAt, kAbilityWidth, 17);   // 57
  SetBits(rec, kJumpAt, kAbilityWidth, 33);    // 73
  SetBits(rec, kHeightAt, kHeightWidth, 85);   // 185 cm
  // The bits beside each field belong to other fields: set them, a reader off by one bit sees a different value.
  SetBits(rec, kSpeedAt - 1, 1, 1);
  SetBits(rec, kSpeedAt + kAbilityWidth, 1, 1);
  SetBits(rec, kJumpAt + kAbilityWidth, 1, 1);
  SetBits(rec, kHeightAt - 1, 1, 1);
  SetBits(rec, kHeightAt + kHeightWidth, 1, 1);
  std::string err;
  auto db = pes::PlayerDatabase::Parse(rec, &err);
  ASSERT_TRUE(db.has_value()) << err;
  auto traits = db->Find(21);
  ASSERT_TRUE(traits.has_value());
  EXPECT_EQ(traits->speed, 57);
  EXPECT_EQ(traits->jump, 73);
  EXPECT_EQ(traits->heightCm, 185);
}

// The header's bits are the exe's own accessors: each leaf is `test edx,edx; jnz; mov eax,[rcx+d]; shr eax,s; and eax,m; add eax,b`
// (or `movzx eax,byte [rcx+d]; add al,b`), decoded here from PES2021.exe and compared with the constants.
struct Accessor {
  size_t bit = 0, width = 0;
  int bias = 0;
};
bool DecodeAccessor(const uint8_t* b, Accessor* out) {
  if (b[0] != 0x85 || b[1] != 0xD2 || b[2] != 0x75) return false;  // test edx,edx ; jnz: mode 0 is the value path
  const uint8_t* p = b + 4;
  if (p[0] == 0x8B && p[1] == 0x41 && p[3] == 0xC1 && p[4] == 0xE8 && p[6] == 0x83 && p[7] == 0xE0) {
    out->bit = size_t(p[2]) * 8 + p[5];
    out->width = size_t(std::popcount(unsigned(p[8])));
    p += 9;
    out->bias = 0;
    if (p[0] == 0x83 && p[1] == 0xC0) {
      out->bias = p[2];
      p += 3;
    }
    return p[0] == 0xC3;
  }
  if (p[0] == 0x0F && p[1] == 0xB6 && p[2] == 0x41 && p[4] == 0x04) {
    out->bit = size_t(p[3]) * 8;
    out->width = 8;
    out->bias = p[5];
    return p[6] == 0xC3;
  }
  return false;
}

TEST(PlayerDatabaseInstall, AbilityOffsetsEqualTheExesAccessors) {
  const char* root = std::getenv("PES21_ROOT");
  if (root == nullptr) GTEST_SKIP() << "PES21_ROOT unset (the accessors are read from its exe)";
  std::string error;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &error);
  ASSERT_TRUE(exe) << error;
  struct Want {
    uint64_t va;
    size_t bit, width;
    int bias;
    const char* name;
  };
  for (const Want& w : {Want{0x141F0F7C0, pes::kSpeedBit, pes::kAbilityBits, pes::kAbilityBias, "Speed"},
                        Want{0x141F0F760, pes::kJumpBit, pes::kAbilityBits, pes::kAbilityBias, "Jump"},
                        Want{0x141F0F590, pes::kHeightBit, pes::kHeightBits, pes::kHeightBiasCm, "Height"},
                        Want{0x141F103A0, pes::kStrongerFootBit, 1, 0, "Stronger foot"}}) {
    uint8_t code[24];
    ASSERT_TRUE(exe->ReadVa(w.va, sizeof code, code)) << w.name;
    Accessor got;
    ASSERT_TRUE(DecodeAccessor(code, &got)) << w.name << " is not the expected accessor shape";
    EXPECT_EQ(got.bit, w.bit) << w.name;
    EXPECT_EQ(got.width, w.width) << w.name;
    EXPECT_EQ(got.bias, w.bias) << w.name;
  }
}

}  // namespace
