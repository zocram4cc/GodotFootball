extends Node3D
## Standalone effects scene (not wired into the match): the five static effect models of
## common_effect.fpk on a ground plane, and the placed entries of one effect_config.xml.
## Args after `--`: --pes21-root DIR --effects JSON [--focus NAME]
##   JSON: `pesfmt_dump effects <stadium effect pack> --pes21-root DIR > JSON`.
##   NAME is a model's basename (pitch_leaf, net_splash, ...): the camera moves close to it and
##   the other four models are hidden (a frame of that one effect).

const Effects = preload("res://effects/effect_entries.gd")
const COMMON_PACK := "Asset/effect/#Win/common_effect.fpk"
const FIVE_MODELS := ["3d_rain.fmdl", "effect_rainModel.fmdl", "net_splash.fmdl", "pitch_ground.fmdl", "pitch_leaf.fmdl"]
const MODEL_SPACING_M := 2.0
const CAMERA_FOV_DEG := 40.0  # vertical, the viewer's lens
const CAMERA_OVERVIEW_POS := Vector3(0.0, 6.0, 14.0)  # metres, Fox space: +Z is toward the viewer
const FOCUS_BACK_M := 4.0  # camera distance from the focused model
const FOCUS_HEIGHT_M := 2.0
const FLOOR_SIZE_M := 30.0
const FLOOR_ALBEDO := Color(0.35, 0.45, 0.3)

var camera: Camera3D
var screen: CanvasLayer


func _ready() -> void:
	var args := _parse_args(OS.get_cmdline_user_args())
	if not args.has("pes21-root") or not args.has("effects"):
		push_error("usage: -- --pes21-root DIR --effects JSON [--focus NAME]")
		get_tree().quit(2)
		return
	var library := PesLibrary.new()
	if library.open(args["pes21-root"]) != OK:
		get_tree().quit(1)
		return
	var config: Variant = JSON.parse_string(FileAccess.get_file_as_string(args["effects"]))
	if typeof(config) != TYPE_DICTIONARY:
		push_error("not an effect config JSON: %s" % args["effects"])
		get_tree().quit(1)
		return
	_build_environment()
	var builder := PesModelBuilder.new()
	for i in FIVE_MODELS.size():
		var model: Node3D = builder.build_model(library, COMMON_PACK, FIVE_MODELS[i])
		if model == null:
			push_error("effect model missing from %s: %s" % [COMMON_PACK, FIVE_MODELS[i]])
			get_tree().quit(1)
			return
		model.name = FIVE_MODELS[i].get_basename()
		model.position = Vector3((i - 2) * MODEL_SPACING_M, 0.0, 0.0)
		model.visible = not args.has("focus") or model.name == args["focus"]
		add_child(model)
	var placed := Effects.build_entries(camera, screen, library, config)
	var report: Dictionary = Effects.placement(config)
	var unplaced := PackedStringArray()
	for create in report["unplaced"]:
		unplaced.append(create["type"])
	print("effects: %d placed; unplaced %s; %d orphan creates" % [
		placed, ", ".join(unplaced), report["orphans"].size()])
	if args.has("focus"):
		var model := get_node_or_null(args["focus"])
		if model == null:
			push_error("no model named %s" % args["focus"])
			get_tree().quit(1)
			return
		var at: Vector3 = model.position
		camera.look_at_from_position(at + Vector3(0.0, FOCUS_HEIGHT_M, FOCUS_BACK_M), at)


func _parse_args(raw: PackedStringArray) -> Dictionary:
	var out := {}
	var i := 0
	while i < raw.size():
		var key := raw[i].trim_prefix("--")
		if i + 1 < raw.size() and not raw[i + 1].begins_with("--"):
			out[key] = raw[i + 1]
			i += 2
		else:
			out[key] = true
			i += 1
	return out


# The match's lighting rig: PesLighting finds the WorldEnvironment and Camera3D beside it by
# name (as in match.tscn) and configures both, as the viewer does.
func _build_environment() -> void:
	var world := WorldEnvironment.new()
	world.name = "WorldEnvironment"
	add_child(world)
	var lighting := PesLighting.new()
	lighting.name = "PesLighting"
	add_child(lighting)

	var floor_mesh := PlaneMesh.new()
	floor_mesh.size = Vector2(FLOOR_SIZE_M, FLOOR_SIZE_M)
	var floor_material := StandardMaterial3D.new()
	floor_material.albedo_color = FLOOR_ALBEDO
	floor_mesh.material = floor_material
	var ground := MeshInstance3D.new()
	ground.mesh = floor_mesh
	add_child(ground)

	camera = Camera3D.new()
	camera.name = "Camera3D"
	camera.fov = CAMERA_FOV_DEG
	add_child(camera)
	camera.look_at_from_position(CAMERA_OVERVIEW_POS, Vector3.ZERO)
	camera.current = true

	screen = CanvasLayer.new()
	add_child(screen)
