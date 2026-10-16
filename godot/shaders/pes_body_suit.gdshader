// A whole body's Suit (fox3DFW_ConstantSRGB_NDR, unlit): the team's kit atlas on
// UV0 with the shirt markings laid over it at the uniform's UV0 placement.
shader_type spatial;
render_mode unshaded, cull_disabled;
#include "res://shaders/pes_marking.gdshaderinc"

uniform sampler2D kit_atlas : source_color, filter_linear_mipmap_anisotropic;

void fragment() {
	vec3 base = texture(kit_atlas, UV).rgb;
	vec4 m = has_marking ? shirt_marking(UV) : vec4(0.0);
	ALBEDO = mix(base, m.a > 0.0 ? m.rgb / m.a : base, m.a);
}
