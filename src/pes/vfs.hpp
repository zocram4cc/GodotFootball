#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "pes/cpk.hpp"

namespace pes {

// The PES 2021 file system as the game sees it: the game's own cpks, Data/ then download/ (the later cpk
// winning on equal paths, so download/ beats Data/ as PES does). With VfsOptions::sider, sider's live layer
// answers first: the Lua modules of sider.ini (first listed module that has the file wins), then its cpk.root
// folders (first listed wins). Paths are case-insensitive, '/'-separated, no leading '/'.
struct VfsOptions {
  // sider's live layer. Off by default: GF resolves files as stock PES does (owner ruling 30-10-26). Tools that
  // must see what sider serves opt in.
  bool sider = false;
  // The gameplay mod GameplayLoader.lua answers for: a sider selection, so it needs sider. PES re-rolls its
  // choice at every match when is_random_gameplay_mod_enabled is set (os.clock seeded), so GF takes it as an
  // input. Unset = "No Mod": the loader answers nothing and the game's own cpks decide.
  std::optional<std::string> gameplayMod;
};

class PesVfs {
 public:
  bool Mount(const std::string& root, std::string* error, const VfsOptions& options = {});
  bool Exists(const std::string& path) const;
  std::optional<uint64_t> Size(const std::string& path) const;  // decompressed size
  std::optional<std::vector<uint8_t>> Read(const std::string& path, std::string* error) const;
  // Original-case paths of every file whose folded path starts with the folded prefix.
  std::vector<std::string> List(const std::string& prefix) const;
  const std::string& MountReport() const { return report_; }
  const std::string& Root() const { return root_; }
  // The gameplay mod GameplayLoader.lua has selected for this mount ("No Mod" = none).
  const std::string& GameplayMod() const { return gameplayMod_; }

  static std::string Fold(const std::string& path);

 private:
  static constexpr int32_t LOOSE = -1;
  struct Source {
    int32_t archive;  // index into archives_, or LOOSE
    uint32_t entry;   // entry index in that archive, or index into loose_
  };
  void Add(const std::string& path, Source source);
  void MountCpk(const std::string& file, const std::string& label);
  // `accept` (optional) keeps a file only when it returns true for the root-relative path.
  void MountLooseRoot(const std::string& dir, const std::function<bool(const std::string&)>& accept = {},
                      const std::string& label = "sider");
  // sider.ini modules, listed order; mounts so the first listed answers first.
  bool MountModules(const std::vector<std::string>& modules, const VfsOptions& options, std::string* error);
  const std::string& OriginalPath(const Source& s) const;

  std::string root_, report_, gameplayMod_ = "No Mod";
  std::vector<CpkArchive> archives_;
  std::vector<std::string> loose_;  // root-relative original path
  std::vector<std::string> looseRoots_;
  std::vector<uint32_t> looseRootOf_;
  std::unordered_map<std::string, Source> index_;
};

}  // namespace pes
