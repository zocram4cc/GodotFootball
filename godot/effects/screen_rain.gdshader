shader_type spatial;
// MultiUvScrollScreen layer: the rain texture scrolls by speed_uv per second and
// repeats wrap_rate times per screen; tint is the PES colour (rgb) and its alpha byte.
render_mode unshaded, blend_mix, cull_disabled, depth_test_disabled;

uniform sampler2D rain_texture : source_color, filter_linear;
uniform vec2 speed_uv = vec2(0.0);
uniform vec2 wrap_rate = vec2(1.0);
uniform vec4 tint : source_color = vec4(1.0);

void fragment() {
	vec4 c = texture(rain_texture, fract(UV * wrap_rate + speed_uv * TIME));
	ALBEDO = c.rgb * tint.rgb;
	ALPHA = c.a * tint.a;
}
