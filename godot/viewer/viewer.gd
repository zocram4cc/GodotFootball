extends Node3D
## Shows one PES 2021 character from the user's install, optionally animated.
## Args after `--`: --pes21-root DIR --face ID [--team ID [--kit N]] [--clip NAME|0xHASH] [--in-place]
##   [--hide PART,...] [--number N --name NAME] [--yaw DEG] [--edit-save EDIT00000000] [--distance M]
##   [--stadium N] [--weather W] [--time-zone T] [--season S] [--hour H] [--minute M] [--target-y M]
## Each player's look comes from the user's save. --distance moves the camera (default 3.2 m, the
## whole figure; about 1.5 m frames a kit). The light is the stadium's rig, as match.tscn builds it
## (PesLighting), with match.gd's defaults. With --team the player is assembled as PES does
## (kit N, 0 = 1st); without it, the face pack alone.

const CAMERA_DISTANCE_M := 3.2  # default: the whole figure at CAMERA_FOV_DEG, in front of a character facing +Z
const CAMERA_TARGET_Y := 1.0                    # metres: about chest height of a 1.8 m player
const CAMERA_FOV_DEG := 40.0                     # frames a T-pose (1.54 m arm span) with margin
const FLOOR_SIZE_M := 6.0                       # ground at y = 0 (Fox and gani root height), for foot contact
const FLOOR_ALBEDO := Color(0.35, 0.45, 0.3)
# The light rig's defaults, as match.gd has them: stadium 2, day, fine weather, summer.
const LIGHT_STADIUM_ID := 2
const LIGHT_WEATHER_FINE := 0
const LIGHT_TIME_DAY := 0
const LIGHT_SEASON_SUMMER := 0

var character: Node3D
var skeleton: Skeleton3D
var animation: PesAnimation
var in_place := false
var clock := 0.0


func _ready() -> void:
	var args := _parse_args(OS.get_cmdline_user_args())
	if not args.has("pes21-root") or not args.has("face"):
		push_error("usage: -- --pes21-root DIR --face ID [--clip NAME|0xHASH] [--in-place]")
		get_tree().quit(2)
		return
	_build_environment(float(args.get("distance", str(CAMERA_DISTANCE_M))), float(args.get("target-y", str(CAMERA_TARGET_Y))))
	var library := PesLibrary.new()
	if library.open(args["pes21-root"]) != OK:
		get_tree().quit(1)
		return
	if args.has("edit-save") and library.load_edit_save(args["edit-save"]) != OK:
		get_tree().quit(1)
		return
	var lighting: PesLighting = get_node("PesLighting")
	if not lighting.setup(library, int(args.get("stadium", str(LIGHT_STADIUM_ID))),
			int(args.get("weather", str(LIGHT_WEATHER_FINE))), int(args.get("time-zone", str(LIGHT_TIME_DAY))),
			int(args.get("season", str(LIGHT_SEASON_SUMMER))), int(args.get("hour", "-1")),
			int(args.get("minute", "0"))):
		get_tree().quit(1)
		return
	var builder := PesCharacterBuilder.new()
	if args.has("team"):
		character = builder.build_player(library, int(args["team"]), int(args.get("kit", "0")), int(args["face"]),
				int(args.get("number", "-1")), args.get("name", ""))
	else:
		character = builder.build_face_model(library, int(args["face"]))
	if character == null:
		get_tree().quit(1)
		return
	add_child(character)
	character.rotation_degrees.y = float(args.get("yaw", "0"))  # inspection: 180 shows the back
	skeleton = character.get_node("Skeleton")
	if args.has("hide"):  # inspection: drop parts by model stem, e.g. --hide socks_middle,arm
		var hidden: PackedStringArray = args["hide"].split(",")
		for m in skeleton.get_children():
			if m is MeshInstance3D and str(m.get_meta("part", "")) in hidden:
				m.visible = false
	in_place = args.has("in-place")
	if args.has("clip"):
		animation = PesAnimation.new()
		if animation.load(library, args["clip"]) != OK:
			push_error("clip not found: %s" % args["clip"])
			get_tree().quit(1)
			return
		print("clip ", args["clip"], ": ", animation.length_seconds(), " s")


func _process(delta: float) -> void:
	if animation == null:
		return
	clock += delta
	animation.apply(skeleton, fmod(clock, animation.length_seconds()), in_place)


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


# PesLighting finds the WorldEnvironment and Camera3D beside it by name (as in match.tscn) and
# configures both, so the viewer lights its character with the same rig as the match.
func _build_environment(distance_m: float, target_y: float) -> void:
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

	var camera := Camera3D.new()
	camera.name = "Camera3D"
	camera.fov = CAMERA_FOV_DEG
	add_child(camera)
	var target := Vector3(0.0, target_y, 0.0)
	camera.look_at_from_position(Vector3(0.0, target_y, distance_m), target)
	camera.current = true
