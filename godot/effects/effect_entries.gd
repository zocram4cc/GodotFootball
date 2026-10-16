extends RefCounted
## Effect entries of one effect_config.xml (PesEffects.placement) as scene nodes.
## Placed: Vignetting (screen overlay), MultiUvScrollScreen (rain screen), RainDemo (rain
## streaks). Not placed, reported by PesEffects.placement(): LightBillboard (its locator models are
## not decoded; st002's are 32 zero bytes), LensFlare (layer and scale units not decoded),
## Halo (scale units not decoded). Positions are Fox space, as the static models are.

const SCREEN_SHADER := "res://effects/screen_rain.gdshader"
const PES_ASSETS := "Assets/pes16/"
const VFS_ASSETS := "Asset/"
const PLATFORM_DIR := "#windx11/"
const CHANNEL_MAX := 255.0  # 8-bit alpha: the vignette's alpha param and the rgba colours
const SCREEN_ASPECT := 16.0 / 9.0  # the capture is 1280x720
const RAIN_FALL_HEIGHT_M := 10.0  # ponytail: the emitter box height is not in the config; 10 m assumed
const RAIN_ZONE_SHARE := 0.5  # ponytail: the config does not split num_unit between the zones; half each


## A PES texture path from the config (/Assets/pes16/<dir>/<name>) as its VFS path
## (Asset/<dir>/#windx11/<name>): the platform folder sits in front of the file name.
static func vfs_texture(pes_path: String) -> String:
	var path := pes_path.trim_prefix("/").trim_prefix(PES_ASSETS)
	var slash := path.rfind("/")
	return VFS_ASSETS + path.substr(0, slash + 1) + PLATFORM_DIR + path.substr(slash + 1)


## The numbers of one param (converted by the parser), [] when absent. wanted_index -1
## matches a param with no index attribute.
static func values(setting: Dictionary, id: String, wanted_index := -1) -> Array:
	return _param(setting, id, wanted_index).get("values", [])


## The text of one param (paths), "" when absent.
static func text(setting: Dictionary, id: String, wanted_index := -1) -> String:
	return _param(setting, id, wanted_index).get("text", "")


## Adds every placed entry: screen overlays under `screen`, rain under the camera (it moves
## with the view). Returns how many entries were placed.
static func build_entries(camera: Camera3D, screen: CanvasLayer, library: PesLibrary, placed: Array) -> int:
	for entry in placed:
		var s: Dictionary = entry["setting"]
		match entry["type"]:
			"Vignetting":
				screen.add_child(_vignette(s, library))
			"MultiUvScrollScreen":
				camera.add_child(_screen_rain(s, library, camera.fov))
			"RainDemo":
				camera.add_child(_rain_streaks(s, library))
	return placed.size()


static func _param(setting: Dictionary, id: String, wanted_index: int) -> Dictionary:
	for p in setting.get("params", []):
		if p["id"] == id and int(p["index"]) == wanted_index:
			return p
	return {}


# Vignetting: a full-screen overlay. Primitive_Vignetting writes rgb = 0 and blends by
# alpha, so the overlay is black and its strength is the alpha param.
static func _vignette(s: Dictionary, library: PesLibrary) -> TextureRect:
	var rect := TextureRect.new()
	rect.name = "Vignetting"
	rect.texture = library.load_texture(vfs_texture(text(s, "texturePath")))
	rect.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	rect.stretch_mode = TextureRect.STRETCH_SCALE
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	rect.modulate = Color(0.0, 0.0, 0.0, values(s, "alpha")[0] / CHANNEL_MAX)
	return rect


# MultiUvScrollScreen: two full-screen quads at `dist` in front of the camera. Layer i
# scrolls its texture at speed_u/v[i] and repeats it wrap_rate_u/v[i] times per screen,
# tinted by colors[4 i] (rgba; its alpha byte is the strength).
static func _screen_rain(s: Dictionary, library: PesLibrary, fov_deg: float) -> Node3D:
	var root := Node3D.new()
	root.name = "MultiUvScrollScreen"
	var dist: float = values(s, "dist")[0]
	var half_h := dist * tan(deg_to_rad(fov_deg) * 0.5)
	for layer in 2:
		var mat := ShaderMaterial.new()
		mat.shader = load(SCREEN_SHADER)
		mat.set_shader_parameter("rain_texture", library.load_texture(vfs_texture(text(s, "texturePath", layer))))
		mat.set_shader_parameter("speed_uv", Vector2(values(s, "speed_u", layer)[0], values(s, "speed_v", layer)[0]))
		mat.set_shader_parameter("wrap_rate", Vector2(values(s, "wrap_rate_u", layer)[0], values(s, "wrap_rate_v", layer)[0]))
		var c: Array = values(s, "colors", layer * 4)
		mat.set_shader_parameter("tint", Color(c[0], c[1], c[2], c[3]))
		var mesh := QuadMesh.new()
		mesh.size = Vector2(2.0 * half_h * SCREEN_ASPECT, 2.0 * half_h)
		mesh.material = mat
		var quad := MeshInstance3D.new()
		quad.name = "layer%d" % layer
		quad.mesh = mesh
		quad.position = Vector3(0.0, 0.0, -dist)
		root.add_child(quad)
	return root


# RainDemo: a near-zone and a far-zone particle emitter. Each is a box of area_x by area_z
# metres at its zone distance in front of the camera, emitting streak quads that fall at
# the zone speed (m/s) and are recycled when they leave the box.
static func _rain_streaks(s: Dictionary, library: PesLibrary) -> Node3D:
	var root := Node3D.new()
	root.name = "RainDemo"
	var streak := Vector2(values(s, "width")[0], values(s, "height")[0])
	var tex := library.load_texture(vfs_texture(text(s, "texturePath")))
	var units: int = values(s, "num_unit")[0]
	for zone in ["near", "far"]:
		var speed: float = values(s, zone + "_zone_speed")[0]
		var dist: float = values(s, zone + "_zone_dist")[0]
		var area_x: float = values(s, zone + "_zone_area_x")[0]
		var area_z: float = values(s, zone + "_zone_area_z")[0]
		var c: Array = values(s, zone + "_zone_colors", 0)
		var process := ParticleProcessMaterial.new()
		process.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
		process.emission_box_extents = Vector3(area_x * 0.5, RAIN_FALL_HEIGHT_M * 0.5, area_z * 0.5)
		process.direction = Vector3.DOWN
		process.spread = 0.0
		process.initial_velocity_min = speed
		process.initial_velocity_max = speed
		process.gravity = Vector3.ZERO
		var mat := StandardMaterial3D.new()
		mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		mat.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
		mat.albedo_texture = tex
		mat.albedo_color = Color(c[0], c[1], c[2], c[3])
		var quad := QuadMesh.new()
		quad.size = streak
		quad.material = mat
		var emitter := GPUParticles3D.new()
		emitter.name = zone
		emitter.amount = int(units * RAIN_ZONE_SHARE)
		emitter.lifetime = RAIN_FALL_HEIGHT_M / speed
		emitter.preprocess = emitter.lifetime
		emitter.process_material = process
		emitter.draw_pass_1 = quad
		emitter.position = Vector3(0.0, 0.0, -dist)
		root.add_child(emitter)
	return root
