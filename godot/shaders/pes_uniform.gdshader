// PES 2021 uniform cloth: pes3ddf_{shirt_nb,collar,sleeve,pants_nb} and their
// _NC variants (is_nc), and pes3DDF_Uniform (socks, is_socks). Formulas:
// docs/re/pes-re-shaders.md 3.1-3.2. The engine's Repetition and BlendNormalX
// values are set by pes_character.cpp from docs/re/pes-re-uniform-params.md.
// Kit atlas on UV0 (alpha = kit coverage a); fabric weave (uni_pattern array)
// and wrinkle normals (shirts_normal / pants_normal arrays) on UV1; name and
// numbers on UV0. Non-NC parts cull back faces (discarded here, since Godot's
// render_mode is static); _NC parts draw them with specular zeroed. Not drawn:
// stain (live Stain 0.001 gives s = 0) and wetness (Wet = 0 in every live kit).
// Pose-driven BlendNormalX mix and detail weights stay the fmdl's static values.
shader_type spatial;
render_mode cull_disabled;
#include "res://shaders/pes_common.gdshaderinc"

uniform sampler2D kit_atlas : source_color, filter_linear_mipmap_anisotropic;
uniform sampler2D kit_srm : hint_default_white, filter_linear_mipmap;  // R spec, G rough
uniform bool has_kit_srm = false;
uniform sampler2DArray pattern : filter_linear_mipmap;   // uni_pattern
uniform sampler2DArray wrinkles : filter_linear_mipmap;  // shirts_normal / pants_normal
uniform bool has_wrinkles = false;
uniform sampler2D socks_normal : hint_normal, filter_linear_mipmap;  // socks only, UV1
uniform vec4 repetition = vec4(25.0, 50.0, 0.0, 0.0);  // RepetitionParam
uniform vec4 blend_normal_x = vec4(0.0);                // BlendNormalXParam
uniform vec4 blend_normal_leg = vec4(0.0);              // BlendNormalLegParam
uniform bool is_socks = false;
uniform bool is_nc = true;  // technique ends in _NC
// Shirt/collar: name u in [0,0.64), back number [0.64,0.88), chest [0.88,1)x[0,0.5).
uniform sampler2D Marking_Tex_LIN : filter_linear_mipmap;  // LIN role: no sRGB decode
uniform bool has_marking = false;
uniform vec4 NameScaleXParam = vec4(0.0);
uniform vec4 BNumberScaleXParam = vec4(0.0);
uniform vec4 FNumberScaleXParam = vec4(0.0);
// Pants: the leg number over the whole 128x128 texture.
uniform sampler2D Number_Tex_LIN : filter_linear_mipmap;
uniform bool has_number = false;
uniform vec4 NumberScaleXParam = vec4(0.0);
// Where a marking lies, roughness/specular take the print's values.
const vec2 MARKING_SRM = vec2(0.27, 0.629);
const float BACK_NUMBER_V_SCALE = 160.0 / 255.0;

// The shirt family draws an opaque kit at 85% of its texel colour.
const float OPAQUE_KIT_ALBEDO = 0.85;

// Slice-1 normal: its x is in A and its y in G (pes-re-shaders.md 3.1).
vec3 pes_normal_ag(vec4 t) {
	vec2 xy = vec2(t.a, t.g) * 2.0 - 1.0;
	return vec3(xy, sqrt(max(0.0, 1.0 - dot(xy, xy))));
}

// One part of the marking texture: its [0,1]^2 placement mapped into an atlas
// region; outside the rectangle it contributes nothing (premultiplied).
vec4 marking_part(sampler2D tex, vec2 q, vec2 origin, vec2 size) {
	if (any(lessThan(q, vec2(0.0))) || any(greaterThan(q, vec2(1.0)))) {
		return vec4(0.0);
	}
	vec4 t = texture(tex, origin + q * size);
	return vec4(t.rgb * t.a, t.a);
}

// Shirt-family wrinkle normal (pes-re-shaders.md 3.1): wrinkle slice nA picked by
// BlendNormalX.y, mixed with a neighbour slice nB by BlendNormalX.x; slice 1 and
// slice 3 then add their detail through the weights in BlendNormalX.zw and
// BlendNormalLeg.xy.
vec3 wrinkle_normal(vec2 uv) {  // uv = UV2 (built-ins are not visible in helper functions)
	float y = blend_normal_x.y;
	vec3 na = pes_normal(texture(wrinkles, vec3(uv, y)));
	vec3 nb = pes_normal(texture(wrinkles, vec3(uv, (y + 1.0) * ceil(blend_normal_x.x - 0.5))));
	vec3 n = normalize(mix(nb, na, 1.0 - 2.0 * abs(blend_normal_x.x - 0.5)));
	vec4 s1 = texture(wrinkles, vec3(uv, 1.0));
	vec3 n1 = pes_normal_ag(s1);
	n = normalize(mix(n, n1, dot(blend_normal_x.zw, vec2(s1.r, s1.b))));
	vec4 s3 = texture(wrinkles, vec3(uv, 3.0));
	return normalize(mix(n, n1, dot(blend_normal_leg.xy, vec2(s3.r, s3.b))));
}

void fragment() {
	if (!is_nc && !FRONT_FACING) discard;  // PES culls back faces of non-NC cloth
	vec4 base = texture(kit_atlas, UV);
	vec4 m = vec4(0.0);  // premultiplied markings
	if (has_marking) {
		vec2 qn = vec2((UV.x - 0.5) * NameScaleXParam.x + 0.5, UV.y * NameScaleXParam.y + NameScaleXParam.z);
		vec4 name = marking_part(Marking_Tex_LIN, qn, vec2(0.0), vec2(0.64, 1.0));
		// Back number (shirt_nb_nc.ps: cb4[4]): centred in u like the name, v scaled
		// by 160/255 before the offset; .w unused.
		vec2 qb = vec2((UV.x - 0.5) * BNumberScaleXParam.x + 0.5,
				BACK_NUMBER_V_SCALE * UV.y * BNumberScaleXParam.y + BNumberScaleXParam.z);
		vec4 back = marking_part(Marking_Tex_LIN, qb, vec2(0.64, 0.0), vec2(0.24, 1.0));
		vec2 qf = UV * FNumberScaleXParam.xy + FNumberScaleXParam.zw;
		vec4 front = marking_part(Marking_Tex_LIN, qf, vec2(0.88, 0.0), vec2(0.12, 0.5));
		m = name + back * (1.0 - name.a);
		m = m + front * (1.0 - m.a);
	} else if (has_number) {
		m = marking_part(Number_Tex_LIN, UV * NumberScaleXParam.xy + NumberScaleXParam.zw, vec2(0.0), vec2(1.0));
	}
	base.rgb = mix(base.rgb, m.a > 0.0 ? m.rgb / m.a : base.rgb, m.a);
	float a = max(m.a, base.a);
	vec2 weave_uv = is_socks ? UV * repetition.x : UV2 * repetition.xy;
	float slice = 2.0 * repetition.z;
	vec4 p0 = texture(pattern, vec3(weave_uv, slice));
	vec4 p1 = texture(pattern, vec3(weave_uv, slice + 1.0));
	vec2 srm = has_kit_srm ? texture(kit_srm, UV).rg : vec2(p1.x, p1.y);
	srm = mix(srm, MARKING_SRM, m.a);

	ALBEDO = is_socks ? base.rgb * max(a, p0.x) : base.rgb * mix(p0.x, OPAQUE_KIT_ALBEDO, a);
	ROUGHNESS = mix(p1.y, srm.g, a);  // no f() remap for cloth (pes-re-shaders.md 3.1)
	// GB2.y is multiplied by frontFace on _NC parts; PES's specular is linear in
	// that intensity, and Godot's specular 0.5 is a 0.04 F0 (pes_specular).
	float spec = mix(p1.x, srm.r, a);
	if (is_nc && !FRONT_FACING) spec = 0.0;
	SPECULAR = pes_specular(spec);

	vec3 n = vec3(0.0, 0.0, 1.0);
	if (is_socks) {
		n = pes_normal(texture(socks_normal, UV2));
	} else if (has_wrinkles) {
		n = wrinkle_normal(UV2);
	}
	vec3 weave_n = pes_normal(vec4(p0.x, p0.y, 0.0, p0.w));
	vec3 nd = normalize(mix(weave_n, n, a));
	vec3 nt = normalize(vec3(nd.xy + n.xy, n.z));
	NORMAL_MAP = nt * 0.5 + 0.5;
}
