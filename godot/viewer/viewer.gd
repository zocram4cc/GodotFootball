extends Node3D
## Shows one PES 2021 character from the user's install, optionally animated.
## Args after `--`: --pes21-root DIR --face ID [--team ID [--kit N]] [--clip NAME|0xHASH] [--in-place]
##   [--hide PART,...] [--number N --name NAME] [--yaw DEG] [--edit-save EDIT00000000] [--distance M]
##   [--stadium N] [--weather W] [--time-zone T] [--season S] [--hour H] [--minute M] [--target-y M]
##   [--engine-pose DUMP --engine-label LABEL]: poses the skeleton with PES's anime engine output (GF_ENGINE_POSE_DUMP of pes_anime_engine_test)
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
var engine_pose := {}  # --engine-pose: {"root": Vector3, "local": Array of Quaternion} from a pes_anime_engine_test dump
const ENGINE_JOINT_FLOATS := 7  # a dump row's per-joint floats: position (3), rotation x y z w (4)


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
	if args.has("engine-pose"):
		engine_pose = _read_engine_pose(args["engine-pose"], args.get("engine-label", "idle"))
		if engine_pose.is_empty():
			push_error("engine pose not found: %s %s" % [args["engine-pose"], args.get("engine-label", "idle")])
			get_tree().quit(1)
			return
		_apply_engine_pose()


func _process(delta: float) -> void:
	if not engine_pose.is_empty():
		_apply_engine_pose()
	if animation == null:
		return
	clock += delta
	animation.apply(skeleton, fmod(clock, animation.length_seconds()), in_place)


# The pose PES's anime engine left in a player's control (pes_anime_engine_test with GF_ENGINE_POSE_DUMP): per rig joint the model-space
# position and rotation in PES axes (y up, z forward). The engine keeps each bone in a frame of its own: its rotation is GF's clip rotation
# (the chain of the clip's locals below the body joint) times a fixed per-joint frame B_j, the dump's `bind` row (measured by the test against
# GF's sampler on four clips). So below the body joint chain_j = R_body^-1 * R_j * B_j^-1 and a bone's local rotation is its parent's chain
# taken off its own; the body joint takes the engine's own rotation and position.
func _read_engine_pose(path: String, label: String) -> Dictionary:
	var parents := PackedInt32Array()
	var bind: Array[Quaternion] = []
	var pose := {}
	for line in FileAccess.get_file_as_string(path).split("\n", false):
		var p := line.split(" ", false)
		if p[0] == "parents":
			for i in range(1, p.size()):
				parents.append(int(p[i]))
		elif p[0] == "bind":
			for j in range((p.size() - 1) / 4):
				bind.append(Quaternion(float(p[1 + j * 4]), float(p[2 + j * 4]), float(p[3 + j * 4]), float(p[4 + j * 4])))
		elif p[0] == "pose" and p[1] == label:
			var rotations: Array[Quaternion] = []
			var root := Vector3.ZERO
			for j in range((p.size() - 2) / ENGINE_JOINT_FLOATS):
				var at := 2 + j * ENGINE_JOINT_FLOATS
				if j == 0:
					root = Vector3(float(p[at]), float(p[at + 1]), float(p[at + 2]))
				rotations.append(Quaternion(float(p[at + 3]), float(p[at + 4]), float(p[at + 5]), float(p[at + 6])))
			var body := rotations[0]
			var chain: Array[Quaternion] = []
			for j in rotations.size():
				chain.append(body.inverse() * rotations[j] * bind[j].inverse())
			var local: Array[Quaternion] = []
			for j in rotations.size():
				local.append(body if parents[j] < 0 else chain[parents[j]].inverse() * chain[j])
			pose = {"root": root, "local": local}
	return pose


func _apply_engine_pose() -> void:
	var local: Array = engine_pose["local"]
	for j in local.size():
		skeleton.set_bone_pose_rotation(j, local[j])
	skeleton.set_bone_pose_position(0, engine_pose["root"])


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
