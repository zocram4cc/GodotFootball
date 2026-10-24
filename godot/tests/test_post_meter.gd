extends SceneTree
## GPU check of the match post chain's REDUCE+LAW passes (godot/post/pes_post.glsl). PES's
## Tonemap_Lum meters max(dot709, 0.001, 32a²) arithmetically over the cb5[12] UV sub-rectangle
## (ComposePath §4); the 32a² term is omitted (Godot alpha is not the bright-pixel code).
## The adaptation law is PES's FUN_1416c7930: key·2^comp pins Lmeas at equilibrium.
## Needs a local rendering device, so it runs under gamescope (see tools/capture_viewer.sh),
## not --headless. Exit 0 pass, 1 fail, 77 skipped.

const SKIP := 77
const TOLERANCE := 5e-4  # half-precision input/output below 1
const WIDTH := 256
const HEIGHT := 16  # 16x16 tiles: one tile per workgroup, exact partial sums
const HALF_BYTES := 2
const CHANNELS := 4
const PUSH_FLOATS := 4  # slots (vec2) + ext (vec2) push block

var failures := 0


func check(cond: bool, what: String) -> void:
	if not cond:
		failures += 1
		printerr("FAIL: ", what)


func _initialize() -> void:
	var rd := RenderingServer.create_local_rendering_device()
	if rd == null:
		print("SKIP: no local rendering device (run under gamescope)")
		quit(SKIP)
		return
	var reduce_shader := _compile(rd, "PASS_REDUCE")
	check(reduce_shader.is_valid(), "REDUCE pass compiles")
	var law_shader := _compile(rd, "PASS_LAW")
	check(law_shader.is_valid(), "LAW pass compiles")
	if reduce_shader.is_valid() and law_shader.is_valid():
		_run_full_rect(rd, reduce_shader, law_shader)
		_run_left_half_rect(rd, reduce_shader, law_shader)
		_run_empty_rect(rd, reduce_shader, law_shader)
	print("post meter: %d failure(s)" % failures)
	quit(1 if failures > 0 else 0)


func _compile(rd: RenderingDevice, pass_def: String) -> RID:
	var body := FileAccess.get_file_as_string("res://post/pes_post.glsl").replace("#version 450\n", "")
	var source := RDShaderSource.new()
	source.language = RenderingDevice.SHADER_LANGUAGE_GLSL
	source.source_compute = "#version 450\n#define %s\n" % pass_def + body
	var spirv := rd.shader_compile_spirv_from_source(source)
	if not spirv.compile_error_compute.is_empty():
		printerr(spirv.compile_error_compute)
		return RID()
	return rd.shader_create_from_spirv(spirv)


## Flat field through REDUCE+LAW. rect = PES's cb5[12] (xy offset, zw scale); the metered
## mean is over the covered pixels only (boundary pixels count when their centre is in [0,1]).
## want_lmeas: the expected mean; want_ev: the expected new EV after one LAW step from ev0.
func _run_meter(rd: RenderingDevice, reduce_shader: RID, law_shader: RID, tag: String,
		field: float, ev0: float, rect: Vector4, want_lmeas: float, want_ev: float) -> void:
	var input := PackedFloat32Array()
	for _i in WIDTH * HEIGHT:
		input.append_array([field, field, field, 1.0])
	var bytes := PackedByteArray()
	bytes.resize(input.size() * HALF_BYTES)
	for k in input.size():
		bytes.encode_half(k * HALF_BYTES, input[k])

	var format := RDTextureFormat.new()
	format.texture_type = RenderingDevice.TEXTURE_TYPE_2D
	format.format = RenderingDevice.DATA_FORMAT_R16G16B16A16_SFLOAT
	format.width = WIDTH
	format.height = HEIGHT
	format.depth = 1
	format.array_layers = 1
	format.mipmaps = 1
	format.usage_bits = RenderingDevice.TEXTURE_USAGE_STORAGE_BIT | RenderingDevice.TEXTURE_USAGE_CAN_UPDATE_BIT \
		| RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT
	var color := rd.texture_create(format, RDTextureView.new(), [bytes])

	var groups := Vector2i((WIDTH + 15) / 16, (HEIGHT + 15) / 16)
	var partial_count := groups.x * groups.y
	var partials := rd.storage_buffer_create(partial_count * 4 * 2)
	var ev_buf := rd.storage_buffer_create(8, PackedFloat32Array([ev0, ev0]).to_byte_array())
	# Params: key 0.18, comp 0 (Lc = Lmeas), wide clamp, speed 10, step gain 1 (full step).
	var params := PackedFloat32Array([0.18, 0.0, -20.0, 20.0, 10.0, 1.0,
		rect.x, rect.y, rect.z, rect.w])
	var params_buf := rd.storage_buffer_create(40, params.to_byte_array())

	var reduce_set := _make_set(rd, reduce_shader, [_image(color), _storage_at(partials, 1), _storage_at(ev_buf, 2), _storage_at(params_buf, 3)])
	var reduce_pipeline := rd.compute_pipeline_create(reduce_shader)
	var law_set := _make_set(rd, law_shader, [_storage_at(partials, 0), _storage_at(ev_buf, 1), _storage_at(params_buf, 2)])
	var law_pipeline := rd.compute_pipeline_create(law_shader)

	var list := rd.compute_list_begin()
	rd.compute_list_bind_compute_pipeline(list, reduce_pipeline)
	rd.compute_list_bind_uniform_set(list, reduce_set, 0)
	# slots: EV read slot 0, write slot 1; ext: (pixels, tile partials).
	var push := PackedFloat32Array([0, 1, WIDTH * HEIGHT, partial_count])
	rd.compute_list_set_push_constant(list, push.to_byte_array(), PUSH_FLOATS * 4)
	rd.compute_list_dispatch(list, groups.x, groups.y, 1)
	rd.compute_list_add_barrier(list)
	rd.compute_list_bind_compute_pipeline(list, law_pipeline)
	rd.compute_list_bind_uniform_set(list, law_set, 0)
	rd.compute_list_set_push_constant(list, push.to_byte_array(), PUSH_FLOATS * 4)
	rd.compute_list_dispatch(list, 1, 1, 1)
	rd.compute_list_end()
	rd.submit()
	rd.sync()

	var got := rd.buffer_get_data(ev_buf).to_float32_array()
	var got_ev := got[1]
	var err := absf(got_ev - want_ev)
	print("%s: field %.4f ev0 %.2f rect %s -> ev %.6f want %.6f (lmeas %.6f)" % [tag, field, ev0, str(rect), got_ev, want_ev, want_lmeas])
	check(err <= 0.02, "%s: EV %.6f, want %.6f" % [tag, got_ev, want_ev])
	rd.free_rid(reduce_set)
	rd.free_rid(reduce_pipeline)
	rd.free_rid(law_set)
	rd.free_rid(law_pipeline)
	rd.free_rid(color)
	rd.free_rid(partials)
	rd.free_rid(ev_buf)
	rd.free_rid(params_buf)


func _image(tex: RID) -> RDUniform:
	var u := RDUniform.new()
	u.uniform_type = RenderingDevice.UNIFORM_TYPE_IMAGE
	u.binding = 0
	u.add_id(tex)
	return u


func _make_set(rd: RenderingDevice, shader: RID, uniforms: Array) -> RID:
	return rd.uniform_set_create(uniforms, shader, 0)


# Full-frame rect: Lmeas = the field; one full LAW step from ev0 = -14.
func _run_full_rect(rd: RenderingDevice, reduce_shader: RID, law_shader: RID) -> void:
	var field := 0.25
	var lmeas := maxf(field * pow(2.0, -14.0), 0.001)
	# target = ev0 + 0.5*(log2(0.181) - log2(lmeas + 0.001)); step gain 1, |Δ| <= 0.5 clamp.
	var target := -14.0 + 0.5 * (log(0.181) / log(2.0) - log(lmeas + 0.001) / log(2.0))
	target = clampf(target, -20.0, 20.0)
	_run_meter(rd, reduce_shader, law_shader, "full", field, -14.0, Vector4(0, 0, 1, 1), lmeas, target)


# Left-half rect on a left-bright/right-dark frame: the meter sees only the bright half.
# ev0 = 0, so nothing floors: Lmeas = bright (dot709 of a grey field), and the dark half
# must not move the EV. A full-frame meter would read (bright + dark) / 2 instead.
func _run_left_half_rect(rd: RenderingDevice, reduce_shader: RID, law_shader: RID) -> void:
	# ndc.x in [-1, 0] maps to uv.x in [0, 0.5] only when offset 0 scale 0.5: covered = left half.
	var bright := 0.4
	var dark := 0.02
	var lmeas := bright
	var target := 0.5 * (log(0.181) / log(2.0) - log(lmeas + 0.001) / log(2.0))
	target = clampf(target, -20.0, 20.0)
	_run_meter_lr(rd, reduce_shader, law_shader, "lefthalf", bright, dark, 0.0, Vector4(0, 0, 0.5, 1), lmeas, target)
	# Control: the same frame metered full-frame settles elsewhere; the rect must matter.
	var full_lmeas := (bright + dark) / 2.0
	var full_target := 0.5 * (log(0.181) / log(2.0) - log(full_lmeas + 0.001) / log(2.0))
	full_target = clampf(full_target, -20.0, 20.0)
	check(absf(target - full_target) > 0.3, "left-half rect differs from full-frame by %.3f" % absf(target - full_target))


# Same but the colour buffer is split bright/dark; REDUCE must ignore the dark half.
func _run_meter_lr(rd: RenderingDevice, reduce_shader: RID, law_shader: RID, tag: String,
		bright: float, dark: float, ev0: float, rect: Vector4, want_lmeas: float, want_ev: float) -> void:
	var input := PackedFloat32Array()
	for y in HEIGHT:
		for x in WIDTH:
			var f := bright if x < WIDTH / 2 else dark
			input.append_array([f, f, f, 1.0])
	var bytes := PackedByteArray()
	bytes.resize(input.size() * HALF_BYTES)
	for k in input.size():
		bytes.encode_half(k * HALF_BYTES, input[k])

	var format := RDTextureFormat.new()
	format.texture_type = RenderingDevice.TEXTURE_TYPE_2D
	format.format = RenderingDevice.DATA_FORMAT_R16G16B16A16_SFLOAT
	format.width = WIDTH
	format.height = HEIGHT
	format.depth = 1
	format.array_layers = 1
	format.mipmaps = 1
	format.usage_bits = RenderingDevice.TEXTURE_USAGE_STORAGE_BIT | RenderingDevice.TEXTURE_USAGE_CAN_UPDATE_BIT \
		| RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT
	var color := rd.texture_create(format, RDTextureView.new(), [bytes])

	var groups := Vector2i((WIDTH + 15) / 16, (HEIGHT + 15) / 16)
	var partial_count := groups.x * groups.y
	var partials := rd.storage_buffer_create(partial_count * 4 * 2)
	var ev_buf := rd.storage_buffer_create(8, PackedFloat32Array([ev0, ev0]).to_byte_array())
	var params := PackedFloat32Array([0.18, 0.0, -20.0, 20.0, 10.0, 1.0,
		rect.x, rect.y, rect.z, rect.w])
	var params_buf := rd.storage_buffer_create(40, params.to_byte_array())

	var reduce_set := _make_set(rd, reduce_shader, [_image(color), _storage_at(partials, 1), _storage_at(ev_buf, 2), _storage_at(params_buf, 3)])
	var reduce_pipeline := rd.compute_pipeline_create(reduce_shader)
	var law_set := _make_set(rd, law_shader, [_storage_at(partials, 0), _storage_at(ev_buf, 1), _storage_at(params_buf, 2)])
	var law_pipeline := rd.compute_pipeline_create(law_shader)

	var list := rd.compute_list_begin()
	rd.compute_list_bind_compute_pipeline(list, reduce_pipeline)
	rd.compute_list_bind_uniform_set(list, reduce_set, 0)
	var push := PackedFloat32Array([0, 1, WIDTH * HEIGHT, partial_count])
	rd.compute_list_set_push_constant(list, push.to_byte_array(), PUSH_FLOATS * 4)
	rd.compute_list_dispatch(list, groups.x, groups.y, 1)
	rd.compute_list_add_barrier(list)
	rd.compute_list_bind_compute_pipeline(list, law_pipeline)
	rd.compute_list_bind_uniform_set(list, law_set, 0)
	rd.compute_list_set_push_constant(list, push.to_byte_array(), PUSH_FLOATS * 4)
	rd.compute_list_dispatch(list, 1, 1, 1)
	rd.compute_list_end()
	rd.submit()
	rd.sync()

	var got := rd.buffer_get_data(ev_buf).to_float32_array()
	var got_ev := got[1]
	print("%s: bright %.3f dark %.3f rect %s -> ev %.6f want %.6f (lmeas %.6f)" % [tag, bright, dark, str(rect), got_ev, want_ev, want_lmeas])
	check(absf(got_ev - want_ev) <= 0.02, "%s: EV %.6f, want %.6f" % [tag, got_ev, want_ev])
	for r in [reduce_set, law_set, reduce_pipeline, law_pipeline, color, partials, ev_buf, params_buf]:
		rd.free_rid(r)


func _storage_at(buf: RID, binding: int) -> RDUniform:
	var u := RDUniform.new()
	u.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	u.binding = binding
	u.add_id(buf)
	return u


# Degenerate rect (offset out of range): PES's in-shader clamp folds it to a point sample of
# the corner texel. On the split frame the clamped corner is dark, so the meter reads dark.
func _run_empty_rect(rd: RenderingDevice, reduce_shader: RID, law_shader: RID) -> void:
	var dark := 0.02
	var lmeas := dark
	var target := 0.5 * (log(0.181) / log(2.0) - log(lmeas + 0.001) / log(2.0))
	target = clampf(target, -20.0, 20.0)
	_run_meter_lr(rd, reduce_shader, law_shader, "corner", 0.4, dark, 0.0, Vector4(1, 1, 1, 1), lmeas, target)
