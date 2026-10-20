extends SceneTree
## GPU check of the match post chain's COMPOSE pass (godot/post/pes_post.glsl). PES's grading pair
## (the ColorCorrection_CubeLut stub, then SdrDisplayMapping) returns the exposed scene and clips it
## to [0, 1]: out = clamp(2^EV * in, 0, 1), alpha passed through. Needs a local rendering device,
## so it runs under gamescope (see tools/capture_viewer.sh), not --headless. Exit 0 pass, 1 fail,
## 77 skipped.

const SKIP := 77
const WIDTH := 256              # ramp pixels in one row
# The pass reads and writes RGBA16F, so the expectation uses the half-precision input the pass
# reads. One half ulp below 1 is 4.9e-4; the pair itself is exact to ~1e-6.
const TOLERANCE := 5e-4
const ALPHA := 0.75             # exact in half precision; the pair must pass it through
const RAMP_LOW := -0.25         # below 0 clips to 0
const RAMP_HIGH := 1.75         # above 1 clips to 1
const EV_CASES := [0.0, -1.0]   # exposure: 2^EV scales the scene before the pair (EV -1 halves it)
const PUSH_FLOATS := 4          # slots (vec2) + ext (vec2) push block
const HALF_BYTES := 2
const CHANNELS := 4

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
	var shader := _compile(rd)
	check(shader.is_valid(), "COMPOSE pass compiles")
	if shader.is_valid():
		for ev: float in EV_CASES:
			_run_case(rd, shader, ev)
	print("post compose: %d failure(s)" % failures)
	quit(1 if failures > 0 else 0)


func _compile(rd: RenderingDevice) -> RID:
	var body := FileAccess.get_file_as_string("res://post/pes_post.glsl").replace("#version 450\n", "")
	var source := RDShaderSource.new()
	source.language = RenderingDevice.SHADER_LANGUAGE_GLSL
	source.source_compute = "#version 450\n#define PASS_COMPOSE\n" + body
	var spirv := rd.shader_compile_spirv_from_source(source)
	if not spirv.compile_error_compute.is_empty():
		printerr(spirv.compile_error_compute)
		return RID()
	return rd.shader_create_from_spirv(spirv)


## One ramp row through the pass at a given EV; every channel follows its own affine ramp so a
## channel swap or a per-channel error shows up.
func _run_case(rd: RenderingDevice, shader: RID, ev: float) -> void:
	var input := PackedFloat32Array()
	for i in WIDTH:
		var s := RAMP_LOW + (RAMP_HIGH - RAMP_LOW) * float(i) / float(WIDTH - 1)
		input.append_array([s, 0.5 * s + 0.25, 0.2 - 0.3 * s, ALPHA])
	var bytes := PackedByteArray()
	bytes.resize(input.size() * HALF_BYTES)
	var stored := PackedFloat32Array()  # the half-precision values the pass reads
	for k in input.size():
		bytes.encode_half(k * HALF_BYTES, input[k])
		stored.append(bytes.decode_half(k * HALF_BYTES))

	var format := RDTextureFormat.new()
	format.texture_type = RenderingDevice.TEXTURE_TYPE_2D
	format.format = RenderingDevice.DATA_FORMAT_R16G16B16A16_SFLOAT
	format.width = WIDTH
	format.height = 1
	format.depth = 1
	format.array_layers = 1
	format.mipmaps = 1
	format.usage_bits = RenderingDevice.TEXTURE_USAGE_STORAGE_BIT | RenderingDevice.TEXTURE_USAGE_CAN_UPDATE_BIT \
		| RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT
	var color := rd.texture_create(format, RDTextureView.new(), [bytes])

	var ev_buf := rd.storage_buffer_create(8, PackedFloat32Array([ev, ev]).to_byte_array())
	var image := RDUniform.new()
	image.uniform_type = RenderingDevice.UNIFORM_TYPE_IMAGE
	image.binding = 0
	image.add_id(color)
	var ev_u := RDUniform.new()
	ev_u.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	ev_u.binding = 1
	ev_u.add_id(ev_buf)
	var set := rd.uniform_set_create([image, ev_u], shader, 0)
	var pipeline := rd.compute_pipeline_create(shader)

	var list := rd.compute_list_begin()
	rd.compute_list_bind_compute_pipeline(list, pipeline)
	rd.compute_list_bind_uniform_set(list, set, 0)
	var push := PackedFloat32Array([0, 1, 0, 0])  # slots: EV read slot 0, write slot 1
	rd.compute_list_set_push_constant(list, push.to_byte_array(), PUSH_FLOATS * 4)
	rd.compute_list_dispatch(list, (WIDTH + 7) / 8, 1, 1)
	rd.compute_list_end()
	rd.submit()
	rd.sync()

	var out := rd.texture_get_data(color, 0)
	var worst := 0.0
	var bad := 0
	for i in WIDTH:
		for c in 3:
			var x := stored[i * CHANNELS + c]
			var want := clampf(x * pow(2.0, ev), 0.0, 1.0)
			var got := out.decode_half((i * CHANNELS + c) * HALF_BYTES)
			var err := absf(got - want)
			worst = maxf(worst, err)
			if err > TOLERANCE:
				bad += 1
				if bad <= 4:
					printerr("ev=%.1f in=%.4f (ch %d): out %.5f want %.5f" % [ev, x, c, got, want])
		var alpha := out.decode_half((i * CHANNELS + 3) * HALF_BYTES)
		if absf(alpha - ALPHA) > TOLERANCE:
			bad += 1
	print("ev=%.1f: worst colour error %.6f, bad values %d" % [ev, worst, bad])
	check(bad == 0, "compose at ev %.1f matches clamp(2^ev * in, 0, 1), alpha kept" % ev)
	rd.free_rid(set)
	rd.free_rid(pipeline)
	rd.free_rid(ev_buf)
	rd.free_rid(color)
