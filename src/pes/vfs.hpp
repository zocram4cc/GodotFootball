#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "pes/cpk.hpp"

namespace pes {

// The PES 2021 file system as the game sees it: Data/ cpks, then download/
// cpks in DpFileList.bin order, then sider live-cpk roots. On equal paths the
// later source wins. Paths are case-insensitive, '/'-separated, no leading '/'.
class PesVfs {
 public:
  bool Mount(const std::string& root, std::string* error);
  bool Exists(const std::string& path) const;
  std::optional<uint64_t> Size(const std::string& path) const;  // decompressed size
  std::optional<std::vector<uint8_t>> Read(const std::string& path, std::string* error) const;
  // The copy of `path` inside the mounted archive named `archive` (its file name, e.g. "dt18_all.cpk"),
  // not the winner. Fails with a reason when no such archive is mounted or it does not hold the path.
  std::optional<std::vector<uint8_t>> ReadFrom(const std::string& archive, const std::string& path,
                                               std::string* error) const;
  // The file name of the archive that holds the winning copy of `path` (folded, e.g. "dt18_all.cpk");
  // empty for a loose live file or an absent path.
  std::string SourceOf(const std::string& path) const;
  // Original-case paths of every file whose folded path starts with the folded prefix.
  std::vector<std::string> List(const std::string& prefix) const;
  const std::string& MountReport() const { return report_; }
  const std::string& Root() const { return root_; }

  static std::string Fold(const std::string& path);

 private:
  static constexpr int32_t LOOSE = -1;
  struct Source {
    int32_t archive;  // index into archives_, or LOOSE
    uint32_t entry;   // entry index in that archive, or index into loose_
  };
  void Add(const std::string& path, Source source);
  void MountCpk(const std::string& file, const std::string& label);
  void MountLooseRoot(const std::string& dir);
  const std::string& OriginalPath(const Source& s) const;

  std::string root_, report_;
  std::vector<CpkArchive> archives_;
  std::vector<std::string> archiveNames_;  // folded file name of each archive, parallel to archives_
  std::vector<std::string> loose_;  // root-relative original path
  std::vector<std::string> looseRoots_;
  std::vector<uint32_t> looseRootOf_;
  std::unordered_map<std::string, Source> index_;
};

// The archive every constant file is read from: the stock base cpk. A download/ or sider overlay (the
// 4cc gameplay pack overrides constant_team/match/player.bin) never reaches a constant loader.
inline constexpr const char* kStockArchive = "dt18_all.cpk";
// The stock copy of a constant file. Every constant_*.bin loader reads through this one helper.
std::optional<std::vector<uint8_t>> ReadStockConstant(const PesVfs& vfs, const std::string& path,
                                                      std::string* error);

}  // namespace pes
