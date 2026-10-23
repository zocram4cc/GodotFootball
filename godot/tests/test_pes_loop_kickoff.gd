extends Node
## A PES-loop match on the fixture teams runs past kick-off. The team AI writes the slot-point rows every
## tick (WriteRoleArrays -> WriteSlotPoints), which needs the install's formation tables and sheet constants
## from the library; without them the match stops here. Exit 0/1, 77 skipped.

const SKIP := 77
const HOME := 1  # the fixture's teams: Tactics.bin holds a plan-0 record for each
const AWAY := 2
const FRAMES := 1200  # about 20 s of match time at 60 fps
const PAST_KICK_OFF_MS := 15000  # the kick-off restart is a few seconds; play has started well before this

var failures := 0


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("FAIL: ", what)


func _ready() -> void:
	var root := OS.get_environment("PES21_ROOT")
	if root.is_empty():
		print("SKIP: PES21_ROOT not set")
		get_tree().quit(SKIP)
		return
	var scene: Node3D = load("res://match/match.tscn").instantiate()
	scene.configure({"pes21-root": root, "home-team": str(HOME), "away-team": str(AWAY), "pes-loop": ""})
	add_child(scene)
	for i in FRAMES:
		await get_tree().process_frame
	var elapsed_ms: int = scene.sim.match_time_ms()
	check(elapsed_ms >= PAST_KICK_OFF_MS, "the match passed kick-off (%d ms)" % elapsed_ms)
	get_tree().quit(1 if failures > 0 else 0)
