extends SceneTree
## A match through the GDExtension with substitutions on by default. The test steps the match until the
## active roster changes (a substitution), then runs on for a while so the Godot side has to follow the
## new man. Any crash fails the run; so does a swap the Godot side cannot see. Exit 0/1, 77 skipped.

const SKIP := 77
const HOME := 1
const AWAY := 2
const SEED := 7
const MINUTES := 20
const PLAYERS := 22
const TICKS_PER_SECOND := 54  # PES's logic frame rate (1/54 s)
const TICKS_BETWEEN_ROSTER_CHECKS := 54
const TICKS_AFTER_FIRST_SWAP := 3000
const MAX_TICKS := MINUTES * 60 * TICKS_PER_SECOND + 10000

var failures := 0


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("FAIL: ", what)


func roster(m: GfMatch) -> Array:
	var ids := []
	for i in m.player_count():
		ids.append(m.player_database_id(i))
	return ids


func _init() -> void:
	var root := OS.get_environment("PES21_ROOT")
	if root.is_empty():
		print("SKIP: PES21_ROOT not set")
		quit(SKIP)
		return
	var library := PesLibrary.new()
	check(library.open(root) == OK, "library opens")

	var m := GfMatch.new()
	check(m.start(library, ProjectSettings.globalize_path("res://../assets/fixtures/teams.sql"),
			ProjectSettings.globalize_path("res://../assets/gf_anims"), HOME, AWAY, SEED, MINUTES, true) == OK,
			"match starts")
	var before := roster(m)
	check(before.size() == PLAYERS, "22 players on the pitch (%d)" % before.size())

	var ticks := 0
	var swapAt := -1
	while not m.is_over() and ticks < MAX_TICKS:
		m.step()
		ticks += 1
		if swapAt < 0 and ticks % TICKS_BETWEEN_ROSTER_CHECKS == 0 and roster(m) != before:
			swapAt = ticks
			print("substitution on the pitch at tick %d (match minute %.1f)" % [ticks, m.match_time_ms() / 60000.0])
		if swapAt >= 0 and ticks - swapAt >= TICKS_AFTER_FIRST_SWAP:
			break

	check(swapAt >= 0 or m.is_over(), "the match ran without a stall")
	check(m.player_count() == PLAYERS, "22 players still on the pitch (%d)" % m.player_count())
	for i in m.player_count():
		var p: Vector3 = m.player_position(i)
		check(is_finite(p.x) and is_finite(p.y) and is_finite(p.z), "player %d position is finite" % i)
	print("ticks %d, swap at tick %d, match over %s" % [ticks, swapAt, m.is_over()])
	quit(1 if failures > 0 else 0)
