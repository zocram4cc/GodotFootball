// PES body clips converted for the engine (onthepitch/pesclips.hpp): a
// synthetic clip checks the verbatim basis map, the 10 ms resampling and the
// player line; one install test converts every imported clip and loads it.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "onthepitch/pesclips.hpp"
#include "onthepitch/pesreceive.hpp"
#include "pes/mbinfo.hpp"
#include "rig/clip_pose.hpp"
#include "rig/rig_names.hpp"
#include "onthepitch/player/humanoid/animcollection.hpp"
#include "utils/animation.hpp"

using namespace blunted;

namespace {

pesrig::Rig SyntheticRig() {
  pesrig::Rig rig;
  const auto& names = pesrig::RigNodeNames();
  for (int j = 0; j < pesrig::BODY_NODE_COUNT; ++j) {
    pesrig::RigBone b;
    b.gfName = names[j].gfName;
    b.pesBone = names[j].pesBone;
    b.parent = j == 0 ? -1 : 0;
    rig.bones.push_back(b);
  }
  rig.bones[0].animGlobalPos = {0.0f, 1.0f, 0.0f};  // body bind, Fox Y up
  return rig;
}

pes::GaniSegment QuatTrack(pes::Quat a, pes::Quat b, uint32_t frames) {
  pes::GaniSegment s;
  s.type = pes::GANI_QUAT;
  s.frames = {0, frames};
  s.quats = {a, b};
  return s;
}

pes::GaniSegment VecTrack(pes::Vec4 a, pes::Vec4 b, uint32_t frames) {
  pes::GaniSegment s;
  s.type = pes::GANI_VECTOR3;
  s.frames = {0, frames};
  s.vecs = {a, b};
  return s;
}

TEST(PesClips, ConvertsVerbatimOntoTheEngineBasis) {
  const pesrig::Rig rig = SyntheticRig();
  pes::PesClip clip;
  clip.name = "synthetic";
  clip.motionNo = 7;
  const uint32_t frames = 3;  // 0.1 s at 1/30 s
  clip.gani.frameCount = frames;
  clip.gani.units.resize(11);
  // RIG_ROOT: no rotation, moves +Z (Fox forward) by 16384 raw = 1 m.
  clip.gani.units[pesrig::PES_ROOT_UNIT].segments = {
      QuatTrack({}, {}, frames), VecTrack({0, 0, 0, 0}, {0, 0, 16384, 0}, frames)};
  clip.gani.units[pesrig::PES_MOTION_UNIT].segments = {QuatTrack({}, {}, frames),
                                                      VecTrack({}, {}, frames)};
  // sk_neck (unit 9 seg 0) turned 90 deg about Fox +Y (up).
  const float h = std::sqrt(0.5f);
  clip.gani.units[9].segments = {QuatTrack({0, h, 0, h}, {0, h, 0, h}, frames)};

  Animation anim;
  anim.LoadText(clip.name, pes::PesClipAnimText(clip, rig));
  // 0.1 s at 10 ms: frames 0..10.
  EXPECT_EQ(anim.GetFrameCount(), 11);
  Quaternion q;
  Vector3 pos;
  ASSERT_TRUE(anim.GetKeyFrame("neck", 5, q, pos, true, false));
  // (x, y, z, w) -> (x, -z, y, w): Fox up becomes engine up (+Z).
  EXPECT_NEAR(q.elements[0], 0.0f, 1e-5f);
  EXPECT_NEAR(q.elements[1], 0.0f, 1e-5f);
  EXPECT_NEAR(q.elements[2], h, 1e-5f);
  EXPECT_NEAR(q.elements[3], h, 1e-5f);
  // The root's +Z (Fox forward) becomes engine -Y, starting at the origin.
  ASSERT_TRUE(anim.GetKeyFrame("player", 10, q, pos, false, true));
  EXPECT_NEAR(pos.coords[0], 0.0f, 1e-4f);
  EXPECT_NEAR(pos.coords[1], -1.0f, 1e-4f);
  EXPECT_NEAR(pos.coords[2], 0.0f, 1e-4f);
  EXPECT_EQ(anim.GetAnimType(), "special");
  EXPECT_EQ(anim.GetVariable("specialvar1"), std::to_string(int(pes::PesClipFamily::kDodge)));
  EXPECT_EQ(anim.GetVariable("specialvar2"), "7");
}

TEST(PesClipsInstall, EveryDodgeClipConvertsAndLoads) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig.has_value()) << err;
  auto clips = pes::LoadPesClips(*exe, vfs, &err);
  ASSERT_TRUE(clips.has_value()) << err;
  EXPECT_GT(clips->clips.size(), 1u);
  // Every clip of the dodge family: the ball dodges (dodge_*), the human
  // dodges (avoidjump_/avoidslide_) and the goal-net dodges (goalnet_*).
  for (const auto& clip : clips->clips) {
    // The dodge, tackle, locomotion and trap families: every clip of the set belongs to one of them, and each
    // has its own conversion test below.
    const bool dodgeFamily =
        clip.name.rfind("dodge_", 0) == 0 || clip.name.rfind("avoid", 0) == 0 ||
        clip.name.rfind("goalnet_", 0) == 0 || clip.name.rfind("rapid", 0) == 0;
    const bool tackleFamily = clip.family == pes::PesClipFamily::kTackle;
    const bool locomotionFamily = clip.family == pes::PesClipFamily::kLocomotion;
    const bool trapFamily = clip.family == pes::PesClipFamily::kTrap;
    EXPECT_TRUE(dodgeFamily || tackleFamily || locomotionFamily || trapFamily) << clip.name;
    if (tackleFamily) continue;  // the tackle family has its own test below
    Animation anim;
    anim.LoadText(clip.name, pes::PesClipAnimText(clip, *rig));
    const pesrig::ClipSampler sampler(clip.gani, *rig);
    EXPECT_NEAR(anim.GetFrameCount() - 1, sampler.LengthSeconds() / 0.01, 1.0) << clip.name;
    Quaternion q;
    Vector3 pos;
    ASSERT_TRUE(anim.GetKeyFrame("player", 0, q, pos, false, true)) << clip.name;
  }
}

TEST(PesClipsInstall, EveryTackleClipConvertsAndLoads) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig.has_value()) << err;
  auto clips = pes::LoadPesClips(*exe, vfs, &err);
  ASSERT_TRUE(clips.has_value()) << err;

  // Whole families, not a sample: the tackle family is what a standing tackle
  // plays, so every one of its clips has to convert or the sub-kind's pick
  // would name a clip the engine cannot load.
  const char* tacklePrefixes[] = {"tacklefoot_", "tacklehold_", "tacklepulled_", "tackleshoulder_",
                                  "block090_", "js_idle_0_0_000_y00_tacklefoot_passcut"};
  int tackled = 0;
  for (const auto& clip : clips->clips) {
    if (clip.family != pes::PesClipFamily::kTackle) continue;
    ++tackled;
    Animation anim;
    anim.LoadText(clip.name, pes::PesClipAnimText(clip, *rig));
    const pesrig::ClipSampler sampler(clip.gani, *rig);
    EXPECT_NEAR(anim.GetFrameCount() - 1, sampler.LengthSeconds() / 0.01, 1.0) << clip.name;
    Quaternion q;
    Vector3 pos;
    ASSERT_TRUE(anim.GetKeyFrame("player", 0, q, pos, false, true)) << clip.name;
  }
  // The install's tackle family is 86 clips; fewer means a prefix was dropped.
  EXPECT_GE(tackled, 86);
}

TEST(PesClipsInstall, ASpecialQueryNamesExactlyOneTackleClip) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig.has_value()) << err;
  auto clips = pes::LoadPesClips(*exe, vfs, &err);
  ASSERT_TRUE(clips.has_value()) << err;
  auto mbinfo = pes::LoadMbinfo(vfs, *exe, &err);
  ASSERT_TRUE(mbinfo.has_value()) << err;
  AnimCollection anims(*rig);
  anims.AddPesClips(*clips, *mbinfo);
  // A named tackle clip resolves to exactly itself and its mirror.
  const pes::PesClip* wanted = nullptr;
  for (const auto& clip : clips->clips)
    if (clip.name == "tacklefoot_mid_0_0_000") wanted = &clip;
  ASSERT_NE(wanted, nullptr);
  CrudeSelectionQuery query;
  query.byFunctionType = true;
  query.functionType = e_FunctionType_Special;
  query.properties.Set("specialvar1", int(pes::PesClipFamily::kTackle));
  query.properties.Set("specialvar2", wanted->motionNo);
  DataSet found;
  anims.CrudeSelection(found, query);
  ASSERT_EQ(found.size(), 2u);  // as authored and mirrored
  for (int id : found) EXPECT_EQ(anims.GetAnim(id)->GetName().rfind(wanted->name, 0), 0u);
}

// The standby motions MOVESTEP's refusals name (4154 mid_jocky_standby_noloop, 4155 near_delay_standby_goalline_noloop).
// They are loaded in the locomotion family as special clips, so the locomotion special query finds them: the load is not
// the cause of their refusals (the body's dodge clip is; PlayIssuedClip's first gate).
TEST(PesClipsInstall, TheStandbyMotionsAreInTheLocomotionSpecialSet) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig.has_value()) << err;
  auto clips = pes::LoadPesClips(*exe, vfs, &err);
  ASSERT_TRUE(clips.has_value()) << err;
  const pes::PesClip* standby = nullptr;
  for (const auto& clip : clips->clips)
    if (clip.motionNo == 4154) standby = &clip;
  ASSERT_NE(standby, nullptr);
  EXPECT_EQ(standby->family, pes::PesClipFamily::kLocomotion);
  auto mbinfo = pes::LoadMbinfo(vfs, *exe, &err);
  ASSERT_TRUE(mbinfo.has_value()) << err;
  AnimCollection anims(*rig);
  anims.AddPesClips(*clips, *mbinfo);
  const int locomotion = int(pes::PesClipFamily::kLocomotion);
  EXPECT_TRUE(anims.HasSpecial(locomotion, 4154)) << "HasSpecial(102, 4154) after AddPesClips";
  EXPECT_TRUE(anims.HasSpecial(locomotion, 4155)) << "HasSpecial(102, 4155) after AddPesClips";
}
// The stalled free-kick taker (seed 7) drifts ~90 deg off his walk command while
// idle_0_3_run_045_mirror plays. A mirrored clip must mirror the root travel on
// the lateral axis only: same forward (engine -Y) travel, negated sideways (X).
TEST(PesClipsInstall, AMirroredLocomotionClipMirrorsRootTravelSideways) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig.has_value()) << err;
  auto clips = pes::LoadPesClips(*exe, vfs, &err);
  ASSERT_TRUE(clips.has_value()) << err;
  auto mbinfo = pes::LoadMbinfo(vfs, *exe, &err);
  ASSERT_TRUE(mbinfo.has_value()) << err;
  AnimCollection anims(*rig);
  anims.AddPesClips(*clips, *mbinfo);
  const pes::PesClip* wanted = nullptr;
  for (const auto& clip : clips->clips)
    if (clip.name == "idle_0_3_run_045") wanted = &clip;
  ASSERT_NE(wanted, nullptr);
  CrudeSelectionQuery query;
  query.byFunctionType = true;
  query.functionType = e_FunctionType_Special;
  query.properties.Set("specialvar1", int(pes::PesClipFamily::kLocomotion));
  query.properties.Set("specialvar2", wanted->motionNo);
  DataSet found;
  anims.CrudeSelection(found, query);
  ASSERT_EQ(found.size(), 2u);
  Animation* plain = nullptr;
  Animation* mirrored = nullptr;
  for (int id : found) {
    Animation* a = anims.GetAnim(id);
    if (a->GetName() == wanted->name)
      plain = a;
    else
      mirrored = a;
  }
  ASSERT_NE(plain, nullptr);
  ASSERT_NE(mirrored, nullptr);
  Quaternion q;
  Vector3 p0, p1, m0, m1;
  const int last = plain->GetFrameCount() - 1;
  ASSERT_GE(last, 1);
  ASSERT_TRUE(plain->GetKeyFrame("player", 0, q, p0, false, true));
  ASSERT_TRUE(plain->GetKeyFrame("player", last, q, p1, false, true));
  ASSERT_TRUE(mirrored->GetKeyFrame("player", 0, q, m0, false, true));
  ASSERT_TRUE(mirrored->GetKeyFrame("player", last, q, m1, false, true));
  const Vector3 plainTravel = (p1 - p0).Get2D();
  const Vector3 mirrorTravel = (m1 - m0).Get2D();
  EXPECT_GT(std::fabs(plainTravel.coords[0]), 1e-4f)
      << "the 045 cell must carry sideways travel to test the mirror";
  EXPECT_NEAR(mirrorTravel.coords[0], -plainTravel.coords[0], 1e-4f);
  EXPECT_NEAR(mirrorTravel.coords[1], plainTravel.coords[1], 1e-4f);
}

}  // namespace

// A PES clip played through a Special command is the action its family names:
// the contact layer must see the dodger as DODGING and the tackler as
// TACKLING, or PES's dodge rules and the challenge count never apply to them.
TEST(PesClips, ASpecialClipCarriesItsFamilysContactAction) {
  using A = pes::ContactAction;
  EXPECT_EQ(pes::SpecialClipAction(int(pes::PesClipFamily::kDodge), A::kDodge), A::kDodge);
  EXPECT_EQ(pes::SpecialClipAction(int(pes::PesClipFamily::kTackle), A::kTackle), A::kTackle);
  // Any other special animation (celebrations, GF's own) is not a challenge,
  // whatever the player's last challenge was.
  EXPECT_EQ(pes::SpecialClipAction(0, A::kTackle), A::kOther);
  EXPECT_EQ(pes::SpecialClipAction(7, A::kDodge), A::kOther);
}

// The clip families PES chose by situation (Animetable row/column): GF must not
// veto them on its own pose-matching heuristics, so the selector asks this
// predicate before relaxing the velocity, body and side filters.
TEST(PesClips, OnlyPesFamiliesAreNamedClips) {
  EXPECT_TRUE(pes::IsPesClipFamily(int(pes::PesClipFamily::kDodge)));
  EXPECT_TRUE(pes::IsPesClipFamily(int(pes::PesClipFamily::kTackle)));
  EXPECT_FALSE(pes::IsPesClipFamily(0));
  EXPECT_FALSE(pes::IsPesClipFamily(99));
  EXPECT_FALSE(pes::IsPesClipFamily(102));
}

TEST(PesClips, LocomotionIsPlayedAsPesPicksItAndIsNeverAChallenge) {
  EXPECT_TRUE(pes::IsPesLocomotionFamily(int(pes::PesClipFamily::kLocomotion)));
  EXPECT_FALSE(pes::PesFamilyPoseMatched(int(pes::PesClipFamily::kLocomotion)));
  EXPECT_TRUE(pes::PesFamilyPoseMatched(int(pes::PesClipFamily::kDodge)));
  EXPECT_TRUE(pes::PesFamilyPoseMatched(int(pes::PesClipFamily::kTackle)));
  EXPECT_FALSE(pes::IsPesClipFamily(int(pes::PesClipFamily::kLocomotion)));
}

// The trap picker names its clips by motion number: the action-7 rows of the action-clip table (pesreceive.hpp).
// Each of those clips must be loaded in the trap family, or the picker would name a clip the engine cannot play.
TEST(PesClipsInstall, TheTrapFamilyHoldsTheClipsTheTrapRowsName) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto receive = pes::LoadReceiveConstants(*exe, vfs, &err);
  ASSERT_TRUE(receive.has_value()) << err;
  auto clips = pes::LoadPesClips(*exe, vfs, &err);
  ASSERT_TRUE(clips.has_value()) << err;
  ASSERT_FALSE(receive->trapRows.empty());
  for (const pes::ActionRow& row : receive->trapRows) {
    const bool loaded = std::any_of(clips->clips.begin(), clips->clips.end(), [&](const pes::PesClip& c) {
      return c.family == pes::PesClipFamily::kTrap && c.motionNo == row.clip;
    });
    EXPECT_TRUE(loaded) << "trap row clip " << row.clip << " is not in the trap family";
  }
}

// A HitData frame counts gani frames at 30 Hz (1/30 s each); the engine's touch frame is 10 ms. So gani frame f is at
// engine frame f * (1/30) / 0.01 = 10f/3, rounded. Hand values: 1 -> 3.33 -> 3; 2 -> 6.67 -> 7; 18 -> 60; 30 -> 100.
TEST(PesTouch, AGaniHitFrameIsTheEngineFrameItLandsOn) {
  EXPECT_EQ(pes::PesTouchAnimFrame(0), 0);
  EXPECT_EQ(pes::PesTouchAnimFrame(1), 3);
  EXPECT_EQ(pes::PesTouchAnimFrame(2), 7);
  EXPECT_EQ(pes::PesTouchAnimFrame(18), 60);
  EXPECT_EQ(pes::PesTouchAnimFrame(30), 100);
}

// Clip 1351 is the first trap row's clip (gkcatch_b00_0_0_y00). Its first HitData record is the install's contact frame.
TEST(PesTouchInstall, TheTrapClipTouchesAtItsHitFrame) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto mbinfo = pes::LoadMbinfo(vfs, *exe, &err);
  ASSERT_TRUE(mbinfo.has_value()) << err;
  const auto& hits = mbinfo->Hits(1351);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0].frame, 7);  // the install's first contact frame for clip 1351 (the first-touch note's 18 is another clip)
  EXPECT_EQ(pes::PesTouchAnimFrame(hits[0].frame), 23);  // 7 gani frames = 0.233 s = 23.3 engine frames, rounded
}
