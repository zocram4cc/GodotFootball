extends SceneTree
## PesModelBuilder builds static PES models (ball, goals, stadium scenes) with
## their techniques' shaders. Exit 0/1, 77 skipped.

const SKIP := 77
const BALL_PACK := "Asset/model/ball/ball001/#Win/ball.fpk"  # vanilla; 4cc balls are novelty shapes
const GOAL_PACK := "Asset/model/bg/common/goal/#Win/goal_post.fpk"
const PITCH_PACK := "Asset/model/bg/st002/pitch/#Win/pitch_st002.fpk"
const BALL_RENDER_RADIUS_M := 0.110       # ball.fmdl bbox (pes-re-officials-ball.md 6)
const GOAL_FRAME_FACE_X_M := -52.44       # goal.fmdl AABB max x (pes-re-stadium-camera.md 1.3)
const GOAL_CROSSBAR_TOP_M := 2.56
const TOLERANCE_M := 0.01

var failures := 0


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("FAIL: ", what)


func meshes_of(n: Node) -> Array[MeshInstance3D]:
	var out: Array[MeshInstance3D] = []
	for c in n.find_children("*", "MeshInstance3D", true, false):
		out.append(c)
	return out


func bounds(n: Node) -> AABB:
	var box := AABB()
	var first := true
	for m in meshes_of(n):
		var b := m.transform * m.mesh.get_aabb()
		box = b if first else box.merge(b)
		first = false
	return box


func shader_of(m: MeshInstance3D) -> String:
	var mat := m.mesh.surface_get_material(0)
	return (mat as ShaderMaterial).shader.resource_path if mat is ShaderMaterial else ""


# The body of a shader function: from its header to the closing brace at column 0.
func fn_body(src: String, header: String) -> String:
	var start := src.find(header)
	if start < 0:
		return ""
	var end := src.find("\n}", start)
	return src.substr(start, end - start) if end > start else ""


# The pitch PS adds the VS fog terms o7.z and o7.w to specular and roughness
# (pitch PS asm 166; VS asm 122-129). Headless there is no frame to read back,
# so this checks the shader source: the vertex stage writes the terms, and the
# fragment stage adds them to roughness (asm 177) and to the specular scale (asm 176-178).
func check_pitch_v7_terms() -> void:
	var src := FileAccess.get_file_as_string("res://shaders/pes_pitch.gdshader")
	var vertex_fn := fn_body(src, "void vertex()")
	var fragment_fn := fn_body(src, "void fragment()")
	check(RegEx.create_from_string("pes_v7\\s*=\\s*vec2\\(").search(vertex_fn) != null,
			"pitch vertex stage writes the VS fog terms o7.z, o7.w (VS asm 122-129)")
	check(RegEx.create_from_string("ROUGHNESS\\s*=\\s*s1\\s*\\+\\s*pes_v7\\.y").search(fragment_fn) != null,
			"pitch roughness adds o7.w (PS asm 166, 177)")
	check(RegEx.create_from_string("specScale\\s*=\\s*m4\\s*\\*\\s*slope\\.r\\s*\\*\\s*match\\s*\\*\\s*\\(\\s*s0\\s*\\+\\s*pes_v7\\.x\\s*\\)").search(fragment_fn) != null,
			"pitch specular adds o7.z to S0 (PS asm 166, 176, 178)")


func _initialize() -> void:
	check_pitch_v7_terms()
	var root := OS.get_environment("PES21_ROOT")
	if root.is_empty():
		print("SKIP: PES21_ROOT not set")
		quit(1 if failures > 0 else SKIP)
		return
	var library := PesLibrary.new()
	check(library.open(root) == OK, "library opens")
	var builder := PesModelBuilder.new()

	var ball: Node3D = builder.build_model(library, BALL_PACK, "")
	check(ball != null and meshes_of(ball).size() > 0, "ball builds")
	if ball != null:
		var b := bounds(ball)
		check(absf(b.size.x * 0.5 - BALL_RENDER_RADIUS_M) < TOLERANCE_M, "ball radius 0.110 (%s)" % b)
		var mat = meshes_of(ball)[0].mesh.surface_get_material(0)
		check(mat is ShaderMaterial and mat.get_shader_parameter("Base_Tex_SRGB") != null, "ball base texture bound")
		check(shader_of(meshes_of(ball)[0]) == "res://shaders/pes_blin.gdshader", "ball draws with fox3DDF_Blin")

	var goal: Node3D = builder.build_model(library, GOAL_PACK, "goal.fmdl")  # the pack also holds shadow.fmdl
	check(goal != null, "goal builds")
	if goal != null:
		var g := bounds(goal)
		check(absf(g.end.x - GOAL_FRAME_FACE_X_M) < TOLERANCE_M, "goal frame face at x -52.44 (%s)" % g)
		check(absf(g.end.y - GOAL_CROSSBAR_TOP_M) < TOLERANCE_M, "crossbar top 2.56 (%s)" % g)

	var pitch: Node3D = builder.build_model(library, PITCH_PACK, "")
	check(pitch != null, "pitch builds")
	if pitch != null:
		var shaders := {}
		for m in meshes_of(pitch):
			shaders[shader_of(m)] = true
		check(shaders.has("res://shaders/pes_pitch.gdshader"), "pitch draws with pes3DDF_Pitch2020 (%s)" % str(shaders.keys()))
		# line_alp_dds1 is fox3DDC_GGX (the install's lua: Deferred_Decal, fox3ddf_ggx_decal);
		# pitch_scratch is fox3DDC_Blin (fox3ddf_blin_decal). Both are decals, not opaque Blin.
		check(shaders.has("res://shaders/pes_ggx_decal.gdshader"), "lines draw as the GGX decal (%s)" % str(shaders.keys()))
		check(shaders.has("res://shaders/pes_blin_decal.gdshader"), "pitch_scratch draws as the Blin decal (%s)" % str(shaders.keys()))

	check(builder.build_model(library, "Asset/model/none/#Win/none.fpk", "") == null, "a missing pack builds nothing")
	quit(1 if failures > 0 else 0)
