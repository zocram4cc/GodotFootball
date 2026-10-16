#include "gdext/pes_motion.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace godot {
namespace {

Quaternion Q(const pesrig::Quat& q) { return Quaternion(q.x, q.y, q.z, q.w); }
pesrig::Quat P(const Quaternion& q) { return {q.x, q.y, q.z, q.w}; }

// The pose `t` of the way from `a` to `b`: slerp per joint and on the root, lerp on the root's travel.
pesrig::ClipPose Blend(const pesrig::ClipPose& a, const pesrig::ClipPose& b, float t) {
  pesrig::ClipPose out = a;
  for (size_t j = 0; j < out.local.size() && j < b.local.size(); ++j)
    out.local[j] = P(Q(a.local[j]).slerp(Q(b.local[j]), t));
  out.bodyRotation = P(Q(a.bodyRotation).slerp(Q(b.bodyRotation), t));
  out.bodyPosition.x += (b.bodyPosition.x - a.bodyPosition.x) * t;
  out.bodyPosition.y += (b.bodyPosition.y - a.bodyPosition.y) * t;
  out.bodyPosition.z += (b.bodyPosition.z - a.bodyPosition.z) * t;
  return out;
}

int IntOf(const Dictionary& d, const char* key, int fallback) { return int(int64_t(d.get(key, fallback))); }
double RealOf(const Dictionary& d, const char* key, double fallback) { return double(d.get(key, fallback)); }

}  // namespace

void PesMotionPlayer::_bind_methods() {
  ClassDB::bind_method(D_METHOD("load", "library"), &PesMotionPlayer::load);
  ClassDB::bind_method(D_METHOD("update", "index", "skeleton", "clip", "delta_seconds"), &PesMotionPlayer::update);
}

Error PesMotionPlayer::load(const Ref<PesLibrary>& library) {
  if (library.is_null() || !library->motion_names() || !library->rig()) {
    UtilityFunctions::push_error("PesMotionPlayer: library lacks the motion table or the rig");
    return ERR_UNCONFIGURED;
  }
  std::string err;
  auto table = pes::LoadBlendTable(library->vfs(), &err);
  if (!table) {
    UtilityFunctions::push_error("PesMotionPlayer: ", ToGd(err));
    return ERR_FILE_NOT_FOUND;
  }
  library_ = library;
  blend_ = std::move(*table);
  clips_.clear();
  groups_.clear();
  tracks_.clear();
  return OK;
}

PesMotionPlayer::Command PesMotionPlayer::CommandOf(const Dictionary& clip) {
  Command c;
  c.motion = IntOf(clip, "motion", -1);
  c.group = IntOf(clip, "group", -1);
  c.mirror = bool(clip.get("mirror", false));
  c.startTime = RealOf(clip, "start_time", 0.0);
  c.startRate = RealOf(clip, "start_rate", 0.0);
  c.rate = RealOf(clip, "rate", 0.0);
  c.duration = RealOf(clip, "duration", 0.0);
  return c;
}

Ref<PesAnimation> PesMotionPlayer::ClipFor(int motion) {
  auto it = clips_.find(motion);
  if (it != clips_.end()) return it->second;
  const auto* names = library_->motion_names();
  if (!names || motion < 0 || size_t(motion) >= names->size()) return Ref<PesAnimation>();
  Ref<PesAnimation> clip;
  clip.instantiate();
  if (clip->load(library_, ToGd((*names)[size_t(motion)])) != OK) return Ref<PesAnimation>();
  clips_[motion] = clip;
  return clip;
}

const PesMotionPlayer::Group& PesMotionPlayer::GroupOf(int group) {
  auto it = groups_.find(group);
  if (it == groups_.end()) {
    Group g;
    g.members = pes::BlendGroup(blend_, group);
    for (const auto& m : g.members) g.params.push_back(m.param);
    it = groups_.emplace(group, std::move(g)).first;
  }
  return it->second;
}

bool PesMotionPlayer::Members(const Command& c, pes::Bracketed& b, Ref<PesAnimation>& lo, Ref<PesAnimation>& hi) {
  const Group& g = GroupOf(c.group);
  if (g.members.empty()) return false;
  b = pes::Bracket(g.params, float(c.rate));
  lo = ClipFor(g.members[size_t(b.lo)].motionNo);
  hi = ClipFor(g.members[size_t(b.hi)].motionNo);
  return lo.is_valid() && hi.is_valid();
}

bool PesMotionPlayer::Sample(const Layer& layer, Sampled& out) {
  const Command& c = layer.command;
  out.mirror = c.mirror;
  if (c.group < 0) {  // SetMotion: one clip at its clock, in seconds
    out.clip = ClipFor(c.motion);
    if (out.clip.is_null()) return false;
    out.handTime = layer.clock;
    out.pose = out.clip->Pose(layer.clock, c.mirror);
    return true;
  }
  // SetParametric: the two members around the rate, all at one normalised phase.
  pes::Bracketed b;
  Ref<PesAnimation> lo, hi;
  if (!Members(c, b, lo, hi)) return false;
  const double tl = layer.clock * lo->length_seconds(), th = layer.clock * hi->length_seconds();
  out.pose = Blend(lo->Pose(tl, c.mirror), hi->Pose(th, c.mirror), b.t);
  const bool loLeads = b.t < 0.5f;  // the hands follow the heavier member
  out.clip = loLeads ? lo : hi;
  out.handTime = loLeads ? tl : th;
  return true;
}

void PesMotionPlayer::Advance(Layer& layer, double seconds) {
  if (layer.command.group < 0) {
    layer.clock += seconds;
    return;
  }
  pes::Bracketed b;
  Ref<PesAnimation> lo, hi;
  if (!Members(layer.command, b, lo, hi)) return;
  // One phase for all members: a cycle is the bracketed members' lengths blended.
  const double cycle = lo->length_seconds() + (hi->length_seconds() - lo->length_seconds()) * b.t;
  if (cycle > 0.0) layer.clock = std::fmod(layer.clock + seconds / cycle, 1.0);
}

Error PesMotionPlayer::update(int index, Skeleton3D* skeleton, const Dictionary& clip, double delta_seconds) {
  if (library_.is_null() || !skeleton) return ERR_UNCONFIGURED;
  if (index < 0) return ERR_INVALID_PARAMETER;
  if (size_t(index) >= tracks_.size()) tracks_.resize(size_t(index) + 1);
  Track& track = tracks_[size_t(index)];
  const uint32_t serial = uint32_t(int64_t(clip.get("serial", 0)));
  track.playback = RealOf(clip, "playback", 1.0);
  if (serial != 0 && serial != track.serial) {
    // A new issue is a new clip in PES. The clip it replaces is the crossfade's outgoing clip.
    if (track.issued) {
      track.previous = track.current;
      track.hasPrevious = true;
    }
    track.current.command = CommandOf(clip);
    track.current.clock = track.current.command.group < 0 ? track.current.command.startTime
                                                          : track.current.command.startRate;
    track.blendTotal = std::max(0.0, track.current.command.duration);
    track.blendElapsed = 0.0;
    track.serial = serial;
    track.issued = true;
  }
  if (!track.issued) return OK;

  // The clock advances by the playback rate PES keeps on the player, once per tick.
  const double seconds = delta_seconds * track.playback;
  Advance(track.current, seconds);
  if (track.hasPrevious) {
    Advance(track.previous, seconds);
    track.blendElapsed += delta_seconds;
    if (track.blendElapsed >= track.blendTotal) track.hasPrevious = false;
  }

  Sampled now;
  if (!Sample(track.current, now)) return ERR_INVALID_DATA;
  Sampled before;
  if (track.hasPrevious && Sample(track.previous, before)) {
    const float w = float(track.blendElapsed / track.blendTotal);
    const Sampled& lead = w < 0.5f ? before : now;
    lead.clip->Write(skeleton, Blend(before.pose, now.pose, w), true, lead.handTime);
  } else {
    now.clip->Write(skeleton, now.pose, true, now.handTime);
  }
  return OK;
}

}  // namespace godot
