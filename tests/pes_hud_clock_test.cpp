// The HUD clock-state builder (FUN_1420EFEC0) run from the translated layer on PES-layout state
// (src/pes/hud_clock_build.*). The install supplies the constants and globals the builder reads, so
// the tests skip without PES21_ROOT.

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "pes/exe.hpp"
#include "pes/hud_clock_build.hpp"

namespace {

std::unique_ptr<pes::ClockStateBuilder> MakeBuilder() {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return nullptr;
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  if (!exe) return nullptr;
  return pes::ClockStateBuilder::Create(*exe, &err);
}

// Word indices of the builder's output struct (param_2): hours+offset, minute, the digit variant.
constexpr int kHoursWord = 3;
constexpr int kMinuteWord = 4;
constexpr int kDigitWord = 5;

pes::ClockEnvInputs Live(float seconds) {
  pes::ClockEnvInputs in;
  in.period = 1;  // MI+0x1168 = 1: FUN_1420BE140 adds nothing, so the minute is the HMS minute
  in.inPlay = 1;
  in.liveFlag = 0;
  in.liveSeconds = seconds;
  return in;
}

}  // namespace

TEST(PesHudClockBuilder, LiveHalfMinuteIsTheMinuteOfTheSecondsCount) {
  auto b = MakeBuilder();
  if (!b) GTEST_SKIP() << "PES21_ROOT not set";
  const auto out = b->Build(Live(1350.0f));  // 22 minutes 30 seconds
  EXPECT_EQ(out[kMinuteWord], 22u);
  EXPECT_EQ(out[kHoursWord], 0u);
}

TEST(PesHudClockBuilder, LiveHalfHoursFromTheSecondsCount) {
  auto b = MakeBuilder();
  if (!b) GTEST_SKIP() << "PES21_ROOT not set";
  const auto out = b->Build(Live(3600.0f));  // one hour: the hours word counts it, the minute wraps to 0
  EXPECT_EQ(out[kHoursWord], 1u);
  EXPECT_EQ(out[kMinuteWord], 0u);
}

TEST(PesHudClockBuilder, FrozenPathReadsItsOwnFloat) {
  auto b = MakeBuilder();
  if (!b) GTEST_SKIP() << "PES21_ROOT not set";
  pes::ClockEnvInputs in = Live(100.0f);  // the live float must not reach this path
  in.liveFlag = 1;
  in.frozenSeconds = 2700.0f;  // 45 minutes
  const auto out = b->Build(in);
  EXPECT_EQ(out[kMinuteWord], 45u);
}

TEST(PesHudClockBuilder, DigitVariantFollowsTheHoursWord) {
  auto b = MakeBuilder();
  if (!b) GTEST_SKIP() << "PES21_ROOT not set";
  // The builder's digit variant is 1 + (hours < 10): a one-digit hours word takes variant 2.
  const auto out = b->Build(Live(1350.0f));
  EXPECT_EQ(out[kDigitWord], 2u);
}
