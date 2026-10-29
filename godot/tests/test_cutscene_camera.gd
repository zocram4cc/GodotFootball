extends SceneTree
## PesCutsceneCamera plays the entrance cut's first camera (ent_001_view_st000_cam0000) at the pose the
## live game held, and applies the FixDemo player's cut offset (docs/ports/pes-port-FdcCamera5.md).
## Exit 0/1, 77 skipped.

const SKIP := 77
const FILE := "common/demo/fixdemo/ent/cut_data/ent_001_view_st000_cam1.fdc"
# Rendezvous capture 008 of 0x141F39CC0 (pid 1694737): the render block 0x1437F4A40 at the clock before the
# capture's (player+0x80 - player+0x6C = 178.88922 ticks). Float32 values; Godot keeps float32 transforms.
const LIVE_CLOCK_TICKS := 178.88922
const LIVE_EYE := Vector3(53.325878, 0.36969614, 35.635445)
const LIVE_FOV_RAD := 0.92731059
const TICKS_PER_S := 60.0
const TOL := 1e-4

var failures := 0


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("FAIL: ", what)


func _initialize() -> void:
	var root := OS.get_environment("PES21_ROOT")
	if root.is_empty():
		print("SKIP: PES21_ROOT not set")
		quit(SKIP)
		return
	var library := PesLibrary.new()
	check(library.open(root) == OK, "library opens")
	var cut := PesCutsceneCamera.new()
	if cut.load_file(library, FILE, 0) != OK:
		print("SKIP: the install has no ", FILE)
		quit(SKIP)
		return
	var camera := Camera3D.new()
	get_root().add_child(camera)
	await process_frame
	var t_s := LIVE_CLOCK_TICKS / TICKS_PER_S

	cut.update(camera, t_s)
	var eye := camera.global_position
	var forward0 := -camera.global_transform.basis.z
	check(eye.distance_to(LIVE_EYE) < TOL, "eye is the live camera's (%s)" % eye)
	check(absf(deg_to_rad(camera.fov) - LIVE_FOV_RAD) < TOL, "fov is the live camera's (%f)" % camera.fov)

	# The record asks for the player's offset (+0xA6 = 1): a position offset moves the eye by exactly it.
	var off := Vector3(10.0, -1.0, 5.0)
	cut.set_cut_offset(off, 0.0, false)
	cut.update(camera, t_s)
	check(camera.global_position.distance_to(eye + off) < TOL, "position offset is added to the eye")

	# rotY turns the shot about world Y (0x141F7B8FC): (x, y, z) goes to (z, y, -x) for 90 degrees.
	cut.set_cut_offset(Vector3.ZERO, 90.0, false)
	cut.update(camera, t_s)
	check(camera.global_position.distance_to(Vector3(eye.z, eye.y, -eye.x)) < 1e-3,
		"rotY 90 turns the eye about Y (%s)" % camera.global_position)
	var forward := -camera.global_transform.basis.z
	check(forward.distance_to(Vector3(forward0.z, forward0.y, -forward0.x)) < 1e-3,
		"rotY 90 turns the view direction about Y (%s)" % forward)

	# Flip mirrors X: the eye's x changes sign.
	cut.set_cut_offset(Vector3.ZERO, 0.0, true)
	cut.update(camera, t_s)
	check(absf(camera.global_position.x + eye.x) < TOL, "flip mirrors the eye's x")
	quit(1 if failures > 0 else 0)
