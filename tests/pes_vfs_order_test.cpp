// The order PES resolves a file in: sider's Lua modules (livecpk_make_key /
// livecpk_get_filepath, in sider.ini module order), then sider's cpk.root
// folders (first listed wins), then the game's own cpks. Synthetic installs
// pin the rule; the install tests at the bottom check it against the owner's
// files and the live process (docs/ports/pes-port-VfsOrder.md).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "pes/cpk.hpp"
#include "pes/uniform.hpp"
#include "pes/vfs.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* CONSTANT_PLAYER = "common/match/constant/constant_player.bin";

// The two sider modules of the owner's install that answer livecpk events,
// reduced to what the resolution reads from them.
const std::string kLoaderLua = R"(
local contentPath  = ".\\content\\gameplay-loader"
function isGameplayFile(filename)
    if      string.match(filename, "common\\match\\constant\\constant_player.bin")          or
            string.match(filename, "common\\match\\constant\\constant_positionCK.bin")      or
            string.match(filename, "common\\anime\\")
    then
        return true
    else
        return false
    end
end
function livecpk_make_key(ctx, filename) end
ctx.register("livecpk_make_key", livecpk_make_key)
ctx.register("livecpk_get_filepath", livecpk_get_filepath)
)";
const std::string kNetblockLua = R"(
local content_root = ".\\content\\netblock\\"
ctx.register("livecpk_get_filepath", m.get_filepath)
)";

class SyntheticInstall {
 public:
  explicit SyntheticInstall(const std::string& name) : root_(fs::current_path() / ("vfs_order_" + name)) {
    fs::remove_all(root_);
    fs::create_directories(root_ / "Data");
  }
  ~SyntheticInstall() { fs::remove_all(root_); }
  void Put(const std::string& rel, const std::string& bytes) {
    fs::create_directories((root_ / rel).parent_path());
    std::ofstream(root_ / rel, std::ios::binary) << bytes;
  }
  // modules: sider.ini lua.module lines, in order ("" entries are skipped).
  void Sider(const std::string& roots, const std::string& modules, const std::string& extra = "") {
    Put("sider.ini", "[sider]\nlivecpk.enabled = 1\nlua.enabled = 1\n" + extra + roots + modules);
  }
  void LoaderState(const std::string& favorite, const std::string& mods = "No Mod\nmodA\nmodB\n") {
    Put("modules/GameplayLoader.lua", kLoaderLua);
    Put("content/gameplay-loader/gameplay-mods.csv", mods);
    Put("content/gameplay-loader/config.ini", "favorite_gameplay_mod = " + favorite + "\n");
  }
  std::string Root() const { return root_.string(); }

 private:
  fs::path root_;
};

std::string ReadStr(const pes::PesVfs& vfs, const char* path) {
  std::string err;
  auto b = vfs.Read(path, &err);
  return b ? std::string(b->begin(), b->end()) : "<" + err + ">";
}

const std::string kTwoRoots =
    "cpk.root = \".\\livecpk\\a\"\ncpk.root = \".\\livecpk\\b\"\n";

}  // namespace

// A gameplay mod the loader selects beats every cpk.root: the loader's
// livecpk_get_filepath answers before sider looks at the roots.
TEST(VfsOrder, LoaderModBeatsLiveRoots) {
  SyntheticInstall in("modbeatsroot");
  in.Sider(kTwoRoots, "lua.module = \"GameplayLoader.lua\"\n");
  in.LoaderState("modA");
  in.Put("livecpk/a/common/match/constant/constant_player.bin", "ROOT_A");
  in.Put("content/gameplay-loader/modA/common/match/constant/constant_player.bin", "MODA");
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(in.Root(), &err)) << err;
  EXPECT_EQ(ReadStr(vfs, CONSTANT_PLAYER), "MODA");
}

// "No Mod" selected: the loader answers nothing, the first listed root wins.
TEST(VfsOrder, NoModFallsThroughToFirstListedRoot) {
  SyntheticInstall in("nomod");
  in.Sider(kTwoRoots, "lua.module = \"GameplayLoader.lua\"\n");
  in.LoaderState("");
  in.Put("livecpk/a/common/match/constant/constant_player.bin", "ROOT_A");
  in.Put("livecpk/b/common/match/constant/constant_player.bin", "ROOT_B");
  in.Put("content/gameplay-loader/modA/common/match/constant/constant_player.bin", "MODA");
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(in.Root(), &err)) << err;
  EXPECT_EQ(ReadStr(vfs, CONSTANT_PLAYER), "ROOT_A");
}

// The default mount is stock PES: no gameplay mod answers unless one is named, so
// config.ini's favorite is not the selection and the first listed root wins.
TEST(VfsOrder, DefaultMountIgnoresTheBootFavorite) {
  SyntheticInstall in("defaultnomod");
  in.Sider(kTwoRoots, "lua.module = \"GameplayLoader.lua\"\n");
  in.LoaderState("modA");
  in.Put("livecpk/a/common/match/constant/constant_player.bin", "ROOT_A");
  in.Put("content/gameplay-loader/modA/common/match/constant/constant_player.bin", "MODA");
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(in.Root(), &err)) << err;
  EXPECT_EQ(vfs.GameplayMod(), "No Mod");
  EXPECT_EQ(ReadStr(vfs, CONSTANT_PLAYER), "ROOT_A");
}

// The loader only serves the files its isGameplayFile accepts: anything else in
// the mod folder is invisible to the game.
TEST(VfsOrder, LoaderServesOnlyWhatIsGameplayFileAccepts) {
  SyntheticInstall in("onlygameplay");
  in.Sider("", "lua.module = \"GameplayLoader.lua\"\n");
  in.LoaderState("modA");
  in.Put("content/gameplay-loader/modA/common/match/constant/constant_positionCK.bin", "CK");
  in.Put("content/gameplay-loader/modA/common/anime/FoxAnim/Body/x.mtar", "ANIME");
  in.Put("content/gameplay-loader/modA/common/render/thumbnail/y.dds", "NOT_GAMEPLAY");
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(in.Root(), &err)) << err;
  EXPECT_EQ(ReadStr(vfs, "common/match/constant/constant_positionCK.bin"), "CK");
  EXPECT_EQ(ReadStr(vfs, "common/anime/FoxAnim/Body/x.mtar"), "ANIME");
  EXPECT_FALSE(vfs.Exists("common/render/thumbnail/y.dds"));
}

// Modules answer in sider.ini order, the first answer wins.
TEST(VfsOrder, ModulesAnswerInSiderIniOrder) {
  for (const bool netblockFirst : {true, false}) {
    SyntheticInstall in(netblockFirst ? "netfirst" : "loaderfirst");
    const std::string net = "lua.module = \"netblock.lua\"\n", ldr = "lua.module = \"GameplayLoader.lua\"\n";
    in.Sider("", netblockFirst ? net + ldr : ldr + net);
    in.LoaderState("modA");
    in.Put("modules/netblock.lua", kNetblockLua);
    in.Put("content/netblock/common/match/constant/constant_player.bin", "NETBLOCK");
    in.Put("content/gameplay-loader/modA/common/match/constant/constant_player.bin", "MODA");
    pes::PesVfs vfs;
    std::string err;
    ASSERT_TRUE(vfs.Mount(in.Root(), &err)) << err;
    EXPECT_EQ(ReadStr(vfs, CONSTANT_PLAYER), netblockFirst ? "NETBLOCK" : "MODA");
  }
}

// A module sider.ini does not load, or any module when lua is off, answers nothing.
TEST(VfsOrder, DisabledModulesAnswerNothing) {
  {
    SyntheticInstall in("commented");
    in.Sider(kTwoRoots, ";lua.module = \"GameplayLoader.lua\"\n");
    in.LoaderState("modA");
    in.Put("livecpk/a/common/match/constant/constant_player.bin", "ROOT_A");
    in.Put("content/gameplay-loader/modA/common/match/constant/constant_player.bin", "MODA");
    pes::PesVfs vfs;
    std::string err;
    ASSERT_TRUE(vfs.Mount(in.Root(), &err)) << err;
    EXPECT_EQ(ReadStr(vfs, CONSTANT_PLAYER), "ROOT_A");
  }
  {
    SyntheticInstall in("luaoff");
    in.Sider(kTwoRoots, "lua.module = \"GameplayLoader.lua\"\n", "lua.enabled = 0\n");
    in.LoaderState("modA");
    in.Put("livecpk/a/common/match/constant/constant_player.bin", "ROOT_A");
    in.Put("content/gameplay-loader/modA/common/match/constant/constant_player.bin", "MODA");
    pes::PesVfs vfs;
    std::string err;
    ASSERT_TRUE(vfs.Mount(in.Root(), &err)) << err;
    EXPECT_EQ(ReadStr(vfs, CONSTANT_PLAYER), "ROOT_A");
  }
}

// The loader's selection is an input of the mount: an explicit mod replaces the
// boot state, "No Mod" selects nothing, and a name the loader does not list is
// refused (PES could never have selected it).
TEST(VfsOrder, ExplicitGameplayModReplacesBootState) {
  SyntheticInstall in("explicit");
  in.Sider(kTwoRoots, "lua.module = \"GameplayLoader.lua\"\n");
  in.LoaderState("modA");
  in.Put("livecpk/a/common/match/constant/constant_player.bin", "ROOT_A");
  in.Put("content/gameplay-loader/modA/common/match/constant/constant_player.bin", "MODA");
  in.Put("content/gameplay-loader/modB/common/match/constant/constant_player.bin", "MODB");
  const auto mount = [&](const std::string& mod, pes::PesVfs& vfs, std::string* err) {
    return vfs.Mount(in.Root(), err, {.gameplayMod = mod});
  };
  std::string err;
  pes::PesVfs b, none, unknown;
  ASSERT_TRUE(mount("modB", b, &err)) << err;
  EXPECT_EQ(ReadStr(b, CONSTANT_PLAYER), "MODB");
  EXPECT_EQ(b.GameplayMod(), "modB");
  ASSERT_TRUE(mount("No Mod", none, &err)) << err;
  EXPECT_EQ(ReadStr(none, CONSTANT_PLAYER), "ROOT_A");
  EXPECT_FALSE(mount("modC", unknown, &err));
  EXPECT_NE(err.find("modC"), std::string::npos);
}

// The boot state is the favorite only when the loader lists it.
TEST(VfsOrder, BootStateIsTheListedFavorite) {
  SyntheticInstall in("favorite");
  in.Sider("", "lua.module = \"GameplayLoader.lua\"\n");
  in.LoaderState("modB", "No Mod\r\nmodA\r\n# modB\r\n");  // CRLF file; modB commented out
  in.Put("content/gameplay-loader/modA/common/match/constant/constant_player.bin", "MODA");
  in.Put("content/gameplay-loader/modB/common/match/constant/constant_player.bin", "MODB");
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(in.Root(), &err)) << err;
  EXPECT_EQ(vfs.GameplayMod(), "No Mod");
  EXPECT_FALSE(vfs.Exists(CONSTANT_PLAYER));
}

// ---- the owner's install (PES21_ROOT) -----------------------------------



namespace {

const char* InstallRoot() { return std::getenv("PES21_ROOT"); }

std::optional<std::string> Slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return std::nullopt;
  return std::string(std::istreambuf_iterator<char>(in), {});
}

// The loader's own folder and mod list, read the way GameplayLoader.lua reads them.
struct LoaderFiles {
  fs::path base;
  std::vector<std::string> mods;  // every listed mod except "No Mod"
};
std::optional<LoaderFiles> InstallLoader(const char* root) {
  LoaderFiles lf{fs::path(root) / "content" / "gameplay-loader", {}};
  const auto csv = Slurp(lf.base / "gameplay-mods.csv");
  if (!csv || !fs::exists(fs::path(root) / "modules" / "GameplayLoader.lua")) return std::nullopt;
  std::istringstream in(*csv);
  for (std::string l; std::getline(in, l);) {
    if (!l.empty() && l.back() == '\r') l.pop_back();
    if (!l.empty() && l[0] != '#' && l.find(';') == std::string::npos) lf.mods.push_back(l);
  }
  return lf;
}

const char* const kConstantFiles[] = {
    "common/match/constant/constant_match.bin",        "common/match/constant/constant_player.bin",
    "common/match/constant/constant_positionCK.bin",   "common/match/constant/constant_positionPK.bin",
    "common/match/constant/constant_shootAging.bin",   "common/match/constant/constant_stadium.bin",
    "common/match/constant/constant_team.bin",         "common/match/constant/constant_tutorial.bin",
    "common/match/constant/constant_tutorialConsole.bin"};

}  // namespace

// Every file a mod folder holds for the loader's gameplay files resolves to that
// mod's bytes, for every mod the loader lists: the rule the live PES follows
// (it opened content/gameplay-loader/<mod>/common/match/constant/*.bin, sixteen
// at the time of the read, for all nine constant files).
TEST(VfsOrderInstall, GameplayFilesAreTheSelectedModsBytes) {
  const char* root = InstallRoot();
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  const auto loader = InstallLoader(root);
  if (!loader || loader->mods.empty()) GTEST_SKIP() << "no GameplayLoader in this install";
  size_t checked = 0;
  for (const std::string& mod : loader->mods) {
    pes::PesVfs vfs;
    std::string err;
    ASSERT_TRUE(vfs.Mount(root, &err, {.gameplayMod = mod})) << err;
    for (const char* file : kConstantFiles) {
      const auto disk = Slurp(loader->base / mod / file);
      if (!disk) continue;
      EXPECT_EQ(ReadStr(vfs, file), *disk) << mod << " " << file;
      ++checked;
    }
  }
  EXPECT_GT(checked, 0u);
}

// "No Mod": the loader answers nothing, so every gameplay file is one the game's
// own cpks hold (several mods ship the very bytes of a cpk, so the mods cannot
// tell the layers apart; the cpk variants can).
TEST(VfsOrderInstall, NoModServesACpkVariant) {
  const char* root = InstallRoot();
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  const auto loader = InstallLoader(root);
  if (!loader || loader->mods.empty()) GTEST_SKIP() << "no GameplayLoader in this install";
  pes::PesVfs none;
  std::string err;
  ASSERT_TRUE(none.Mount(root, &err, {.gameplayMod = "No Mod"})) << err;
  EXPECT_EQ(none.GameplayMod(), "No Mod");
  std::vector<std::string> variants;
  for (const char* sub : {"Data", "download"}) {
    if (!fs::is_directory(fs::path(root) / sub)) continue;
    for (const auto& d : fs::directory_iterator(fs::path(root) / sub)) {
      pes::CpkArchive a;
      if (d.path().extension() != ".cpk" || !a.Open(d.path().string(), &err)) continue;
      for (const auto& e : a.Entries())
        if (pes::PesVfs::Fold(e.path) == pes::PesVfs::Fold(kConstantFiles[1]))
          if (auto b = a.Read(e, &err)) variants.emplace_back(b->begin(), b->end());
    }
  }
  ASSERT_FALSE(variants.empty());
  EXPECT_NE(std::find(variants.begin(), variants.end(), ReadStr(none, kConstantFiles[1])), variants.end());
}

// constant::Trap in the live PES (read 30-10-26, registry 0x1437F4808 -> entry
// 0x3E -> +8): equal to the stock Data/dt18_all.cpk trap.o in 120 of 120 loader
// fields, not to download/4cc_06_gameplay.cpk's (31 fields differ) although that
// cpk is open in the process. The loader had selected "sixteen" (its path is in
// sider's memory for all nine constant files), whose trap.o is the stock one.
// GF reproduces it by selecting that mod, not by preferring the stock cpk.
TEST(VfsOrderInstall, LiveSelectedModsTrapIsTheStockTrap) {
  const char* root = InstallRoot();
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  const fs::path stockCpk = fs::path(root) / "Data" / "dt18_all.cpk";
  const auto loader = InstallLoader(root);
  if (!fs::exists(stockCpk) || !loader || !fs::exists(loader->base / "sixteen" / kConstantFiles[1]))
    GTEST_SKIP() << "install lacks Data/dt18_all.cpk or the sixteen gameplay mod";
  pes::CpkArchive cpk;
  std::string err;
  ASSERT_TRUE(cpk.Open(stockCpk.string(), &err)) << err;
  std::optional<std::vector<uint8_t>> stock;
  for (const auto& e : cpk.Entries())
    if (pes::PesVfs::Fold(e.path) == kConstantFiles[1] || e.path == kConstantFiles[1]) stock = cpk.Read(e, &err);
  ASSERT_TRUE(stock) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err, {.gameplayMod = "sixteen"})) << err;
  const auto mounted = vfs.Read(kConstantFiles[1], &err);
  ASSERT_TRUE(mounted) << err;
  const auto a = pes::ParseUniformTable(*mounted, &err), b = pes::ParseUniformTable(*stock, &err);
  ASSERT_TRUE(a && b) << err;
  const auto trapMounted = a->Find("trap.o"), trapStock = b->Find("trap.o");
  ASSERT_TRUE(trapMounted && trapStock);
  ASSERT_EQ(trapMounted->size(), trapStock->size());
  EXPECT_EQ(0, std::memcmp(trapMounted->data(), trapStock->data(), trapStock->size()));
}

// Below the sider layers the game's own cpks: a file a download/ cpk shares with
// a Data/ cpk resolves to the download/ bytes.
TEST(VfsOrderInstall, DownloadCpkOverridesDataCpk) {
  const char* root = InstallRoot();
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err, {.gameplayMod = "No Mod"})) << err;
  const fs::path data = fs::path(root) / "Data", dl = fs::path(root) / "download";
  if (!fs::is_directory(dl)) GTEST_SKIP() << "no download/ folder";
  // Data side: first Data cpk's file sizes, keyed by folded path, for the three Data cpks that carry the demo files.
  std::map<std::string, std::vector<uint8_t>> dataBytes;
  size_t checked = 0;
  for (const auto& d : fs::directory_iterator(data)) {
    const std::string n = d.path().filename().string();
    if (n != "dt12_g4.cpk") continue;  // the Data cpk of common/demo/fixdemo
    pes::CpkArchive a;
    if (!a.Open(d.path().string(), &err)) continue;
    for (const auto& e : a.Entries())
      if (e.path.rfind("common/demo/fixdemo/", 0) == 0 && e.size < (1u << 20))
        if (auto b = a.Read(e, &err)) dataBytes[pes::PesVfs::Fold(e.path)] = *b;
  }
  if (dataBytes.empty()) GTEST_SKIP() << "no Data/dt12_g4.cpk fixdemo files";
  for (const auto& d : fs::directory_iterator(dl)) {
    if (d.path().extension() != ".cpk") continue;
    pes::CpkArchive a;
    if (!a.Open(d.path().string(), &err)) continue;
    for (const auto& e : a.Entries()) {
      const auto it = dataBytes.find(pes::PesVfs::Fold(e.path));
      if (it == dataBytes.end() || e.size >= (1u << 20)) continue;
      const auto dlBytes = a.Read(e, &err);
      if (!dlBytes || *dlBytes == it->second) continue;  // identical bytes cannot tell the layers apart
      const auto mounted = vfs.Read(e.path, &err);
      ASSERT_TRUE(mounted) << e.path;
      EXPECT_NE(*mounted, it->second) << "Data bytes won: " << e.path << " from " << d.path().filename();
      ++checked;
    }
  }
  EXPECT_GT(checked, 0u) << "no differing fixdemo file shared by Data and download";
}
