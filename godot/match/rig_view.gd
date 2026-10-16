extends Node3D
## Minimal lighting inspection scene: a stadium (or fallback plane) under the
## rig PesLighting builds, with an orbit-free fixed camera. Args (after `--`):
## --pes21-root DIR --stadium N [--weather 0/1/2] [--time-zone 0/1]
## [--season 0/1] [--hour H --minute M] [--yaw DEG] [--pitch DEG] [--dist M]
## [--target "x,y,z"].
const DEFAULT_STADIUM_ID := 2

var args := {}

func _parse_args(raw: PackedStringArray) -> Dictionary:
	var out := {}
	var key := ""
	for a in raw:
		if a.begins_with("--"):
			key = a.trim_prefix("--")
			out[key] = true
		elif key != "":
			out[key] = a
			key = ""
	return out

func _ready() -> void:
	args = _parse_args(OS.get_cmdline_user_args())
	if not args.has("pes21-root"):
		push_error("usage: -- --pes21-root DIR [--stadium N] ...")
		get_tree().quit(2)
		return
	var library := PesLibrary.new()
	if library.open(args["pes21-root"]) != OK:
		get_tree().quit(1)
		return
	var stadium_id := int(args.get("stadium", DEFAULT_STADIUM_ID))
	var stadium: Node3D = PesModelBuilder.new().build_stadium(library, stadium_id)
	if stadium == null:
		push_error("stadium %d missing" % stadium_id)
		get_tree().quit(1)
		return
	var rig := PesLighting.new()
	add_child(rig)
	if not rig.setup(library, stadium_id, int(args.get("weather", 0)), int(args.get("time-zone", 0)), int(args.get("season", 0)), int(args.get("hour", -1)), int(args.get("minute", 0))):
		get_tree().quit(1)
		return
	print("rig children: ", rig.get_child_count())
	for c in rig.get_children():
		print("  ", c.get_class(), " ", c.name, " vis=", c.visible if c is Node3D else "?")
	var target := Vector3(0, 1, 0)
	if args.has("target"):
		var p: PackedStringArray = args["target"].split(",")
		if p.size() == 3:
			target = Vector3(float(p[0]), float(p[1]), float(p[2]))
	var yaw := deg_to_rad(float(args.get("yaw", 30)))
	var pitch := deg_to_rad(float(args.get("pitch", -18)))
	var dist := float(args.get("dist", 60))
	var off := Vector3(sin(yaw) * cos(pitch), -sin(pitch), cos(yaw) * cos(pitch)) * dist
	$Camera3D.position = target + off
	$Camera3D.look_at(target)
	print("cam pos=", $Camera3D.position, " fov=", $Camera3D.fov, " sky=", $WorldEnvironment.environment.sky != null)
	print("sun basis -z=", rig.get_node("Sun").global_transform.basis.z, " lux=", rig.get_node("Sun").get("light_intensity_lux"))
