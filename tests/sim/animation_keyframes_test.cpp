// Keyframe storage of blunted::Animation: one node's keys stay sorted and unique, lookups find exactly the
// stored frames, interpolation mixes the two bracketing keys, and a loaded clip costs one key record per key.
#include <gtest/gtest.h>
#include <malloc.h>

#include <string>
#include <vector>

#include "utils/animation.hpp"

using namespace blunted;

namespace {

// A loaded key costs one record, std::pair<int, KeyFrame>, in a block per node. A glibc malloc header adds 8 B
// to a block, the allowance here. A std::map node costs its record plus a 32 B tree header on top of that.
constexpr size_t kMallocHeaderBytes = 8;
constexpr size_t kMaxHeapBytesPerKeyFrame = sizeof(std::pair<int, KeyFrame>) + kMallocHeaderBytes;

// Keys per node in the bulk clip. Large enough that per-key cost dominates the fixed allocations of a load.
constexpr int kBulkKeyFrames = 20000;

// GF's own clip, assets/gf_anims/deflect/walk/000_kneeheight_holdball_close.anim: its player and body data
// lines verbatim, then the type block that follows the data lines in that file.
constexpr const char* kGfClip =
    "player,0,0.000000,0.015124,-0.214547,9,0.000000,-0.388708,-0.300894,20,0.000000,-0.635105,"
    "-0.182659,35,0.000000,-0.850717,-0.151296,48,0.000000,-0.953742,-0.121877\n"
    "body,0,-0.015707,0.000000,0.000000,0.999877,9,0.062790,0.000000,0.000000,0.998027,20,0.031411,"
    "-0.000000,0.000000,0.999507,48,0.000000,0.000000,0.000000,1.000000\n"
    "<type>\n\tdeflect\n</type>\n";

// Heap bytes in use, including the mmapped blocks a large vector gets (uordblks alone leaves them out).
size_t HeapInUse() {
  const struct mallinfo2 info = mallinfo2();
  return info.uordblks + info.hblkhd;
}

// Frames of one node's keys, in the order the animation stores them.
std::vector<int> Frames(Animation& anim, const std::string& node) {
  std::vector<int> frames;
  for (const NodeAnimation* n : anim.GetNodeAnimations())
    if (n->nodeName == node)
      for (const auto& key : n->animation) frames.push_back(key.first);
  return frames;
}

size_t KeyCount(Animation& anim) {
  size_t count = 0;
  for (const NodeAnimation* n : anim.GetNodeAnimations()) count += n->animation.size();
  return count;
}

// A clip with keyFrames keys on each of two nodes, player and body, in the format LoadText reads.
std::string BulkClipText(int keyFrames) {
  std::string player = "player,0,0,0,0";
  std::string body = "body,0,0,0,0,1";
  for (int f = 1; f < keyFrames; ++f) {
    player += "," + std::to_string(f) + ",0,0,0";
    body += "," + std::to_string(f) + ",0,0,0,1";
  }
  return player + "\n" + body + "\n<type>\n\tdeflect\n</type>\n";
}

TEST(AnimationKeyFrames, KeysStaySortedAndUniqueWhenSetOutOfOrder) {
  Animation anim;
  anim.SetKeyFrame("player", 9, QUATERNION_IDENTITY, Vector3(9, 0, 0));
  anim.SetKeyFrame("player", 3, QUATERNION_IDENTITY, Vector3(3, 0, 0));
  anim.SetKeyFrame("player", 6, QUATERNION_IDENTITY, Vector3(6, 0, 0));
  anim.SetKeyFrame("player", 3, QUATERNION_IDENTITY, Vector3(30, 0, 0));  // overwrites the key at 3
  anim.SetKeyFrame("player", 0, QUATERNION_IDENTITY, Vector3(0, 0, 0));

  EXPECT_EQ(Frames(anim, "player"), (std::vector<int>{0, 3, 6, 9}));
  Quaternion q;
  Vector3 p;
  ASSERT_TRUE(anim.GetKeyFrame("player", 3, q, p, false, true));
  EXPECT_NEAR(p.coords[0], 30.0f, 1e-5f);
}

TEST(AnimationKeyFrames, LookupAndDeleteTouchOnlyTheStoredFrames) {
  Animation anim;
  for (int f : {2, 5, 8}) anim.SetKeyFrame("player", f, QUATERNION_IDENTITY, Vector3(f, 0, 0));

  Quaternion q;
  Vector3 p;
  for (int f : {2, 5, 8}) EXPECT_TRUE(anim.GetKeyFrame("player", f, q, p, false, false)) << f;
  for (int f : {3, 4, 6, 7}) EXPECT_FALSE(anim.GetKeyFrame("player", f, q, p, false, false)) << f;

  anim.DeleteKeyFrame("player", 5);
  EXPECT_FALSE(anim.GetKeyFrame("player", 5, q, p, false, false));
  EXPECT_EQ(Frames(anim, "player"), (std::vector<int>{2, 8}));
}

TEST(AnimationKeyFrames, InterpolatesLinearlyBetweenBracketingKeys) {
  Animation anim;
  anim.SetKeyFrame("player", 2, QUATERNION_IDENTITY, Vector3(0, 0, 0));
  anim.SetKeyFrame("player", 6, QUATERNION_IDENTITY, Vector3(4, 8, 0));
  anim.SetKeyFrame("player", 10, QUATERNION_IDENTITY, Vector3(4, 0, 0));

  Quaternion q;
  Vector3 p;
  // Frame 4 is halfway between keys 2 and 6; frame 8 is halfway between keys 6 and 10.
  anim.GetKeyFrame("player", 4, q, p, false, true);
  EXPECT_NEAR(p.coords[0], 2.0f, 1e-5f);
  EXPECT_NEAR(p.coords[1], 4.0f, 1e-5f);
  anim.GetKeyFrame("player", 8, q, p, false, true);
  EXPECT_NEAR(p.coords[0], 4.0f, 1e-5f);
  EXPECT_NEAR(p.coords[1], 4.0f, 1e-5f);
  EXPECT_NEAR(p.coords[2], 0.0f, 1e-5f);
}

TEST(AnimationKeyFrames, GfClipKeysAreLoadedVerbatimAndInterpolated) {
  Animation anim;
  anim.LoadText("gf_walk", kGfClip);
  EXPECT_EQ(Frames(anim, "player"), (std::vector<int>{0, 9, 20, 35, 48}));

  Quaternion q;
  Vector3 p;
  anim.GetKeyFrame("player", 9, q, p, false, true);
  EXPECT_NEAR(p.coords[1], -0.388708f, 1e-6f);
  EXPECT_NEAR(p.coords[2], -0.300894f, 1e-6f);
  anim.GetKeyFrame("body", 9, q, p, true, false);
  EXPECT_NEAR(q.elements[0], 0.062790f, 1e-6f);
  EXPECT_NEAR(q.elements[3], 0.998027f, 1e-6f);

  // Frame 5 is 5/9 of the way from key 9 back to key 0, so its position mixes the two.
  const float t = 5.0f / 9.0f;
  anim.GetKeyFrame("player", 5, q, p, false, true);
  EXPECT_NEAR(p.coords[1], 0.015124f * (1.0f - t) + -0.388708f * t, 1e-5f);
  EXPECT_NEAR(p.coords[2], -0.214547f * (1.0f - t) + -0.300894f * t, 1e-5f);
}

TEST(AnimationKeyFrames, LoadedClipCostsLessThanMaxHeapBytesPerKeyFrame) {
  const std::string text = BulkClipText(kBulkKeyFrames);
  Animation anim;

  const size_t before = HeapInUse();
  anim.LoadText("bulk", text);
  const size_t after = HeapInUse();

  // Two nodes of kBulkKeyFrames keys each.
  ASSERT_EQ(KeyCount(anim), size_t(2 * kBulkKeyFrames));
  EXPECT_LE((after - before) / KeyCount(anim), kMaxHeapBytesPerKeyFrame);
}

}  // namespace
