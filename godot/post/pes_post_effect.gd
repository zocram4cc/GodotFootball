class_name PesPostEffect
extends CompositorEffect
## The PES post chain on the colour buffer after the transparent pass
## (docs/re/pes-re-lighting.md Part C): eye adaptation, Kawase bloom, white
## balance, the FilmToneMap curve and the display encode. PesLighting supplies
## the rig's values through configure(). Not applied, and open in the report:
## the 33³ grading LUT, AA, sharpening and grain.

const SHADER_PATH := "res://post/pes_post.glsl"
const PASSES := ["reduce", "law", "bright", "kawase", "compose"]
const PARAM_FLOATS := 32  # Params layout: see pes_post.glsl and pes_lighting.cpp
const PARAM_BYTES := 128
const PUSH_BYTES := 48    # three vec4 push constants
const REDUCE_TILE := 16   # reduce kernel's local size
const WORK_TILE := 8      # bright, kawase and compose local size
const LAW_THREADS := 256  # law kernel's local size, one group
const QUARTER_SHIFT := 2  # PES's bloom target is a quarter of the colour buffer (>> 2)

var _rd: RenderingDevice
var _compiled := false
var _shader := {}
var _pipeline := {}
var _sets := {}
var _sampler := RID()
var _ev := RID()           # two EV slots; the read and write slot swap each frame
var _params_buf := RID()
var _params := PackedFloat32Array()
var _params_dirty := false
var _initial_ev := -14.0
var _ev_dirty := true
var _ev_read := 0
var _size := Vector2i.ZERO
var _color := RID()
var _bright := RID()
var _bloom := RID()
var _partials := RID()
var _partial_count := 0
var _pixel_count := 0


func _init() -> void:
	effect_callback_type = EFFECT_CALLBACK_TYPE_POST_TRANSPARENT
	access_resolved_color = false
	access_resolved_depth = false
	_rd = RenderingServer.get_rendering_device()


## post: {"params": PackedFloat32Array(32), "initial_ev": float}. Resets the
## adaptation to the rig's initial exposure.
func configure(post: Dictionary) -> void:
	_params = post["params"]
	_params_dirty = true
	_initial_ev = float(post["initial_ev"])
	_ev_dirty = true


func _render_callback(effect_callback_type: int, render_data: RenderData) -> void:
	if effect_callback_type != EFFECT_CALLBACK_TYPE_POST_TRANSPARENT or _rd == null:
		return
	if _params.size() != PARAM_FLOATS:
		return
	var buffers := render_data.get_render_scene_buffers() as RenderSceneBuffersRD
	if buffers == null:
		return
	var size := buffers.get_internal_size()
	var color := buffers.get_color_layer(0)
	if size.x < 1 or size.y < 1 or not color.is_valid():
		return
	if not _compiled and not _compile():
		return
	_ensure_base()
	if size != _size or color != _color:
		_free_targets()
		_create_targets(size, color)
	if _params_dirty:
		_rd.buffer_update(_params_buf, 0, PARAM_BYTES, _params.to_byte_array())
		_params_dirty = false
	if _ev_dirty:
		_rd.buffer_update(_ev, 0, 8, PackedFloat32Array([_initial_ev, _initial_ev]).to_byte_array())
		_ev_dirty = false
		_ev_read = 0
	_run(size)


func _run(size: Vector2i) -> void:
	var ev_write := 1 - _ev_read
	var idx := Vector4(_ev_read, ev_write, 0, 0)
	var ext := Vector4(_pixel_count, _partial_count, 0, 0)
	var quarter := Vector2i(maxi(size.x >> QUARTER_SHIFT, 1), maxi(size.y >> QUARTER_SHIFT, 1))
	var reduce_groups := Vector3i((size.x + REDUCE_TILE - 1) / REDUCE_TILE, (size.y + REDUCE_TILE - 1) / REDUCE_TILE, 1)
	var work_groups := Vector3i((size.x + WORK_TILE - 1) / WORK_TILE, (size.y + WORK_TILE - 1) / WORK_TILE, 1)
	var quarter_groups := Vector3i((quarter.x + WORK_TILE - 1) / WORK_TILE, (quarter.y + WORK_TILE - 1) / WORK_TILE, 1)
	var list := _rd.compute_list_begin()
	_pass(list, "reduce", reduce_groups, idx, Vector4(), ext)
	_rd.compute_list_add_barrier(list)
	_pass(list, "law", Vector3i(1, 1, 1), idx, Vector4(), ext)
	_rd.compute_list_add_barrier(list)
	_pass(list, "bright", quarter_groups, idx, Vector4(), ext)
	_rd.compute_list_add_barrier(list)
	# Bloom kernels (FUN_141804af0): floor(10·size) passes at least one; the
	# first carries the weight, each adds w/N. A zero offset scale is the copy branch.
	var weight := _params[27]
	var kernels := int(_params[28])
	var offset_scale := _params[29]
	var first_weight := weight if offset_scale != 0.0 else 1.0
	for i in kernels:
		var offset := (float(i) + 0.5) * offset_scale
		var z := first_weight if i == 0 else 1.0
		_pass(list, "kawase", quarter_groups, Vector4(_ev_read, ev_write, i, 1.0 if i == 0 else 0.0),
			Vector4(offset, z, weight / float(kernels), 0), ext)
		_rd.compute_list_add_barrier(list)
	_pass(list, "compose", work_groups, idx, Vector4(), ext)
	_rd.compute_list_end()
	_ev_read = ev_write


func _pass(list: int, pass_name: String, groups: Vector3i, idx: Vector4, kern: Vector4, ext: Vector4) -> void:
	_rd.compute_list_bind_compute_pipeline(list, _pipeline[pass_name])
	_rd.compute_list_bind_uniform_set(list, _sets[pass_name], 0)
	var push := PackedFloat32Array([idx.x, idx.y, idx.z, idx.w, kern.x, kern.y, kern.z, kern.w,
		ext.x, ext.y, ext.z, ext.w])
	_rd.compute_list_set_push_constant(list, push.to_byte_array(), PUSH_BYTES)
	_rd.compute_list_dispatch(list, groups.x, groups.y, groups.z)


func _compile() -> bool:
	var body := FileAccess.get_file_as_string(SHADER_PATH)
	if body.is_empty():
		push_error("PesPostEffect: missing %s" % SHADER_PATH)
		return false
	body = body.replace("#version 450\n", "")
	for pass_name: String in PASSES:
		var source := RDShaderSource.new()
		source.language = RenderingDevice.SHADER_LANGUAGE_GLSL
		source.source_compute = "#version 450\n#define PASS_%s\n%s" % [pass_name.to_upper(), body]
		var spirv := _rd.shader_compile_spirv_from_source(source)
		if not spirv.compile_error_compute.is_empty():
			push_error("PesPostEffect %s: %s" % [pass_name, spirv.compile_error_compute])
			return false
		var shader := _rd.shader_create_from_spirv(spirv)
		_shader[pass_name] = shader
		_pipeline[pass_name] = _rd.compute_pipeline_create(shader)
	_compiled = true
	return true


func _ensure_base() -> void:
	if _sampler.is_valid():
		return
	var state := RDSamplerState.new()
	state.min_filter = RenderingDevice.SAMPLER_FILTER_LINEAR
	state.mag_filter = RenderingDevice.SAMPLER_FILTER_LINEAR
	state.repeat_u = RenderingDevice.SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE
	state.repeat_v = RenderingDevice.SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE
	_sampler = _rd.sampler_create(state)
	_ev = _rd.storage_buffer_create(8, PackedFloat32Array([_initial_ev, _initial_ev]).to_byte_array())
	_params_buf = _rd.storage_buffer_create(PARAM_BYTES, _params.to_byte_array())


func _create_targets(size: Vector2i, color: RID) -> void:
	var quarter := Vector2i(maxi(size.x >> QUARTER_SHIFT, 1), maxi(size.y >> QUARTER_SHIFT, 1))
	var format := RDTextureFormat.new()
	format.texture_type = RenderingDevice.TEXTURE_TYPE_2D
	format.format = RenderingDevice.DATA_FORMAT_R16G16B16A16_SFLOAT
	format.width = quarter.x
	format.height = quarter.y
	format.depth = 1
	format.array_layers = 1
	format.mipmaps = 1
	format.usage_bits = RenderingDevice.TEXTURE_USAGE_STORAGE_BIT | RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT
	_bright = _rd.texture_create(format, RDTextureView.new())
	_bloom = _rd.texture_create(format, RDTextureView.new())
	var reduce_groups := Vector2i((size.x + REDUCE_TILE - 1) / REDUCE_TILE, (size.y + REDUCE_TILE - 1) / REDUCE_TILE)
	_partial_count = reduce_groups.x * reduce_groups.y
	_partials = _rd.storage_buffer_create(_partial_count * 4)
	_pixel_count = size.x * size.y
	_sets["reduce"] = _rd.uniform_set_create([
		_image(0, color), _storage(1, _partials), _storage(2, _ev)], _shader["reduce"], 0)
	_sets["law"] = _rd.uniform_set_create([
		_storage(0, _partials), _storage(1, _ev), _storage(2, _params_buf)], _shader["law"], 0)
	_sets["bright"] = _rd.uniform_set_create([
		_sampled(0, color), _image(1, _bright), _storage(2, _ev), _storage(3, _params_buf)], _shader["bright"], 0)
	_sets["kawase"] = _rd.uniform_set_create([
		_sampled(0, _bright), _image(1, _bloom)], _shader["kawase"], 0)
	_sets["compose"] = _rd.uniform_set_create([
		_image(0, color), _sampled(1, _bloom), _storage(2, _ev), _storage(3, _params_buf)], _shader["compose"], 0)
	_size = size
	_color = color


func _free_targets() -> void:
	for pass_name in _sets:
		_rd.free_rid(_sets[pass_name])
	_sets.clear()
	for rid: RID in [_bright, _bloom, _partials]:
		if rid.is_valid():
			_rd.free_rid(rid)
	_bright = RID()
	_bloom = RID()
	_partials = RID()
	_size = Vector2i.ZERO
	_color = RID()


func _image(binding: int, tex: RID) -> RDUniform:
	var u := RDUniform.new()
	u.uniform_type = RenderingDevice.UNIFORM_TYPE_IMAGE
	u.binding = binding
	u.add_id(tex)
	return u


func _sampled(binding: int, tex: RID) -> RDUniform:
	var u := RDUniform.new()
	u.uniform_type = RenderingDevice.UNIFORM_TYPE_SAMPLER_WITH_TEXTURE
	u.binding = binding
	u.add_id(_sampler)
	u.add_id(tex)
	return u


func _storage(binding: int, buf: RID) -> RDUniform:
	var u := RDUniform.new()
	u.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	u.binding = binding
	u.add_id(buf)
	return u
