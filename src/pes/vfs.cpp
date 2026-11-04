#include "pes/vfs.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <tuple>

namespace fs = std::filesystem;

namespace pes {
namespace {

// DpFileList.bin: u32 0, u32 count, 8 bytes pad, then 48-byte records whose
// first bytes are a NUL-terminated cpk name.
constexpr size_t DPFILELIST_COUNT_OFFSET = 4;
constexpr size_t DPFILELIST_RECORDS_OFFSET = 16;
constexpr size_t DPFILELIST_RECORD_BYTES = 48;
// Data/ archives; skips "*.bak", "* - Copia.cpk" and other leftovers.
const std::regex DATA_CPK_NAME(R"(^dt(\d\d)_[A-Za-z0-9_]+\.cpk$)");
const std::regex WINDOWS_ABSOLUTE(R"(^[A-Za-z]:)");

std::optional<std::string> Slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return std::nullopt;
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string Trim(std::string s) {
  const char* ws = " \t\r\n";
  s.erase(0, s.find_first_not_of(ws));
  s.erase(s.find_last_not_of(ws) + 1);
  return s;
}

// ---- sider Lua modules: what the resolution reads out of their source ----

// A module that registers for one of the events that decide which file the
// game opens (commented-out registrations do not count).
const std::regex LIVECPK_EVENT(R"RE(register\(\s*"livecpk_(make_key|get_filepath|rewrite)")RE");
const std::regex LUA_LINE_COMMENT(R"(--[^\n]*)");

// The value of a Lua string literal: "\\" is one backslash.
std::string LuaUnescape(const std::string& lit) {
  std::string out;
  for (size_t i = 0; i < lit.size(); ++i) out.push_back(lit[i] == '\\' && i + 1 < lit.size() ? lit[++i] : lit[i]);
  return out;
}

// `local <name> = "<literal>"` in a module's source.
std::optional<std::string> LuaStringVar(const std::string& src, const std::string& name) {
  std::smatch m;
  const std::regex re("\\blocal\\s+" + name + R"RE(\s*=\s*"((?:[^"\\]|\\.)*)")RE");
  if (!std::regex_search(src, m, re)) return std::nullopt;
  return LuaUnescape(m[1]);
}

// ".\\content\\netblock\\" -> "content/netblock", relative to the install root.
std::string SiderRelDir(std::string s) {
  std::replace(s.begin(), s.end(), '\\', '/');
  if (s.starts_with("./")) s.erase(0, 2);
  while (s.ends_with('/')) s.pop_back();
  return s;
}

// The `string.match(filename, "<pattern>")` calls inside `function isGameplayFile`
// of GameplayLoader.lua, as case-insensitive regexes (disk names carry their own
// case). Lua patterns here are literals with '.' as the wildcard; any other Lua
// magic character makes the whole list unreadable (nullopt).
// ponytail: case-insensitive where Lua's match is case-sensitive; PES asks for
// these paths in the case the files are stored in, so it only differs for a mod
// that ships a differently-cased twin. Upgrade: match on the requested name.
std::optional<std::vector<std::regex>> GameplayFilePatterns(const std::string& src) {
  const auto fn = src.find("function isGameplayFile");
  if (fn == std::string::npos) return std::nullopt;
  const auto end = src.find("\nend", fn);
  const std::string body = src.substr(fn, end == std::string::npos ? std::string::npos : end - fn);
  const std::regex call(R"RE(string\.match\(\s*filename\s*,\s*"((?:[^"\\]|\\.)*)"\s*\))RE");
  std::vector<std::regex> out;
  for (auto it = std::sregex_iterator(body.begin(), body.end(), call); it != std::sregex_iterator(); ++it) {
    std::string re;
    for (char c : LuaUnescape((*it)[1])) {
      if (std::strchr("%^$()[]*+-?", c)) return std::nullopt;
      if (c != '.' && c != '_' && !std::isalnum(static_cast<unsigned char>(c))) re.push_back('\\');
      re.push_back(c);
    }
    out.emplace_back(re, std::regex::icase);
  }
  if (out.empty()) return std::nullopt;
  return out;
}

std::vector<std::string> Lines(const std::string& text) {
  std::istringstream in(text);
  std::vector<std::string> out;
  for (std::string l; std::getline(in, l);) {
    if (!l.empty() && l.back() == '\r') l.pop_back();
    out.push_back(l);
  }
  return out;
}

}  // namespace

std::string PesVfs::Fold(const std::string& path) {
  std::string out;
  out.reserve(path.size());
  for (char c : path) out.push_back(c == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(c))));
  out.erase(0, out.find_first_not_of('/'));
  return out;
}

void PesVfs::Add(const std::string& path, Source source) { index_[Fold(path)] = source; }

void PesVfs::MountCpk(const std::string& file, const std::string& label) {
  CpkArchive cpk;
  std::string err;
  if (!cpk.Open(file, &err)) {
    report_ += label + "  FAILED: " + err + "\n";
    return;
  }
  const auto id = int32_t(archives_.size());
  for (uint32_t i = 0; i < cpk.Entries().size(); ++i) Add(cpk.Entries()[i].path, {id, i});
  report_ += label + "  " + std::to_string(cpk.Entries().size()) + " entries\n";
  // Whole cpks stored inside this one (Data/dt30_g4.cpk: Asset/model/bg/stNNN.cpk)
  // mount right after it, read in place: above their parent, below every later
  // mount, so download overrides still win (docs/re/pes-re-stadium-camera.md 1.1).
  std::vector<CpkEntry> nested;
  for (const auto& e : cpk.Entries())
    if (e.size == e.extractSize && Fold(e.path).ends_with(".cpk")) nested.push_back(e);
  archives_.push_back(std::move(cpk));
  for (const auto& e : nested) {
    CpkArchive inner;
    if (!inner.Open(file, &err, e.offset, e.size)) {
      report_ += label + " > " + e.path + "  FAILED: " + err + "\n";
      continue;
    }
    const auto innerId = int32_t(archives_.size());
    for (uint32_t i = 0; i < inner.Entries().size(); ++i) Add(inner.Entries()[i].path, {innerId, i});
    report_ += label + " > " + e.path + "  " + std::to_string(inner.Entries().size()) + " entries\n";
    archives_.push_back(std::move(inner));
  }
}

void PesVfs::MountLooseRoot(const std::string& dir, const std::function<bool(const std::string&)>& accept,
                            const std::string& label) {
  const auto rootId = uint32_t(looseRoots_.size());
  looseRoots_.push_back(dir);
  size_t count = 0;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator();
       it.increment(ec)) {
    if (!it->is_regular_file()) continue;
    const std::string rel = fs::relative(it->path(), dir).generic_string();
    if (accept && !accept(rel)) continue;
    Add(rel, {LOOSE, uint32_t(loose_.size())});
    loose_.push_back(rel);
    looseRootOf_.push_back(rootId);
    ++count;
  }
  report_ += label + " " + fs::relative(dir, root_).generic_string() + "  " + std::to_string(count) + " files\n";
}

bool PesVfs::MountModules(const std::vector<std::string>& modules, const VfsOptions& options, std::string* error) {
  const fs::path rootPath(root_);
  // sider asks the modules in listed order and takes the first file that exists: mount in reverse.
  for (auto it = modules.rbegin(); it != modules.rend(); ++it) {
    const std::string& name = *it;
    const auto raw = Slurp(rootPath / "modules" / name);
    if (!raw) {
      report_ += "module " + name + "  absent\n";
      continue;
    }
    const std::string src = std::regex_replace(*raw, LUA_LINE_COMMENT, "");
    const std::string label = "module " + name;
    if (name == "netblock.lua") {
      // key = the requested name: serves content_root/<name> when that file exists.
      const auto dir = LuaStringVar(src, "content_root");
      if (dir && fs::is_directory(rootPath / SiderRelDir(*dir))) MountLooseRoot((rootPath / SiderRelDir(*dir)).string(), {}, label);
      else report_ += label + "  content folder not found\n";
    } else if (name == "GameplayLoader.lua") {
      const auto content = LuaStringVar(src, "contentPath");
      const auto patterns = GameplayFilePatterns(src);
      if (!content || !patterns) {
        report_ += label + "  UNMODELLED: contentPath or isGameplayFile not readable\n";
        continue;
      }
      const fs::path base = rootPath / SiderRelDir(*content);
      // init(): the list is "No Mod" + one line per mod.
      std::vector<std::string> mods{"No Mod"};
      if (const auto csv = Slurp(base / "gameplay-mods.csv")) {
        for (const std::string& l : Lines(*csv)) {
          if (l.starts_with('#')) continue;
          // Lua: l:split(";") keeps the non-empty runs; a line with exactly one is a mod name.
          std::vector<std::string> fields;
          std::istringstream parts(l);
          for (std::string f; std::getline(parts, f, ';');)
            if (!f.empty()) fields.push_back(f);
          if (fields.size() == 1) mods.push_back(fields[0]);
        }
      }
      const auto listed = [&](const std::string& mod) { return std::find(mods.begin(), mods.end(), mod) != mods.end(); };
      if (options.gameplayMod && !listed(*options.gameplayMod)) {
        *error = "gameplay mod \"" + *options.gameplayMod + "\" is not in " + (base / "gameplay-mods.csv").string();
        return false;
      }
      // Unnamed = "No Mod" (stock PES): config.ini's favorite is the loader's own boot state, not a GF input.
      gameplayMod_ = options.gameplayMod.value_or("No Mod");
      if (gameplayMod_ == "No Mod") {
        report_ += label + "  No Mod selected\n";
        continue;
      }
      const fs::path dir = base / gameplayMod_;
      if (!fs::is_directory(dir)) {
        report_ += label + "  mod folder " + gameplayMod_ + " absent\n";
        continue;
      }
      // livecpk_make_key keys only the files isGameplayFile accepts; the match is on the backslash path.
      MountLooseRoot(dir.string(), [&](const std::string& rel) {
        std::string win = rel;
        std::replace(win.begin(), win.end(), '/', '\\');
        return std::any_of(patterns->begin(), patterns->end(), [&](const std::regex& p) { return std::regex_search(win, p); });
      }, label + " (" + gameplayMod_ + ")");
    } else if (std::regex_search(src, LIVECPK_EVENT)) {
      report_ += label + "  UNMODELLED: registers livecpk events\n";
    }
  }
  return true;
}

bool PesVfs::Mount(const std::string& root, std::string* error, const VfsOptions& options) {
  root_ = root;
  if (options.gameplayMod && !options.sider) {
    *error = "gameplayMod needs VfsOptions::sider: GameplayLoader.lua is a sider module";
    return false;
  }
  const fs::path rootPath(root);
  if (!fs::is_directory(rootPath / "Data")) {
    *error = "no Data/ folder under the PES21 root";
    return false;
  }

  // 1. Data/: by number, then the _4K_ variant after its base so 4K wins, then name.
  std::vector<std::tuple<int, bool, std::string>> data;
  for (const auto& e : fs::directory_iterator(rootPath / "Data")) {
    const std::string name = e.path().filename().string();
    std::smatch m;
    if (e.is_regular_file() && std::regex_match(name, m, DATA_CPK_NAME))
      data.emplace_back(std::stoi(m[1]), name.find("_4K_") != std::string::npos, name);
  }
  std::sort(data.begin(), data.end());
  for (const auto& [num, is4k, name] : data) MountCpk((rootPath / "Data" / name).string(), "data " + name);

  // 2. download/ in DpFileList.bin order.
  const fs::path dl = rootPath / "download";
  if (auto list = Slurp(dl / "DpFileList.bin");
      list && list->size() >= DPFILELIST_RECORDS_OFFSET) {
    uint32_t count;
    std::memcpy(&count, list->data() + DPFILELIST_COUNT_OFFSET, 4);
    for (uint32_t i = 0; i < count; ++i) {
      const size_t at = DPFILELIST_RECORDS_OFFSET + size_t(i) * DPFILELIST_RECORD_BYTES;
      if (at + DPFILELIST_RECORD_BYTES > list->size()) {
        report_ += "download DpFileList.bin truncated at record " + std::to_string(i) + "\n";
        break;
      }
      const std::string name(list->c_str() + at, strnlen(list->c_str() + at, DPFILELIST_RECORD_BYTES));
      if (fs::is_regular_file(dl / name)) MountCpk((dl / name).string(), "download " + name);
      else report_ += "download " + name + "  listed but absent\n";
    }
  } else {
    report_ += "download  no DpFileList.bin, skipped\n";
  }

  // 3. sider's live layer, opt-in (VfsOptions::sider). Without it GF resolves the game's own cpks alone, as stock PES does.
  if (!options.sider) {
    report_ += "sider  not mounted: stock PES (VfsOptions::sider opts in)\n";
  } else {
    // livecpk.enabled gates everything below (it installs the hooks); Lua modules also need lua.enabled.
    // Later settings override earlier ones.
    std::vector<std::string> roots, modules;
    bool livecpk = false, lua = false;
    if (auto ini = Slurp(rootPath / "sider.ini")) {
      for (std::string line : Lines(*ini)) {
        line = Trim(line.substr(0, line.find(';')));
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));
        if (key == "livecpk.enabled") livecpk = value == "1";
        if (key == "lua.enabled") lua = value == "1";
        if (key != "cpk.root" && key != "lua.module") continue;
        value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
        (key == "cpk.root" ? roots : modules).push_back(value);
      }
    }
    if (livecpk) {
      // sider's cpk.root folders: the first listed root that has the file wins, so mount in reverse.
      for (auto it = roots.rbegin(); it != roots.rend(); ++it) {
        std::string r = *it;
        if (std::regex_search(r, WINDOWS_ABSOLUTE)) {
          report_ += "sider root is an absolute Windows path, skipped\n";
          continue;
        }
        std::replace(r.begin(), r.end(), '\\', '/');
        const fs::path dir = (rootPath / r).lexically_normal();
        if (fs::is_directory(dir)) MountLooseRoot(dir.string());
        else report_ += "sider root " + r + "  absent\n";
      }
      // The Lua modules are asked before the roots (sider have_content, then have_live_file).
      if (lua && !MountModules(modules, options, error)) return false;
    }
  }
  report_ += "total " + std::to_string(index_.size()) + " unique paths\n";
  return true;
}

bool PesVfs::Exists(const std::string& path) const { return index_.count(Fold(path)) != 0; }

std::optional<uint64_t> PesVfs::Size(const std::string& path) const {
  auto it = index_.find(Fold(path));
  if (it == index_.end()) return std::nullopt;
  const Source& s = it->second;
  if (s.archive != LOOSE) return archives_[s.archive].Entries()[s.entry].extractSize;
  std::error_code ec;
  const auto size = fs::file_size(fs::path(looseRoots_[looseRootOf_[s.entry]]) / loose_[s.entry], ec);
  return ec ? std::nullopt : std::optional<uint64_t>(size);
}

const std::string& PesVfs::OriginalPath(const Source& s) const {
  return s.archive == LOOSE ? loose_[s.entry] : archives_[s.archive].Entries()[s.entry].path;
}

std::optional<std::vector<uint8_t>> PesVfs::Read(const std::string& path, std::string* error) const {
  auto it = index_.find(Fold(path));
  if (it == index_.end()) {
    *error = "not in PES VFS: " + path;
    return std::nullopt;
  }
  const Source& s = it->second;
  if (s.archive != LOOSE) return archives_[s.archive].Read(archives_[s.archive].Entries()[s.entry], error);
  auto bytes = Slurp(fs::path(looseRoots_[looseRootOf_[s.entry]]) / loose_[s.entry]);
  if (!bytes) {
    *error = "cannot read live file " + path;
    return std::nullopt;
  }
  return std::vector<uint8_t>(bytes->begin(), bytes->end());
}

std::vector<std::string> PesVfs::List(const std::string& prefix) const {
  const std::string p = Fold(prefix);
  std::vector<std::string> out;
  for (const auto& [key, src] : index_)
    if (key.compare(0, p.size(), p) == 0) out.push_back(OriginalPath(src));
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace pes
