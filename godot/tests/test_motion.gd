extends SceneTree
## PesMotionPlayer plays the clip the MOVESTEP sim last issued for a player (the
## dictionary GfMatch.player_clip returns): its motion, mirror, start time, clock and
## crossfade (docs/re/pes-re-anim-blend.md 1-4). Exit 0 pass, 1 fail, 77 skipped.

const SKIP := 77
# A dt-shipped face pack first; the 4cc ones only if this install has none.
const FACE_CANDIDATES := [100117, 100135, 100393, 76711, 75403, 70922]
const CLIP := "sliding_0_0_000"    # a body clip whose limbs move asymmetrically, so mirroring shows
const OTHER_CLIP := "idle"         # the clip a crossfade goes to
const START_S := 0.5               # a start time inside the first clip
const TICK_S := 1.0 / 54.0         # PES's logic tick: the clip clock advances once per tick
const TEN_TICKS_S := 10.0 * TICK_S
const BLEND_S := 0.2
const POSE_TOLERANCE := 1e-4       # summed component distance over the joints
const MIRROR_MIN_CHANGE := 0.05    # a mirror must move the pose at least this much

var failures := 0


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("FAIL: ", what)


func pose_of(skeleton: Skeleton3D) -> Array:
	var out := []
	for j in skeleton.get_bone_count():
		out.append(skeleton.get_bone_pose_rotation(j))
	return out


func difference(a: Array, b: Array) -> float:
	var d := 0.0
	for j in a.size():
		# Component distance: angle_to's acos reads float32 noise as ~7e-4 rad.
		var qa: Quaternion = a[j]
		var qb: Quaternion = b[j]
		d += minf((qa - qb).length(), (qa + qb).length())
	return d


# One issued single clip, in the shape GfMatch.player_clip returns.
func issued(motion: int, start: float, serial: int, mirror: bool, blend: float) -> Dictionary:
	return {"serial": serial, "motion": motion, "group": -1, "mirror": mirror, "start_time": start,
			"start_rate": 0.0, "rate": 0.0, "duration": blend, "playback": 1.0}


func _initialize() -> void:
	var root := OS.get_environment("PES21_ROOT")
	if root.is_empty():
		print("SKIP: PES21_ROOT not set")
		quit(SKIP)
		return
	var library := PesLibrary.new()
	check(library.open(root) == OK, "library opens")
	var character: Node3D = null
	for face in FACE_CANDIDATES:
		character = PesCharacterBuilder.new().build_face_model(library, face)
		if character != null:
			break
	if character == null:
		print("SKIP: no face pack in this install to skeletonise")
		quit(SKIP)
		return
	get_root().add_child(character)
	await process_frame  # the root enters the tree after _initialize
	var skeleton: Skeleton3D = character.get_node("Skeleton")

	var clip := PesAnimation.new()
	check(clip.load(library, CLIP) == OK, "first clip loads")
	var other := PesAnimation.new()
	check(other.load(library, OTHER_CLIP) == OK, "second clip loads")
	var motion := clip.motion_no()
	var other_motion := other.motion_no()
	check(motion >= 0 and other_motion >= 0 and motion != other_motion, "clips carry their motion numbers")

	var player := PesMotionPlayer.new()
	check(player.load(library) == OK, "motion player loads")

	# The start frame: the FSM's start time is where the clip starts, at no elapsed time.
	check(player.update(0, skeleton, issued(motion, START_S, 1, false, 0.0), 0.0) == OK, "a plain issue plays")
	var plain := pose_of(skeleton)
	check(clip.apply(skeleton, START_S, true, false) == OK, "reference pose at the start")
	check(difference(plain, pose_of(skeleton)) < POSE_TOLERANCE, "the clip plays from its start time")
	plain = pose_of(skeleton)

	# The mirror: the same clip, played mirrored, is the mirrored pose and not the plain one.
	check(player.update(1, skeleton, issued(motion, START_S, 1, true, 0.0), 0.0) == OK, "a mirrored issue plays")
	var mirrored := pose_of(skeleton)
	check(clip.apply(skeleton, START_S, true, true) == OK, "reference pose, mirrored")
	check(difference(mirrored, pose_of(skeleton)) < POSE_TOLERANCE, "the clip plays mirrored when the FSM says so")
	check(difference(mirrored, plain) > MIRROR_MIN_CHANGE, "a mirror changes the pose")

	# The clock: a clip advances by the time it is given, at its playback rate.
	check(player.update(2, skeleton, issued(motion, START_S, 1, false, 0.0), TEN_TICKS_S) == OK, "the clock runs")
	var clocked := pose_of(skeleton)
	check(clip.apply(skeleton, START_S + TEN_TICKS_S, true, false) == OK, "reference pose after ten ticks")
	check(difference(clocked, pose_of(skeleton)) < POSE_TOLERANCE, "the clip moves on from its start by the elapsed time")

	# A re-issue is a new clip: it restarts at its start time.
	check(player.update(2, skeleton, issued(motion, START_S, 2, false, 0.0), 0.0) == OK, "a re-issue plays")
	check(difference(plain, pose_of(skeleton)) < POSE_TOLERANCE, "a re-issue restarts the clip")

	# The crossfade: a new clip starts from the old pose and reaches the new one at its blend time.
	check(player.update(3, skeleton, issued(motion, START_S, 1, false, 0.0), 0.0) == OK, "first clip of a crossfade")
	check(player.update(3, skeleton, issued(other_motion, 0.0, 2, false, BLEND_S), 0.0) == OK, "crossfade issue")
	check(difference(plain, pose_of(skeleton)) < POSE_TOLERANCE, "a crossfade starts from the old pose")
	check(player.update(3, skeleton, issued(other_motion, 0.0, 2, false, BLEND_S), BLEND_S) == OK, "crossfade ends")
	var blended := pose_of(skeleton)
	check(other.apply(skeleton, BLEND_S, true, false) == OK, "reference pose of the new clip")
	check(difference(blended, pose_of(skeleton)) < POSE_TOLERANCE, "a finished crossfade is the new clip")
	quit(1 if failures > 0 else 0)
