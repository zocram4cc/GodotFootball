extends Node3D
## A visible AI-vs-AI match: the GF engine (GfMatch) plays, and every frame
## the PES players and the ball are placed where the simulation has them.
## Args (after `--`): --pes21-root DIR --home-team PES_ID --away-team PES_ID
## [--home-kit N --away-kit N] [--edit-save FILE] [--seed N] [--minutes N]
## [--ball ID] [--stadium N] [--follow PLAYER_INDEX] (inspection: a close camera
## on one player). [--camera-pose px,py,pz,fx,fy,fz,fovdeg] pins the camera to an
## exact pose (a live PES read) and skips the broadcast camera.

const GF_HOME_TEAM := 1        # teams in assets/fixtures/teams.sql
const GF_AWAY_TEAM := 2
const DEFAULT_SEED := 1000
const DEFAULT_MINUTES := 10
const TICK_S := 1.0 / 54.0      # GfMatch.step() is one PES logic frame
const MAX_TICKS_PER_FRAME := 10
# A render-frame interpolation distance above this is a teleport (a restart or a
# substitution), which snaps rather than slides.
const SNAP_DISTANCE_SQ := 100.0
# ponytail: PES picks the default ball from the user's settings
# (pes-re-officials-ball.md 1); ball001, the first vanilla ball, until that is read.
const DEFAULT_BALL_ID := 1
const BALL_PACK_FORMAT := "Asset/model/ball/ball%03d/#Win/ball.fpk"
const PITCH_SURROUND_M := 120.0
# Only used when the install ships no such stadium: a mid green mean of the
# common turf map (docs/re/pes-re-pitch-grass.md 5.3).
const PITCH_SURROUND_ALBEDO := Color(0.22, 0.42, 0.18)
const FACE_DIR := "Asset/model/character/face/real/"
const FOLLOW_OFFSET := Vector3(4.0, 2.2, 6.0)  # inspection camera, metres from the followed player
const FOLLOW_TARGET_HEIGHT_M := 1.0
# The registry slot of the goal scene's scorer (CAMERA_TARGET_0201) and the bones standing for the actor's joints 10 and 0.
const CAMERA_TARGET_SCORER := 1
const BONE_JOINT_10 := "sk_head"
const BONE_JOINT_0 := "sk_root_hip"

var args := {}
var library: PesLibrary
var sim: GfMatch
var players: Array[Node3D] = []
var ball: Node3D
var motion: PesMotionPlayer
var _skeletons: Array[Skeleton3D] = []
var _accumulator := 0.0
var _lastAlpha := 1.0
var _camera: Camera3D
var _broadcast: PesBroadcastCamera
var _cutscene: PesCutsceneCamera  # the --fdc-file debug track, on wall time
var _cut_t := 0.0
var _cutscene_scene := 0  # the FixDemo scene in force (GfMatch.cutscene_state); 0 when none
var _cut_steps: Array[PesCutsceneCamera] = []  # one per step of that scene: every layer's cameras of the step
var _cut_state: Dictionary = {}
var _fixed_pose := false


func configure(a: Dictionary) -> void:
	args = a


func _ready() -> void:
	if args.is_empty():
		args = _parse_args(OS.get_cmdline_user_args())
	if not args.has("pes21-root") or not args.has("home-team") or not args.has("away-team"):
		push_error("usage: -- --pes21-root DIR --home-team PES_ID --away-team PES_ID")
		get_tree().quit(2)
		return
	# The WorldEnvironment, sun rig (PesLighting) and camera live in
	# match.tscn; PesLighting fills them from the stadium's light rig.
	library = PesLibrary.new()
	if library.open(args["pes21-root"]) != OK:
		get_tree().quit(1)
		return
	if args.has("edit-save") and library.load_edit_save(args["edit-save"]) != OK:
		get_tree().quit(1)
		return
	sim = GfMatch.new()
	var sql := ProjectSettings.globalize_path("res://../assets/fixtures/teams.sql")
	var anims := ProjectSettings.globalize_path("res://../assets/gf_anims")
	if sim.start(library, sql, anims, GF_HOME_TEAM, GF_AWAY_TEAM, int(args.get("seed", DEFAULT_SEED)),
			int(args.get("minutes", DEFAULT_MINUTES)), args.has("pes-loop")) != OK:
		get_tree().quit(1)
		return
	# The ctx words the FixDemo conditions read from the environment (stadium, time of day, weather).
	sim.set_cutscene_environment(_stadium_id(), int(args.get("time-zone", 0)), int(args.get("weather", 0)))
	motion = PesMotionPlayer.new()
	if motion.load(library) != OK:
		motion = null  # bodies keep the bind pose; the match still plays
		push_warning("PES motion unavailable")
	_build_players()
	_build_stadium()
	_camera = $Camera3D
	if args.has("camera-pose"):
		var v := PackedFloat64Array()
		for s: String in String(args["camera-pose"]).split(","):
			v.append(float(s))
		if v.size() >= 7:
			_camera.look_at_from_position(Vector3(v[0], v[1], v[2]),
					Vector3(v[0], v[1], v[2]) + Vector3(v[3], v[4], v[5]), Vector3.UP)
			_camera.fov = v[6]
			_fixed_pose = true
	_broadcast = PesBroadcastCamera.new()
	$PesLighting.setup(library, _stadium_id(), int(args.get("weather", 0)), int(args.get("time-zone", 0)), int(args.get("season", 0)), int(args.get("hour", -1)), int(args.get("minute", 0)))
	# FieldInfo +0x00 (52.56) and +0x08 (34.06) live; the sim exposes the width.
	if _broadcast.load(library, sim.field_half_width(), sim.field_half_length()) != OK:
		get_tree().quit(1)
		return
	_broadcast.reset(sim.ball_position())
	# Debug entry: --fdc-file <VFS path of one .fdc>, with [--fdc-camera N] [--fdc-t0 SECONDS], plays that camera
	# track; the player clock starts at t0 seconds (the record's startCutTime is on that clock). A goal or foul stop
	# of the match plays the camera of the variant PES's selection picks (_poll_cutscene), not this entry.
	if args.has("fdc-file"):
		_cutscene = PesCutsceneCamera.new()
		_cut_t = float(args.get("fdc-t0", 0.0))
		if _cutscene.load_file(library, String(args["fdc-file"]), int(args.get("fdc-camera", 0))) != OK:
			get_tree().quit(1)
			return
	ball = PesModelBuilder.new().build_model(library, BALL_PACK_FORMAT % int(args.get("ball", DEFAULT_BALL_ID)), "")
	if ball == null:
		push_error("ball model missing")
		get_tree().quit(1)
		return
	add_child(ball)
	_interpolate(0.0)
	_place(0.0)


func _process(delta: float) -> void:
	if sim == null:
		return
	_accumulator += delta
	var ticks := 0
	while _accumulator >= TICK_S and ticks < MAX_TICKS_PER_FRAME and not sim.is_over():
		sim.step()
		_poll_cutscene()
		_accumulator -= TICK_S
		ticks += 1
	# Milestone A1: the sim steps one PES logic frame (1/54 s), which is not the
	# render frame's worth of time, so the pose is interpolated between the last
	# two logic frames by the leftover fraction of the tick that has elapsed.
	_interpolate(clampf(_accumulator / TICK_S, 0.0, 1.0))
	_place(delta)



# A goal or a foul stop made GF's FixDemo request (PES's evaluator and matcher on GF's arena). Its timeline (start, steps, end)
# runs in the sim (GfMatch.cutscene_state, pes::CutsceneTimeline); here every step gets the cameras of all its layers and the
# one in force plays on that step's player clock.
func _poll_cutscene() -> void:
	var req: Dictionary = sim.take_cutscene()
	if not req.is_empty() and not args.has("no-cutscene"):
		_load_cutscene(req)
	if _cutscene_scene == 0:
		return
	_cut_state = sim.cutscene_state()
	if _cut_state.is_empty() or _cut_state["ended"]:
		print("frame %d cutscene scene 0x%x ended" % [Engine.get_process_frames(), _cutscene_scene])
		_cut_steps.clear()
		_cutscene_scene = 0
		_cut_state = {}
	elif int(_cut_state["step_started"]) >= 0:
		print("frame %d cutscene scene 0x%x step %d starts" % [Engine.get_process_frames(), _cutscene_scene, int(_cut_state["step_started"])])
	if not _cut_state.is_empty() and int(_cut_state["step"]) >= 0:
		_advance_cut_camera(_cut_steps[int(_cut_state["step"])], float(_cut_state["clock_s"]))


# One logic frame of the step's procedural camera (PES's engine 0x141F7BFE0 on the translated layer): the goal scene's actor, the
# scorer, is written into CAMERA_TARGET_0201 (ground position and bearing from the sim, the two body points from his skeleton),
# then the engine runs on the step's player clock.
# ponytail: PES's actor update (the writer of CAMERA_TARGET_02nn, source object vtable 0x142BB1048) keeps its 20 joint points
# at +0x7C; the record's points are joints 0 and 10 (CONFIRMED by value in the captured goal: 0.94 m and 1.59 m above the
# scorer). Which bones those are is INF: sk_root_hip and sk_head. The skeleton is the render frame's, up to a logic frame old.
func _advance_cut_camera(cam: PesCutsceneCamera, clock_s: float) -> void:
	var actor: Dictionary = sim.cutscene_actor()
	if not actor.is_empty():
		var skeleton: Skeleton3D = _skeletons[int(actor["index"])]
		if skeleton != null:
			cam.set_target(CAMERA_TARGET_SCORER, actor["ground"], _bone_point(skeleton, BONE_JOINT_10), _bone_point(skeleton, BONE_JOINT_0), actor["facing_deg"])
	cam.advance(clock_s)


func _bone_point(skeleton: Skeleton3D, bone: String) -> Vector3:
	return skeleton.to_global(skeleton.get_bone_global_pose(skeleton.find_bone(bone)).origin)


func _load_cutscene(req: Dictionary) -> void:
	_cut_steps.clear()
	for step: Dictionary in req["steps"]:
		var cam := PesCutsceneCamera.new()
		cam.load_step(library, int(req["category"]), step["fdc"])
		# ponytail: PES's scene start 0x141F5CD00 gives each step its own stage default by the step's placement mode
		# (+0x10: 1 = the anchor, 2 = the scene's waypoint map at scene+0x60); every step takes the anchor.
		if req.has("anchor"):
			var anchor: Dictionary = req["anchor"]
			cam.set_stage_default(anchor["pos"], anchor["rot_y"], anchor["flip"])
		_cut_steps.append(cam)
		print("frame %d cutscene scene 0x%x cut 0x%x row %d layers %s" % [Engine.get_process_frames(), int(req["scene"]), int(step["cut"]), int(step["row"]), step["fdc"]])
	_cutscene_scene = int(req["scene"])


func _interpolate(alpha: float) -> void:
	# A teleport (a restart, a substitution) snaps instead of sliding.
	if alpha < _lastAlpha:
		alpha = 0.0
	_lastAlpha = alpha
	for i in players.size():
		var p: Vector3 = sim.player_position(i)
		var prev: Vector3 = sim.player_position_previous(i)
		if (prev - p).length_squared() > SNAP_DISTANCE_SQ:
			prev = p
		players[i].global_position = prev.lerp(Vector3(p.x, 0.0, p.z), alpha)
	var b: Vector3 = sim.ball_position()
	var bprev: Vector3 = sim.ball_position_previous()
	if (bprev - b).length_squared() > SNAP_DISTANCE_SQ:
		bprev = b
	ball.global_position = bprev.lerp(b, alpha)


# Every player plays the clip MOVESTEP last issued for it (GfMatch.player_clip). A player with no
# issue yet keeps the bind pose. ponytail: the sim's root motion still comes from GF's controller.
func _place(delta: float) -> void:
	for i in players.size():
		var f: Vector3 = sim.player_facing(i)
		if f.length_squared() > 0.0:
			players[i].rotation.y = atan2(f.x, f.z)  # PES characters face +Z
		if motion != null and _skeletons[i] != null:
			motion.update(i, _skeletons[i], sim.player_clip(i), delta)
	if _fixed_pose:
		return
	if _cutscene != null:
		_cut_t += delta
		_cutscene.update(_camera, _cut_t)
		return
	if _cutscene_scene != 0 and int(_cut_state.get("step", -1)) >= 0:
		var t: float = _cut_state["clock_s"]
		if int(_cut_state["step_started"]) < 0:  # the clock moved one tick: show the pose between the two, as the players are
			t = lerpf(_cut_state["clock_prev_s"], t, clampf(_accumulator / TICK_S, 0.0, 1.0))
		# False: no camera is in force (before the first record starts, or a procedural record of a mode the engine 0x141F7BFE0
		# is not proven in: only mode 4 is): the broadcast camera stays.
		if _cut_steps[int(_cut_state["step"])].update(_camera, t):
			return
	if args.has("follow"):
		var p: Vector3 = players[int(args["follow"])].global_position
		_camera.look_at_from_position(p + FOLLOW_OFFSET, p + Vector3.UP * FOLLOW_TARGET_HEIGHT_M)
	elif _broadcast != null:
		_broadcast.update(_camera, sim.ball_position(), delta)


# One PES face per GF slot, in folder order: real/<team><NN>.
# ponytail: squad order and shirt numbers are not read yet (PES team/squad db).
func _team_faces(team: int) -> Array[int]:
	var faces: Array[int] = []
	var prefix := FACE_DIR + str(team)
	for path: String in library.list(prefix):
		var id := int(path.trim_prefix(FACE_DIR).get_slice("/", 0))
		if path.ends_with("/face.fpk") and not faces.has(id):
			faces.append(id)
	faces.sort()
	return faces


func _build_players() -> void:
	var builder := PesCharacterBuilder.new()
	var teams := [int(args["home-team"]), int(args["away-team"])]
	var kits := [int(args.get("home-kit", 0)), int(args.get("away-kit", 1))]
	var faces := [_team_faces(teams[0]), _team_faces(teams[1])]
	var slot := [0, 0]
	for i in sim.player_count():
		var t: int = sim.player_team(i)
		var pool: Array[int] = faces[t]
		var face: int = pool[slot[t] % pool.size()] if not pool.is_empty() else 0
		slot[t] += 1
		var body: Node3D = builder.build_player(library, teams[t], kits[t], face)
		if body == null:
			push_error("player %d (face %d) failed to build" % [i, face])
			body = Node3D.new()
		add_child(body)
		players.append(body)
		_skeletons.append(body.get_node_or_null("Skeleton"))


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



# The stadium the match plays in: --stadium when given, else the home team's own
# stadium as PES resolves it (PesLibrary.team_stadium).
func _stadium_id() -> int:
	if args.has("stadium"):
		return int(args["stadium"])
	return library.team_stadium(int(args["home-team"]))

# PES's own stadium: its scene, its pitch, both goals with their nets and the
# corner flags, each model where PES's fox2 places it (pes::LoadStadiumScene).
# The builder fails only when the install has no such stadium.
func _build_stadium() -> void:
	var stadium_id := _stadium_id()
	var stadium: Node3D = PesModelBuilder.new().build_stadium(library, stadium_id, int(args.get("weather", 0)))
	if stadium == null:
		push_error("stadium %d missing from this install; fallback plane drawn" % stadium_id)
		var plane := PlaneMesh.new()
		plane.size = Vector2(PITCH_SURROUND_M, PITCH_SURROUND_M)
		var grass := StandardMaterial3D.new()
		grass.albedo_color = PITCH_SURROUND_ALBEDO
		plane.material = grass
		var ground := MeshInstance3D.new()
		ground.mesh = plane
		add_child(ground)
		return
	add_child(stadium)
