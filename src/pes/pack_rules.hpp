#pragma once

// PES face-pack conventions: which meshes draw, and where an fmdl's textures
// live in the VFS. Ported 1:1 from GameplayFootball fmdl_to_fullbody.py.

#include <string>
#include <vector>

#include "pes/fmdl.hpp"
#include "pes/vfs.hpp"

namespace pes {

// A texture whose alpha never exceeds this hides its mesh (GF measured: every
// DBG-pack hider peaks at alpha 2; the nearest real art sits at 38).
inline constexpr int HIDER_MAX_ALPHA = 8;

std::string FacePackPath(int faceId);
std::string FaceTextureDir(int faceId);  // the face folder's sourceimages/#windx11/
std::string TextureStem(const std::string& file);  // file name without directory or extension

// Extra passes PES draws (antiblur, outline shell), matched on whole words.
bool IsNonRenderPass(const std::string& material, const std::string& baseTextureStem);
bool IsEffectTexture(const std::string& stem);       // aura, effect, fx_, smoke, flare
bool IsKitSlotTexture(const std::string& stem);      // dummy_kit, or the bare u0<team>p0
bool IsPlaceholderTexture(const std::string& stem);  // dummy*

// VFS path of an fmdl texture, or "" if none: the stored directory mapped into
// the VFS (/Assets/pes16/ -> Asset/, + #windx11/, .ftex), then the face pack's
// own sourceimages. Nothing else: a missing directory leaves the texture unbound,
// as PES does, rather than borrowing another pack's file of the same stem.
class TextureResolver {
 public:
  TextureResolver(const PesVfs& vfs, int faceId) : vfs_(vfs), faceId_(faceId) {}
  std::string Resolve(const FmdlTexture& texture);

 private:
  const PesVfs& vfs_;
  int faceId_;
};

}  // namespace pes
