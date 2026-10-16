// Shirt markings (pes-kit-assembly.md 8.7): one Marking_Tex_LIN sampled at affine
// transforms of UV0. Name u in [0,0.64), back number [0.64,0.88), chest [0.88,1)x[0,0.5).
uniform sampler2D Marking_Tex_LIN : filter_linear_mipmap;  // LIN role: no sRGB decode
uniform bool has_marking = false;
uniform vec4 NameScaleXParam = vec4(0.0);
uniform vec4 BNumberScaleXParam = vec4(0.0);
uniform vec4 FNumberScaleXParam = vec4(0.0);
const float BACK_NUMBER_V_SCALE = 160.0 / 255.0;

// One part of the marking texture: its [0,1]^2 placement mapped into an atlas
// region; outside the rectangle it contributes nothing (premultiplied).
vec4 marking_part(sampler2D tex, vec2 q, vec2 origin, vec2 size) {
	if (any(lessThan(q, vec2(0.0))) || any(greaterThan(q, vec2(1.0)))) {
		return vec4(0.0);
	}
	vec4 t = texture(tex, origin + q * size);
	return vec4(t.rgb * t.a, t.a);
}

// Name, back and chest number over UV0, premultiplied.
vec4 shirt_marking(vec2 uv) {
	vec2 qn = vec2((uv.x - 0.5) * NameScaleXParam.x + 0.5, uv.y * NameScaleXParam.y + NameScaleXParam.z);
	vec4 name = marking_part(Marking_Tex_LIN, qn, vec2(0.0), vec2(0.64, 1.0));
	// Back number (shirt_nb_nc.ps: cb4[4]): centred in u like the name, v scaled
	// by 160/255 before the offset; .w unused.
	vec2 qb = vec2((uv.x - 0.5) * BNumberScaleXParam.x + 0.5,
			BACK_NUMBER_V_SCALE * uv.y * BNumberScaleXParam.y + BNumberScaleXParam.z);
	vec4 back = marking_part(Marking_Tex_LIN, qb, vec2(0.64, 0.0), vec2(0.24, 1.0));
	vec2 qf = uv * FNumberScaleXParam.xy + FNumberScaleXParam.zw;
	vec4 front = marking_part(Marking_Tex_LIN, qf, vec2(0.88, 0.0), vec2(0.12, 0.5));
	vec4 m = name + back * (1.0 - name.a);
	return m + front * (1.0 - m.a);
}
