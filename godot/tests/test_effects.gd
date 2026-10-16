extends SceneTree
## Effects: st002's day-rain effect pack as PesEffects places it (from the install), the VFS
## path of effect textures, and (with PES21_ROOT) that the five static effect models and the
## textures the placed entries use load. Exit 0/1, 77 skipped (no install).

const SKIP := 77
const Effects = preload("res://effects/effect_entries.gd")
const COMMON_PACK := "Asset/effect/#Win/common_effect.fpk"
const FIVE_MODELS := ["3d_rain.fmdl", "effect_rainModel.fmdl", "net_splash.fmdl", "pitch_ground.fmdl", "pitch_leaf.fmdl"]
const GHOST_PES := "/Assets/pes16/model/bg/common/effect/texture/tex_ghost_05.ftex"
const GHOST_VFS := "Asset/model/bg/common/effect/texture/#windx11/tex_ghost_05.ftex"
const FILTER_PES := "/Assets/pes16/model/bg/common/effect/texture/filter/filter.ftex"
const FILTER_VFS := "Asset/model/bg/common/effect/texture/filter/#windx11/filter.ftex"
const RAIN_SCREEN_VFS := "Asset/model/bg/common/effect/texture/#windx11/tex_rain_screen_00.ftex"
const RAIN_PARTICLE_VFS := "Asset/model/bg/common/effect/texture/#windx11/tex_rain_particle_00.ftex"

const ST002 := 2
const WEATHER_RAIN := 1  # 0 fine, 1 rain, 2 snow
const TIME_DAY := 0  # 0 day, 1 night
const DAY_RAIN_PLACED := ["MultiUvScrollScreen", "RainDemo", "Vignetting"]  # sorted
const DAY_RAIN_UNPLACED := ["LensFlare", "LensFlare", "LightBillboard", "LightBillboard"]  # sorted

var failures := 0


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("FAIL: ", what)


func _initialize() -> void:
	check(Effects.vfs_texture(GHOST_PES) == GHOST_VFS, "flare texture maps to its VFS path")
	check(Effects.vfs_texture(FILTER_PES) == FILTER_VFS, "filter texture sits in its own folder")

	var root := OS.get_environment("PES21_ROOT")
	if root.is_empty():
		print("SKIP: PES21_ROOT not set (install checks)")
		quit(SKIP if failures == 0 else 1)
		return
	var library := PesLibrary.new()
	check(library.open(root) == OK, "library opens")
	var report: Dictionary = PesEffects.placement(library, ST002, WEATHER_RAIN, TIME_DAY)
	check(not report.is_empty(), "st002 day-rain effect pack loads")
	if not report.is_empty():
		var placed_types: Array = report["placed"].map(func(e): return e["type"])
		placed_types.sort()
		check(placed_types == DAY_RAIN_PLACED, "placed: %s" % [placed_types])
		var unplaced_types: Array = report["unplaced"].map(func(c): return c["type"])
		unplaced_types.sort()
		check(unplaced_types == DAY_RAIN_UNPLACED, "unplaced: %s" % [unplaced_types])
		check(report["orphans"].is_empty(), "no create lacks its setting")
		var rain: Array = report["placed"].filter(func(e): return e["type"] == "RainDemo")
		check(rain.size() == 1 and Effects.text(rain[0]["setting"], "texturePath") != "",
				"RainDemo carries its texture path")
	var builder := PesModelBuilder.new()
	for m in FIVE_MODELS:
		var node: Node3D = builder.build_model(library, COMMON_PACK, m)
		check(node != null and node.find_children("*", "MeshInstance3D", true, false).size() > 0,
				"%s builds with meshes" % m)
	for vfs_path in [FILTER_VFS, GHOST_VFS, RAIN_SCREEN_VFS, RAIN_PARTICLE_VFS]:
		check(library.load_texture(vfs_path) != null, "texture loads: %s" % vfs_path)
	quit(1 if failures > 0 else 0)
