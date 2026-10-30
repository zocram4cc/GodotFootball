// The order PES resolves a file in: sider's Lua modules (livecpk_make_key /
// livecpk_get_filepath, in sider.ini module order), then sider's cpk.root
// folders (first listed wins), then the game's own cpks. Synthetic installs
// pin the rule; the install tests at the bottom check it against the owner's
// files and the live process (docs/re/pes-re-vfs-order.md).
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

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
