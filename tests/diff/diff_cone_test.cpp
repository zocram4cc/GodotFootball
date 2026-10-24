// Differential test: FUN_140A53DF0's inner cone test vs
// pes::marktable::ConePasses.
//
// The corpus is generated at test time from the user's executable by
// tools/dev/diffharness (specs/cone.json; provenance: DiffHarnessRE
// specs/cone.json -- CAND/HOLD 16-byte pos structs, XMM3=head-deg, stack
// coneDeg=90.0/outerLen=12.0/innerWord, no stubs: bearing/atan2 run real).
// This binary only reads build/diff/cone.jsonl and calls GF's port.
//
// Exactness note: GF's bearing uses std::atan2 where PES runs its atan2
// polynomial 0x140A92D40 (pesmarktable.cpp's PairBearing ponytail;
// mark-cone.md section 8 open question 1: bearings agree to ~1e-7, not bits).
// A case whose vote lands within 2e-6 of the half-cone edge is decided by that
// documented approximation, so it is reported but not failed; anything further
// from the edge must match bit for bit.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include "diff_json.hpp"
#include "onthepitch/pesmarktable.hpp"
#include "pes/exe.hpp"

namespace {

std::string ExePath() {
  const char* exe = std::getenv("PES_EXE");
  if (exe) return exe;
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return "";
  return std::string(root) + "/PES2021.exe";
}

float BitsToFloat(const diff::Value& v) {
  float f = 0.0f;
  const uint32_t bits = v.AsBits();
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

float WrapFold(float d) {
  if (d > 180.0f) d -= 360.0f;
  if (d <= -180.0f) d += 360.0f;
  return d;
}

// The vote's distance from the half-cone edge, recomputed with GF's own
// bearing (std::atan2): |wrap(live - head)| - half*coneDeg. Near zero the
// atan2 polynomial's ~1e-7 error can flip the vote.
float EdgeMargin(float candX, float candZ, float holderX, float holderZ, float head,
                 const pes::marktable::Constants& k) {
  float live = 0.0f;
  if (candX != holderX || candZ != holderZ) {
    live = std::atan2(holderX - candX, holderZ - candZ) * 57.295776f;
    if (live < 0.0f) live += 360.0f;
  }
  return std::fabs(WrapFold(live - head)) - k.half * k.coneDeg;
}

TEST(DiffCone, ConeTestMatchesPES) {
  if (ExePath().empty()) GTEST_SKIP() << "PES21_ROOT/PES_EXE not set";
  auto cases = diff::ReadCorpus(diff::CorpusPath("cone"));
  if (cases.empty()) GTEST_SKIP() << "no corpus (generator skipped)";
  std::string err;
  auto exe = pes::PeImage::Open(ExePath(), &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::marktable::LoadMarkTableConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  int approx = 0;
  for (const auto& c : cases) {
    const float cx = BitsToFloat(c.in.At("cand_x")), cz = BitsToFloat(c.in.At("cand_z"));
    const float hx = BitsToFloat(c.in.At("hold_x")), hz = BitsToFloat(c.in.At("hold_z"));
    const float head = BitsToFloat(c.in.At("headbits"));
    const bool gf = pes::marktable::ConePasses(cx, cz, hx, hz, head, *k);
    const int pes = c.out.At("cone_hit").AsInt();
    if (gf == (pes != 0)) continue;
    // Mismatch: only tolerable inside the documented atan2 margin.
    const float dx = hx - cx, dz = hz - cz;
    const bool annulusVote = dx * dx + dz * dz < k->coneLen * k->coneLen;
    const float margin = EdgeMargin(cx, cz, hx, hz, head, *k);
    if (annulusVote && std::fabs(margin) < 2e-6f) {
      ++approx;  // PairBearing ponytail: std::atan2 vs 0x140A92D40 (~1e-7)
      continue;
    }
    EXPECT_EQ(gf, pes != 0) << "cand (" << cx << "," << cz << ") holder (" << hx << "," << hz
                            << ") head " << head << " margin " << margin;
  }
  printf("diff-cone: %d of %zu cases inside the documented atan2 margin\n", approx, cases.size());
}

}  // namespace
