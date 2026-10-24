class_name PesPostEffect
extends CompositorEffect
## The PES post chain on the colour buffer after the transparent pass
## (docs/re/pes-re-lighting.md Part C, docs/render/pes-render-grading-lut.md): eye adaptation, then
## PES's grading pair (the ColorCorrection_CubeLut stub and SdrDisplayMapping). PesLighting supplies
## the rig's values through configure(). Not applied, and open in the report: AA, sharpening and
## grain.

const SHADER_PATH := "res://post/pes_post.glsl"
const PASSES := ["reduce", "law", "compose"]
const PARAM_FLOATS := 10  # Params layout: see pes_post.glsl and pes_lighting.cpp
const PARAM_BYTES := 40
const PUSH_BYTES := 16    # two vec2 push constants
const REDUCE_TILE := 16   # reduce kernel's local size
const WORK_TILE := 8      # compose kernel's local size
const LAW_THREADS := 256  # law kernel's local size, one group

var _rd: RenderingDevice
var _compiled := false
var _shader := {}
var _pipeline := {}
var _sets := {}
var _ev := RID()           # two EV slots; the read and write slot swap each frame
var _params_buf := RID()
var _params := PackedFloat32Array()
var _params_dirty := false
var _initial_ev := -14.0
var _ev_dirty := true
var _ev_read := 0
var _size := Vector2i.ZERO
var _color := RID()
var _partials := RID()
var _partial_count := 0
var _pixel_count := 0


func _init() -> void:
	effect_callback_type = EFFECT_CALLBACK_TYPE_POST_TRANSPARENT
	access_resolved_color = false
	access_resolved_depth = false
	_rd = RenderingServer.get_rendering_device()


## post: {"params": PackedFloat32Array(PARAM_FLOATS), "initial_ev": float}. Resets the
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
	var slots := Vector2(_ev_read, ev_write)
	var ext := Vector2(_pixel_count, _partial_count)
	var reduce_groups := Vector3i((size.x + REDUCE_TILE - 1) / REDUCE_TILE, (size.y + REDUCE_TILE - 1) / REDUCE_TILE, 1)
	var work_groups := Vector3i((size.x + WORK_TILE - 1) / WORK_TILE, (size.y + WORK_TILE - 1) / WORK_TILE, 1)
	var list := _rd.compute_list_begin()
	_pass(list, "reduce", reduce_groups, slots, ext)
	_rd.compute_list_add_barrier(list)
	_pass(list, "law", Vector3i(1, 1, 1), slots, ext)
	_rd.compute_list_add_barrier(list)
	_pass(list, "compose", work_groups, slots, ext)
	_rd.compute_list_end()
	_ev_read = ev_write


func _pass(list: int, pass_name: String, groups: Vector3i, slots: Vector2, ext: Vector2) -> void:
	_rd.compute_list_bind_compute_pipeline(list, _pipeline[pass_name])
	_rd.compute_list_bind_uniform_set(list, _sets[pass_name], 0)
	var push := PackedFloat32Array([slots.x, slots.y, ext.x, ext.y])
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
	if _ev.is_valid():
		return
	_ev = _rd.storage_buffer_create(8, PackedFloat32Array([_initial_ev, _initial_ev]).to_byte_array())
	_params_buf = _rd.storage_buffer_create(PARAM_BYTES, _params.to_byte_array())


func _create_targets(size: Vector2i, color: RID) -> void:
	var reduce_groups := Vector2i((size.x + REDUCE_TILE - 1) / REDUCE_TILE, (size.y + REDUCE_TILE - 1) / REDUCE_TILE)
	_partial_count = reduce_groups.x * reduce_groups.y
	_partials = _rd.storage_buffer_create(_partial_count * 4)
	_pixel_count = size.x * size.y
	_sets["reduce"] = _rd.uniform_set_create([
		_image(0, color), _storage(1, _partials), _storage(2, _ev), _storage(3, _params_buf)], _shader["reduce"], 0)
	_sets["law"] = _rd.uniform_set_create([
		_storage(0, _partials), _storage(1, _ev), _storage(2, _params_buf)], _shader["law"], 0)
	_sets["compose"] = _rd.uniform_set_create([
		_image(0, color), _storage(1, _ev)], _shader["compose"], 0)
	_size = size
	_color = color


func _free_targets() -> void:
	for pass_name in _sets:
		_rd.free_rid(_sets[pass_name])
	_sets.clear()
	if _partials.is_valid():
		_rd.free_rid(_partials)
	_partials = RID()
	_size = Vector2i.ZERO
	_color = RID()


func _image(binding: int, tex: RID) -> RDUniform:
	var u := RDUniform.new()
	u.uniform_type = RenderingDevice.UNIFORM_TYPE_IMAGE
	u.binding = binding
	u.add_id(tex)
	return u


func _storage(binding: int, buf: RID) -> RDUniform:
	var u := RDUniform.new()
	u.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	u.binding = binding
	u.add_id(buf)
	return u
