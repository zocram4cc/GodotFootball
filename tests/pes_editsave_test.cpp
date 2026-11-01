#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "pes/editsave.hpp"
#include "pes/exe.hpp"

namespace {

std::string Hex(const std::array<uint8_t, 64>& d) {
  static const char* digits = "0123456789abcdef";
  std::string s;
  for (uint8_t b : d) s += digits[b >> 4], s += digits[b & 15];
  return s;
}

// A synthetic EDIT save, built from the documented layout (docs/re/
// pes-kit-assembly.md 8.1): 0x100 encryption header + 0x40 salt, then 0xD0 file
// header, then the four blocks (description 0, logo 1, data 2, serial 3) each
// encrypted with the root key XORed by its block index, and four SHA-512
// digests at 0x00..0x100 of the decrypted header. `mutation` selects a
// corruption applied to the serialised file: 1 flips a byte of the block-2
// ciphertext, 2 zeroes its tail (a torn write), 3 flips a bit of the stored
// block-2 digest.
struct SaveFile {
  std::vector<uint8_t> bytes;
};

SaveFile BuildSave(int mutation) {
  constexpr size_t SALT = 0x40, FILE_HEADER = 0xD0, HEADER = 0x140, BLOCKS_AT = 0x210;
  constexpr uint32_t PLAYERS = 3, STRIDE = 0x138, PLAYERS_AT = 0x7C, APPEARANCE_AT = 0xF0;
  const size_t dataLen = PLAYERS_AT + size_t(PLAYERS) * STRIDE;

  std::vector<uint8_t> b0(384, 0), b1(14235, 0), b2(dataLen, 0), b3(90, 0);
  for (size_t i = 0; i < b0.size(); ++i) b0[i] = uint8_t(i * 7 + 1);
  for (size_t i = 0; i < b1.size(); ++i) b1[i] = uint8_t(i * 11 + 3);
  for (size_t i = 0; i < b3.size(); ++i) b3[i] = uint8_t(i * 17 + 9);
  // data.dat: the player count the reader uses.
  std::memcpy(&b2[0x60], &PLAYERS, 4);
  for (uint32_t r = 0; r < PLAYERS; ++r) {
    uint8_t* rec = &b2[PLAYERS_AT + STRIDE * r];
    const uint32_t id = 70101 + r;
    std::memcpy(rec, &id, 4);
    rec[0x0A] = uint8_t(150 + r);  // the height byte (save id 0x10, row byte 0x31)
    uint8_t* ap = rec + APPEARANCE_AT;  // the 72-byte appearance record
    std::memcpy(ap, &id, 4);
    const uint32_t X = (38u << 4) | (12u << 18);  // boots 38, glove 12
    std::memcpy(ap + 0x04, &X, 4);
    const uint32_t look = (2u << 6) | (2u << 10) | (1u << 14);  // long, socks 2, tail out
    std::memcpy(ap + 0x0C + 0x08, &look, 4);
    const uint32_t skin = 5u << 8;
    std::memcpy(ap + 0x0C + 0x20, &skin, 4);
  }

  const std::array<uint8_t, 64> d0 = pes::Sha512(b0), d1 = pes::Sha512(b1), d2 = pes::Sha512(b2),
                               d3 = pes::Sha512(b3);
  const uint32_t sizes[4] = {384, 14235, uint32_t(dataLen), 45};
  std::vector<uint8_t> fileHeader(FILE_HEADER, 0);
  std::memcpy(&fileHeader[0x40], &sizes[2], 4);
  std::memcpy(&fileHeader[0x44], &sizes[1], 4);
  std::memcpy(&fileHeader[0x48], &sizes[0], 4);
  std::memcpy(&fileHeader[0x4C], &sizes[3], 4);
  std::memcpy(&fileHeader[0x90], "EDIT", 5);
  const char title[] = "eFootball PES 2021 SEASON UPDATE";
  std::memcpy(&fileHeader[0xB0], title, sizeof title);

  std::vector<uint8_t> salt(SALT);
  for (size_t i = 0; i < SALT; ++i) salt[i] = uint8_t(0x5A ^ (i * 3));
  std::vector<uint8_t> titleBytes;
  for (char16_t c : std::u16string_view(u"eFootball PES 2021 SEASON UPDATE"))
    titleBytes.push_back(uint8_t(c)), titleBytes.push_back(uint8_t(c >> 8));
  const std::array<uint8_t, 64> titleDigest = pes::Sha512(titleBytes);
  std::array<uint8_t, 64> keyHdr;
  for (size_t i = 0; i < 64; ++i) keyHdr[i] = titleDigest[i] ^ salt[i];

  // +0x00..0x100 of the decrypted header: the game's four per-block SHA-512
  // digests, which the reader checks exactly as the game does.
  std::vector<uint8_t> head(HEADER, 0);
  std::memcpy(&head[0], &d0, 64);
  std::memcpy(&head[64], &d1, 64);
  std::memcpy(&head[128], &d2, 64);
  std::memcpy(&head[192], &d3, 64);
  std::memcpy(&head[0x100], salt.data(), SALT);
  SaveFile out;
  out.bytes = pes::CryptStream(keyHdr, head);
  std::memcpy(&out.bytes[0x100], salt.data(), SALT);  // the salt stays plain

  // R = the decrypted 0x140-byte header folded into 64 bytes by XOR; each block
  // is encrypted with R XOR its index, the file header with R XOR 0xD0.
  std::array<uint8_t, 64> root{};
  for (size_t i = 0; i < HEADER; ++i) root[i % 64] ^= head[i];
  const auto key = [&root](uint64_t n) {
    std::array<uint8_t, 64> k = root;
    for (size_t i = 0; i < 64; i += 8) {
      uint64_t w;
      std::memcpy(&w, &k[i], 8);
      w ^= n;
      std::memcpy(&k[i], &w, 8);
    }
    return k;
  };
  const std::vector<uint8_t> fileHeaderEnc = pes::CryptStream(key(0xD0), fileHeader);
  out.bytes.insert(out.bytes.end(), fileHeaderEnc.begin(), fileHeaderEnc.end());
  const std::vector<uint8_t>* blocks[4] = {&b0, &b1, &b2, &b3};
  for (int b = 0; b < 4; ++b) {
    const std::vector<uint8_t> enc = pes::CryptStream(key(uint64_t(b)), *blocks[b]);
    out.bytes.insert(out.bytes.end(), enc.begin(), enc.end());
  }
  // Block 2's ciphertext starts after the header and blocks 0 and 1.
  const size_t dataAt = BLOCKS_AT + sizes[0] + sizes[1];
  if (mutation == 1) out.bytes[dataAt + sizes[2] / 2] ^= 0xFF;  // one flipped byte
  if (mutation == 2)
    for (size_t i = 0; i < 0x100; ++i) out.bytes[dataAt + sizes[2] - 1 - i] = 0;  // a torn tail
  if (mutation == 3) {
    // A second, partial write: the block-2 *content* was rewritten after the
    // digest was stored, so the digest no longer covers the bytes in the file
    // while every other block still verifies. This is what the owner's current
    // save does.
    std::vector<uint8_t> stale = b2;
    stale[stale.size() / 2] ^= 0x01;
    const std::vector<uint8_t> staleEnc = pes::CryptStream(key(uint64_t(2)), stale);
    std::copy(staleEnc.begin(), staleEnc.end(), out.bytes.begin() + dataAt);
  }
  return out;
}

}  // namespace

// FIPS 180-2 test vectors.
TEST(Sha512, MatchesTheStandardVectors) {
  const std::string abc = "abc";
  EXPECT_EQ(Hex(pes::Sha512(std::span(reinterpret_cast<const uint8_t*>(abc.data()), abc.size()))),
            "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
            "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
  EXPECT_EQ(Hex(pes::Sha512({})),
            "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
            "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
  // Two blocks: 112 bytes forces the length into a second block.
  const std::string two =
      "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
  EXPECT_EQ(Hex(pes::Sha512(std::span(reinterpret_cast<const uint8_t*>(two.data()), two.size()))),
            "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
            "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");
}

// Matsumoto & Nishimura's mt19937ar.c reference output for
// init_by_array({0x123, 0x234, 0x345, 0x456}).
TEST(Mt19937, InitByArrayMatchesTheReference) {
  pes::Mt19937 mt(std::vector<uint32_t>{0x123, 0x234, 0x345, 0x456});
  EXPECT_EQ(mt.Next(), 1067595299u);
  EXPECT_EQ(mt.Next(), 955945823u);
  EXPECT_EQ(mt.Next(), 477289528u);
}

// The cipher is its own inverse (FUN_14140e0f0 serves both directions), and a
// 1-3 byte tail is covered.
TEST(CryptStream, IsAnInvolutionIncludingTheTail) {
  std::array<uint8_t, 64> key{};
  for (size_t i = 0; i < key.size(); ++i) key[i] = uint8_t(i * 7 + 3);
  std::vector<uint8_t> plain(4 * 9 + 3);
  for (size_t i = 0; i < plain.size(); ++i) plain[i] = uint8_t(i);
  auto once = pes::CryptStream(key, plain);
  EXPECT_NE(once, plain);
  EXPECT_EQ(pes::CryptStream(key, once), plain);
}

// Body +0x08: sleeves bits 6-7 (0 = default short, else value - 1), socks
// bits 10-11, shirt tail bit 14; body +0x20 bits 8-10 skin; X bits 4-17 boots,
// 18-31 glove (pes-kit-assembly.md 8.1).
TEST(Appearance, DecodesTheLookFields) {
  std::array<uint8_t, pes::kAppearanceRecordBytes> rec{};
  const uint32_t id = 76711, x = (1386u << 4) | (12u << 18);
  const uint32_t body08 = (2u << 6) | (2u << 10) | (1u << 14), body20 = 5u << 8;
  std::memcpy(&rec[0], &id, 4);
  std::memcpy(&rec[4], &x, 4);
  std::memcpy(&rec[0xC + 0x08], &body08, 4);
  std::memcpy(&rec[0xC + 0x20], &body20, 4);
  const pes::PlayerLook look = pes::DecodeLook(rec);
  EXPECT_TRUE(look.longSleeves);
  EXPECT_EQ(look.socks, 2);
  EXPECT_TRUE(look.shirtTailOut);
  EXPECT_EQ(look.skin, 5);
  EXPECT_EQ(look.bootsId, 1386);
  EXPECT_EQ(look.gloveId, 12);
}

// The user's own save, given at run time only. The game checks a SHA-512 of
// every block against the decrypted header but loads the block anyway when
// the file's flag bit for it is set (FUN_14140f470); so does the reader. This
// save's values change whenever the owner plays, so only structural
// invariants are asserted. The owner's current save carries a stale block-2
// digest yet runs in the game, so the stale digest is expected, not skipped.
TEST(EditSaveInstall, DecryptsAndMatchesTheRunningGame) {
  const char* path = std::getenv("PES21_EDIT_SAVE");
  if (!path) GTEST_SKIP() << "PES21_EDIT_SAVE not set";
  std::ifstream f(path, std::ios::binary);
  ASSERT_TRUE(f) << "cannot open PES21_EDIT_SAVE";
  const std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), {});
  std::string err;
  auto save = pes::ReadEditSave(file, &err);
  ASSERT_TRUE(save) << err;
  // The game's live table is the EDIT save union the PlayerAppearance.bin
  // fallback (pes-kit-assembly.md 8.1), so any save holds thousands of records.
  EXPECT_GT(save->appearances.size(), 1000u);
  for (const auto& [id, record] : save->appearances) {
    // Appearance record: id at +0x00, and the 56-byte body from +0x0C.
    { uint32_t v; std::memcpy(&v, record.data(), 4); EXPECT_EQ(v, id); }
    const pes::PlayerLook look = pes::DecodeLook(record);
    // Field ranges the decoder's bit masks imply (pes-kit-assembly.md 8.1):
    // sleeves 2 bits -> 0..3, socks 2 bits -> 0..3, skin 3 bits -> 0..7.
    EXPECT_LE(look.longSleeves ? 2 : 0, 2);
    EXPECT_GE(look.socks, 0);
    EXPECT_LE(look.socks, 3);
    EXPECT_GE(look.skin, 0);
    EXPECT_LE(look.skin, 7);
    // Boots 14 bits, glove 14 bits of the X word.
    EXPECT_GE(look.bootsId, 0);
    EXPECT_LE(look.bootsId, 0x3fff);
    EXPECT_GE(look.gloveId, 0);
    EXPECT_LE(look.gloveId, 0x3fff);
  }
}

// A synthetic save, built from the documented layout, round-trips through the
// reader; a flipped ciphertext byte, a zeroed tail and a block rewritten
// after its digest each surface as the block's SHA-512 warning while the
// block is still used (OBS: the load path memcmps 0x40 bytes per block, and
// the running game holds a block whose stored digest is stale).
TEST(EditSaveSave, RoundTripsAFileAndWarnsOnCorruption) {
  const SaveFile good = BuildSave(0);
  std::string err;
  auto save = pes::ReadEditSave(good.bytes, &err);
  ASSERT_TRUE(save) << err;
  EXPECT_TRUE(err.empty());
  ASSERT_EQ(save->appearances.size(), 3u);
  // The raw records come through in save order, whole.
  ASSERT_EQ(save->players.size(), 3u);
  for (uint32_t i = 0; i < 3; ++i) {
    uint32_t id = 0;
    std::memcpy(&id, save->players[i].data() + pes::kEditIdOffset, 4);
    EXPECT_EQ(id, 70101 + i);
    EXPECT_EQ(save->players[i][0x0A], 150 + i);
  }
  for (uint32_t i = 0; i < 3; ++i) {
    const uint32_t id = 70101 + i;
    const pes::PlayerLook look = pes::DecodeLook(save->appearances.at(id));
    EXPECT_TRUE(look.longSleeves);
    EXPECT_EQ(look.socks, 2);
    EXPECT_TRUE(look.shirtTailOut);
    EXPECT_EQ(look.skin, 5);
    EXPECT_EQ(look.bootsId, 38);
    EXPECT_EQ(look.gloveId, 12);
  }
  // 1: a flipped ciphertext byte. 2: a zeroed tail (torn write). 3: a block
  // rewritten after its digest (what the owner's current save does). Each
  // must still read, and each must report the block's SHA-512 warning.
  for (int mode = 1; mode <= 3; ++mode) {
    const SaveFile bad = BuildSave(mode);
    std::string e2;
    auto kept = pes::ReadEditSave(bad.bytes, &e2);
    EXPECT_TRUE(kept) << "mutation " << mode << " must still read, like the game does";
    EXPECT_NE(e2.find("SHA-512"), std::string::npos) << "mutation " << mode << ": " << e2;
  }
}

// PlayerAppearance.bin: 60-byte records (u32 id + the 56-byte body). The game's
// live table holds them as 72-byte records with X and the alternate id zero.
TEST(PlayerAppearanceFile, BecomesRecordsWithNoBootsOrGlove) {
  std::vector<uint8_t> blob(2 * pes::kAppearanceFileRecordBytes, 0);
  const uint32_t id0 = 70101, id1 = 70102, look1 = (2u << 6) | (1u << 14);
  std::memcpy(&blob[0], &id0, 4);
  std::memcpy(&blob[pes::kAppearanceFileRecordBytes], &id1, 4);
  std::memcpy(&blob[pes::kAppearanceFileRecordBytes + 4 + 0x08], &look1, 4);  // body +0x08
  std::string err;
  auto table = pes::ParsePlayerAppearanceFile(blob, &err);
  ASSERT_TRUE(table) << err;
  ASSERT_EQ(table->size(), 2u);
  const pes::PlayerLook look = pes::DecodeLook(table->at(id1));
  EXPECT_TRUE(look.longSleeves);
  EXPECT_TRUE(look.shirtTailOut);
  EXPECT_EQ(look.bootsId, 0);
}

TEST(PlayerAppearanceFile, RejectsPartialRecords) {
  std::vector<uint8_t> blob(pes::kAppearanceFileRecordBytes + 1, 0);
  std::string err;
  EXPECT_FALSE(pes::ParsePlayerAppearanceFile(blob, &err));
}

// --- the save record -> roster row map (editsave.hpp) ---

void SetBits(pes::EditPlayerRecord& rec, size_t bit, size_t width, uint32_t value) {
  for (size_t i = 0; i < width; ++i) {
    const size_t b = bit + i;
    rec[b / 8] = uint8_t((rec[b / 8] & ~(1u << (b % 8))) | (((value >> i) & 1u) << (b % 8)));
  }
}

const pes::EditRowField& FieldOfRow(size_t row) {
  for (const pes::EditRowField& f : pes::kEditRowFields)
    if (f.row == row) return f;
  ADD_FAILURE() << "no field for row byte " << row;
  return pes::kEditRowFields[0];
}

// A non-flat record: every field holds a value of its own, so a row byte that reads another field's bits shows.
TEST(EditRow, EveryRowByteTakesItsOwnFieldWhenNoTwoFieldsAgree) {
  const auto valueOf = [](const pes::EditRowField& f) {
    // Inside the gate and the clamp, and different for every row byte where the range allows.
    const uint32_t lo = std::max(f.gateMin, f.clampMin), hi = std::min(f.gateMax, f.clampMax);
    return lo + f.row % (hi - lo + 1);
  };
  pes::EditPlayerRecord rec{};
  for (const pes::EditRowField& f : pes::kEditRowFields) SetBits(rec, f.bit, f.width, valueOf(f));
  const pes::EditRow row = pes::DecodeEditRow(rec);
  for (const pes::EditRowField& f : pes::kEditRowFields) {
    if (f.row == pes::kRowPosition) continue;  // the position's own rule is tested below
    EXPECT_TRUE(row.set[f.row]) << "row byte " << int(f.row);
    EXPECT_EQ(row.byte[f.row], valueOf(f)) << "row byte " << int(f.row);
  }
  EXPECT_FALSE(row.set[0x33]) << "the builder never writes row byte 0x33";
}

// The fields GF reads, with their save bits as literals (decoded from the exe's getters, pes-port-EditSaveFields.md 2): Speed is
// save id 0x1C, Jump id 0x1F; the two bits a bit-order guess would have read (256, 358) are GK Reach and GK Reflexes.
TEST(EditRow, SpeedJumpHeightFootInjuryAndFormComeFromTheirDecodedBits) {
  pes::EditPlayerRecord rec{};
  SetBits(rec, 174, 7, 71);  // id 0x1C Speed
  SetBits(rec, 192, 7, 82);  // id 0x1F Jump
  SetBits(rec, 256, 7, 93);  // id 0x2A GK Reach (row byte 0x25)
  SetBits(rec, 358, 7, 55);  // id 0x29 GK Reflexes (row byte 0x24)
  rec[0x0A] = 188;           // id 0x10 Height
  SetBits(rec, 381, 1, 1);   // id 0x0F foot
  SetBits(rec, 252, 3, 6);   // id 0x2D Form
  SetBits(rec, 327, 2, 1);   // id 0x2E injury tier
  const pes::EditRow row = pes::DecodeEditRow(rec);
  EXPECT_EQ(row.byte[pes::kRowSpeed], 71);
  EXPECT_EQ(row.byte[pes::kRowJump], 82);
  EXPECT_EQ(row.byte[0x25], 93);
  EXPECT_EQ(row.byte[0x24], 55);
  EXPECT_EQ(row.byte[pes::kRowHeightCm], 188);
  EXPECT_EQ(row.byte[pes::kRowStrongerFoot], 1);
  EXPECT_EQ(row.byte[pes::kRowForm], 6);
  EXPECT_EQ(row.byte[pes::kRowInjuryResistance], 1);
}

TEST(EditRow, AFieldOutsideItsDescriptorRangeIsNotApplied) {
  pes::EditPlayerRecord rec{};
  SetBits(rec, FieldOfRow(pes::kRowSpeed).bit, 7, 39);            // Speed gate 40..99
  SetBits(rec, FieldOfRow(pes::kRowJump).bit, 7, 100);            // Jump gate 40..99
  rec[0x0A] = 99;                                                 // Height gate 100..250
  SetBits(rec, FieldOfRow(pes::kRowInjuryResistance).bit, 2, 3);  // tier gate 0..2
  SetBits(rec, FieldOfRow(pes::kRowForm).bit, 3, 7);              // Form gate 0..7: applied
  const pes::EditRow row = pes::DecodeEditRow(rec);
  EXPECT_FALSE(row.set[pes::kRowSpeed]);
  EXPECT_FALSE(row.set[pes::kRowJump]);
  EXPECT_FALSE(row.set[pes::kRowHeightCm]);
  EXPECT_FALSE(row.set[pes::kRowInjuryResistance]);
  EXPECT_TRUE(row.set[pes::kRowForm]);
  EXPECT_EQ(row.byte[pes::kRowForm], 7);
}

// Gate then clamp: Height 100..250 is accepted and clamped to the block's 132..220, Weight 30..150 to 30..129.
TEST(EditRow, AnAcceptedValueIsClampedToTheBlockBounds) {
  pes::EditPlayerRecord rec{};
  rec[0x0A] = 250;
  rec[0x0B] = 140;
  pes::EditRow row = pes::DecodeEditRow(rec);
  EXPECT_EQ(row.byte[pes::kRowHeightCm], 220);
  EXPECT_EQ(row.byte[0x32], 129);
  rec[0x0A] = 100;
  rec[0x0B] = 30;
  row = pes::DecodeEditRow(rec);
  EXPECT_EQ(row.byte[pes::kRowHeightCm], 132);
  EXPECT_EQ(row.byte[0x32], 30);
}

// The live record's position field is 4 bits and FUN_141FEA000 turns a value above 12 into 1.
TEST(EditRow, PositionKeepsFourBitsAndAboveTwelveBecomesOne) {
  const size_t at = FieldOfRow(pes::kRowPosition).bit;
  using Case = std::pair<uint32_t, uint32_t>;
  for (const auto& [saved, want] : {Case{0, 0}, Case{12, 12}, Case{13, 1}, Case{15, 1}, Case{20, 4}, Case{28, 12}, Case{31, 1}}) {
    pes::EditPlayerRecord rec{};
    SetBits(rec, at, 5, saved);
    EXPECT_EQ(pes::DecodeEditRow(rec).byte[pes::kRowPosition], want) << "saved " << saved;
  }
}

TEST(EditRow, EachSkillFlagSetsItsOwnSkillBit) {
  for (size_t k = 0; k < std::size(pes::kEditSkillBit); ++k) {
    pes::EditPlayerRecord rec{};
    SetBits(rec, pes::kEditSkillFirstBit + k, 1, 1);
    EXPECT_EQ(pes::DecodeEditRow(rec).skills, uint64_t{1} << pes::kEditSkillBit[k]) << "flag " << k;
  }
}

// --- the table against the exe: the descriptor getters, the builder's key tables and the clamp table ---

struct Getter {
  size_t bit = 0, width = 0;
};

// A descriptor getter is `[movzx/mov eax, [rcx+disp8]] [shr eax, imm] [and eax, imm8] mov [rdx], eax; mov al, 1; ret`.
bool DecodeGetter(const uint8_t* p, Getter* out) {
  size_t at = 0, sizeBits = 0, off = 0;
  if (p[0] == 0x0F && p[1] == 0xB6 && p[2] == 0x41) sizeBits = 8, off = p[3], at = 4;
  else if (p[0] == 0x0F && p[1] == 0xB7 && p[2] == 0x41) sizeBits = 16, off = p[3], at = 4;
  else if (p[0] == 0x8B && p[1] == 0x41) sizeBits = 32, off = p[2], at = 3;
  else return false;
  size_t shift = 0, mask = 0;
  if (p[at] == 0xC1 && p[at + 1] == 0xE8) shift = p[at + 2], at += 3;
  else if (p[at] == 0xD1 && p[at + 1] == 0xE8) shift = 1, at += 2;
  if (p[at] == 0x83 && p[at + 1] == 0xE0) mask = p[at + 2], at += 3;
  if (!(p[at] == 0x89 && p[at + 1] == 0x02 && p[at + 2] == 0xB0 && p[at + 3] == 0x01 && p[at + 4] == 0xC3)) return false;
  out->bit = off * 8 + shift;
  out->width = mask ? size_t(std::popcount(mask)) : sizeBits - shift;
  return true;
}

TEST(EditRowInstall, TheTableEqualsTheExesDescriptorGettersAndBuilderTables) {
  const char* root = std::getenv("PES21_ROOT");
  if (root == nullptr) GTEST_SKIP() << "PES21_ROOT unset (the tables are read from its exe)";
  std::string error;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &error);
  ASSERT_TRUE(exe) << error;
  const auto u32At = [&](uint64_t va) {
    uint32_t v = 0;
    EXPECT_TRUE(exe->ReadVa(va, 4, reinterpret_cast<uint8_t*>(&v))) << std::hex << va;
    return v;
  };
  // The descriptor table 0x1434F2B20: 0x7A entries of six qwords {id, getter, setter, min | default << 32, max | bits << 32, 0}.
  struct Desc {
    uint32_t min, max;
    Getter get;
    bool simple;
  };
  std::map<uint32_t, Desc> desc;
  for (uint32_t i = 0; i < 0x7A; ++i) {
    const uint64_t at = 0x1434F2B20 + i * 0x30;
    Desc d{u32At(at + 0x18), u32At(at + 0x20), {}, false};
    uint64_t getter = 0;
    ASSERT_TRUE(exe->ReadVa(at + 8, 8, reinterpret_cast<uint8_t*>(&getter)));
    uint8_t code[16];
    ASSERT_TRUE(exe->ReadVa(getter, sizeof code, code));
    d.simple = DecodeGetter(code, &d.get);
    desc[u32At(at)] = d;
  }
  // The builder's key tables, {row, key} pairs, and the save id of key k (FUN_141F10CE0's cases: 0x12 + k, 0x33 + k, 0x71 + k, 0x2F + k).
  std::map<uint32_t, uint32_t> idOfRow;
  const auto pairs = [&](uint64_t va, int n, uint32_t idBase) {
    for (int i = 0; i < n; ++i) {
      const uint32_t row = u32At(va + 8 * i), key = u32At(va + 8 * i + 4);
      EXPECT_TRUE(idOfRow.emplace(row, idBase + key).second) << "row " << row << " built twice";
    }
  };
  pairs(0x14352AB10, 29, 0x12);  // FUN_141FE9BA0: abilities, the two weak-foot bytes, Form and the injury tier
  pairs(0x14352ADC0, 13, 0x33);  // FUN_141FEA000: position familiarity
  pairs(0x14352AE30, 9, 0x71);   // FUN_141FE9E50
  pairs(0x14352ABF8, 2, 0x2F);   // FUN_141FEA170
  // The single-field rows: position (FUN_141FEA000), the 0x10 byte (FUN_141FE9BA0's tail), age/height/weight (FUN_141FE9D00), foot.
  for (const auto& [row, id] : std::map<uint32_t, uint32_t>{{0x00, 0x0E}, {0x10, 0x32}, {0x30, 0x0D}, {0x31, 0x10}, {0x32, 0x11}, {0x34, 0x0F}})
    EXPECT_TRUE(idOfRow.emplace(row, id).second) << "row " << row << " built twice";

  ASSERT_EQ(idOfRow.size(), std::size(pes::kEditRowFields)) << "the table has one entry per row byte the builder writes";
  for (const pes::EditRowField& f : pes::kEditRowFields) {
    const auto it = idOfRow.find(f.row);
    ASSERT_NE(it, idOfRow.end()) << "row " << int(f.row);
    EXPECT_EQ(f.saveId, it->second) << "row " << int(f.row);
    const Desc& d = desc.at(f.saveId);
    ASSERT_TRUE(d.simple) << "id " << int(f.saveId);
    EXPECT_EQ(f.bit, d.get.bit) << "row " << int(f.row) << " id " << int(f.saveId);
    EXPECT_EQ(f.width, d.get.width) << "row " << int(f.row) << " id " << int(f.saveId);
    EXPECT_EQ(f.gateMin, d.min) << "row " << int(f.row);
    EXPECT_EQ(f.gateMax, d.max) << "row " << int(f.row);
    uint8_t clamp[2];
    ASSERT_TRUE(exe->ReadVa(0x142613C60 + 2 * f.row, 2, clamp));
    EXPECT_EQ(f.clampMin, clamp[0]) << "row " << int(f.row);
    EXPECT_EQ(f.clampMax, clamp[1]) << "row " << int(f.row);
  }
  // Skills: FUN_141FEA250's table 0x14352AC70 (41 {skill bit, key}); key k's save flag is id 0x48 + k.
  for (uint32_t i = 0; i < std::size(pes::kEditSkillBit); ++i) {
    const uint32_t skillBit = u32At(0x14352AC70 + 8 * i), key = u32At(0x14352AC70 + 8 * i + 4);
    ASSERT_LT(key, std::size(pes::kEditSkillBit));
    EXPECT_EQ(pes::kEditSkillBit[key], skillBit) << "key " << key;
    EXPECT_TRUE(desc.at(0x48 + key).simple) << "key " << key;
    EXPECT_EQ(desc.at(0x48 + key).get.bit, pes::kEditSkillFirstBit + key);
    EXPECT_EQ(desc.at(0x48 + key).get.width, 1u);
  }
}
