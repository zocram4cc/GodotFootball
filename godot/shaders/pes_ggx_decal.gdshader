// PES fox3DDC_GGX: the pitch line paint (line_alp_dds1). The technique is a
// Deferred_Decal (ShaderAssignList: fox3DDC_GGX -> fox3ddf_ggx_decal). Ported from
// the install's DXBC, read instruction by instruction (fox3ddf_ggx_decal.ps.asm).
// docs/re/pes-re-pitch-grass.md section 4 describes the Blin decal and does not
// apply to this technique.
//
// PES writes the decal with weight base.a and shading model 1 (GGX): the diffuse
// keeps the albedo, the specular is GGX with F0 = 0.04 + 0.96 * metalness
// (pes-re-shaders.md 2.3).
//
// [DEVIATION 1] The PS's dither coverage (t15 and cb0[0].w, both open in
// pes-re-shaders.md 8.1) is not ported: the decal is its alpha blend alone.
// [DEVIATION 2] PES's Fresnel LUT (g_lut1) is not found, so F is Schlick.
// light() cannot read SPECULAR in Godot 4.7, so GB2.y travels in BACKLIGHT.r.
shader_type spatial;
render_mode blend_mix, depth_draw_never, shadows_disabled;
#include "res://shaders/pes_common.gdshaderinc"

uniform sampler2D Base_Tex_SRGB : filter_linear_mipmap_anisotropic;      // line_alp: RGB sRGB, A coverage
uniform sampler2D NormalMap_Tex_NRM : filter_linear_mipmap_anisotropic;  // line_nrm (x = R*A, y = G)
uniform bool has_NormalMap_Tex_NRM = false;
uniform sampler2D SpecularMap_Tex_LIN : hint_default_black, filter_linear_mipmap_anisotropic;  // line_srm: R spec, G roughness
uniform sampler2D MetalnessMap_Tex_LIN : hint_default_black, filter_linear_mipmap_anisotropic;  // line_mtl: R metalness

// PES's specular anti-aliasing clamp (asm 104: min(..., 0.30001)).
const float SPECULAR_AA_MAX = 0.30001;
// Shading model 1: F0 = 0.04 + 0.96 * GB2.z (pes-re-shaders.md 2.3).
const float DIELECTRIC_F0 = 0.04;
const float METAL_F0 = 0.96;

vec3 srgb_to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

void fragment() {
	vec4 base = texture(Base_Tex_SRGB, UV);
	ALBEDO = srgb_to_linear(base.rgb);
	ALPHA = base.a;  // decal weight: GB0.a = GB2.w = base.a (asm 34, 81-82)

	vec3 n = vec3(0.0, 0.0, 1.0);
	if (has_NormalMap_Tex_NRM) {
		n = pes_normal(texture(NormalMap_Tex_NRM, UV));
	}
	vec3 nw = normalize(TANGENT * n.x + BINORMAL * n.y + NORMAL * n.z);
	vec4 srm = texture(SpecularMap_Tex_LIN, UV);
	// Specular anti-aliasing on the normal's screen-space change (asm 97-111).
	float s = min(0.5 * (length(dFdx(nw)) + length(dFdy(nw))), SPECULAR_AA_MAX);
	ROUGHNESS = sqrt(srm.g * srm.g + 2.0 * s * s);  // GB2.x: no f() remap on GGX (asm 111)
	SPECULAR = srm.r;                                 // GB2.y (asm 110), Godot's own ambient
	BACKLIGHT = vec3(srm.r, 0.0, 0.0);                // GB2.y for light() below
	METALLIC = texture(MetalnessMap_Tex_LIN, UV).r;   // GB2.z (asm 112-113)
	if (has_NormalMap_Tex_NRM) {
		NORMAL_MAP = n * 0.5 + 0.5;
	}
}

void light() {
	float NdotL = clamp(dot(NORMAL, LIGHT), 0.0, 1.0);
	float NdotV = clamp(dot(NORMAL, VIEW), 0.0, 1.0);
	vec3 H = normalize(LIGHT + VIEW);
	float NdotH = clamp(dot(NORMAL, H), 0.0, 1.0);
	float specScale = BACKLIGHT.r;

	// Shading model 1 keeps the albedo in the diffuse term: T = 0 and wrapW = 0,
	// so the diffuse is sat(NdotL). The albedo multiplies this after light().
	DIFFUSE_LIGHT = NdotL * LIGHT_COLOR * ATTENUATION;

	float r = max(ROUGHNESS, 0.02);
	float a = r * r;
	float a2 = a * a;
	float D = a2 / pow(NdotH * NdotH * (a2 - 1.0) + 1.0, 2.0);  // GGX without 1/pi (pes-re-shaders.md 2.3)
	float k = a * 0.5;
	float vis = 1.0 / ((NdotL * (1.0 - k) + k) * (NdotV * (1.0 - k) + k));  // Smith-Schlick
	float f0 = DIELECTRIC_F0 + METAL_F0 * METALLIC;
	float F = f0 + (1.0 - f0) * pow(1.0 - clamp(dot(H, LIGHT), 0.0, 1.0), 5.0);
	SPECULAR_LIGHT = min(LIGHT_COLOR * F * D * vis * NdotL * 0.25 * specScale * ATTENUATION, vec3(1e4));
}
