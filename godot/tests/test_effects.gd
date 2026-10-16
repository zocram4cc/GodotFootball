extends SceneTree
## Effects scene: which effect_config.xml entries are placed, the VFS path of effect
## textures, and (with PES21_ROOT) that the five static effect models and the textures
## the placed entries use load from the install. Exit 0/1, 77 skipped (no install).

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

# An st002 day-rain config as pesfmt_dump effects writes it, without params: the
# placement logic only reads types, indices and creates. The last create names a
# setting the pack does not have, as the nf packs' rain creates do.
const CONFIG := {
	"settings": [
		{"type": "MultiUvScrollScreen", "index": 0, "params": []},
		{"type": "RainDemo", "index": 0, "params": []},
		{"type": "LightBillboard", "index": 0, "params": []},
		{"type": "LensFlare", "index": 0, "params": []},
		{"type": "Vignetting", "index": 0, "params": []},
	],
	"creates": [
		{"type": "MultiUvScrollScreen", "setting": 0, "attributes": {}},
		{"type": "RainDemo", "setting": 0, "attributes": {}},
		{"type": "LightBillboard", "setting": 0, "attributes": {"floor": "upper", "dir": "back"}},
		{"type": "LensFlare", "setting": 0, "attributes": {}},
		{"type": "Vignetting", "setting": 0, "attributes": {}},
		{"type": "RainDemo", "setting": 3, "attributes": {}},
	],
}

var failures := 0


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("FAIL: ", what)


func _initialize() -> void:
	check(Effects.vfs_texture(GHOST_PES) == GHOST_VFS, "flare texture maps to its VFS path")
	check(Effects.vfs_texture(FILTER_PES) == FILTER_VFS, "filter texture sits in its own folder")

	var placed: Dictionary = Effects.placement(CONFIG)
	var placed_types: Array = placed["placeable"].map(func(e): return e["type"])
	check(placed_types == ["MultiUvScrollScreen", "RainDemo", "Vignetting"], "placed: %s" % [placed_types])
	check(placed["unplaced"].size() == 2, "LightBillboard and LensFlare are reported unplaced (%s)" % [placed["unplaced"]])
	check(placed["orphans"].size() == 1 and int(placed["orphans"][0]["setting"]) == 3,
			"a create with no setting is an orphan, not a placement")

	var root := OS.get_environment("PES21_ROOT")
	if root.is_empty():
		print("SKIP: PES21_ROOT not set (install checks)")
		quit(SKIP if failures == 0 else 1)
		return
	var library := PesLibrary.new()
	check(library.open(root) == OK, "library opens")
	var builder := PesModelBuilder.new()
	for m in FIVE_MODELS:
		var node: Node3D = builder.build_model(library, COMMON_PACK, m)
		check(node != null and node.find_children("*", "MeshInstance3D", true, false).size() > 0,
				"%s builds with meshes" % m)
	for vfs_path in [FILTER_VFS, GHOST_VFS, RAIN_SCREEN_VFS, RAIN_PARTICLE_VFS]:
		check(library.load_texture(vfs_path) != null, "texture loads: %s" % vfs_path)
	quit(1 if failures > 0 else 0)
