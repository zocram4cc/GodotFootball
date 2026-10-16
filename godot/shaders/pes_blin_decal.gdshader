// PES fox3DDC_Blin: the pitch_scratch re-turf patch (fox3ddf_blin_decal, a
// Deferred_Decal). Blin family, shading model 0. Ported from
// docs/re/pes-re-pitch-grass.md section 4 (fox3ddf_blin_decal.ps.asm): the albedo
// is unmodified, the decal weight is base.a, roughness is f(sqrt(srm.g^2 + sigma^2)),
// specular is srm.r. The light is Godot's own, as in pes_blin.gdshader.
//
// [DEVIATION 1] As pes_ggx_decal.gdshader: the dither coverage is open
// (pes-re-shaders.md 8.1), so the decal is its alpha blend alone.
shader_type spatial;
render_mode blend_mix, depth_draw_never, shadows_disabled;
#include "res://shaders/pes_common.gdshaderinc"

uniform sampler2D Base_Tex_SRGB : filter_linear_mipmap_anisotropic;      // scratch colour: RGB sRGB, A coverage
uniform sampler2D NormalMap_Tex_NRM : filter_linear_mipmap_anisotropic;  // scratch normal
uniform bool has_NormalMap_Tex_NRM = false;
uniform sampler2D SpecularMap_Tex_LIN : hint_default_black, filter_linear_mipmap_anisotropic;  // R spec, G roughness

// PES's specular anti-aliasing clamp (asm 104: min(..., 0.30001)).
const float SPECULAR_AA_MAX = 0.30001;

vec3 srgb_to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

void fragment() {
	vec4 base = texture(Base_Tex_SRGB, UV);
	ALBEDO = srgb_to_linear(base.rgb);
	ALPHA = base.a;  // decal weight: GB2.w = base.a (report 4, line 422)

	vec3 n = vec3(0.0, 0.0, 1.0);
	if (has_NormalMap_Tex_NRM) {
		n = pes_normal(texture(NormalMap_Tex_NRM, UV));
	}
	vec3 nw = normalize(TANGENT * n.x + BINORMAL * n.y + NORMAL * n.z);
	vec4 srm = texture(SpecularMap_Tex_LIN, UV);
	float s = min(0.5 * (length(dFdx(nw)) + length(dFdy(nw))), SPECULAR_AA_MAX);
	ROUGHNESS = pes_f(sqrt(srm.g * srm.g + 2.0 * s * s));
	SPECULAR = pes_specular(srm.r);
	if (has_NormalMap_Tex_NRM) {
		NORMAL_MAP = n * 0.5 + 0.5;
	}
}
