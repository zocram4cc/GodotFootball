// Shared PES character-shading helpers (docs/re/pes-re-shaders.md).

// Normal maps: x = R·A·2−1, y = G·2−1, z reconstructed (BC5 and DXT5nm alike).
vec3 pes_normal(vec4 t) {
	vec2 xy = vec2(t.r * t.a, t.g) * 2.0 - 1.0;
	return vec3(xy, sqrt(max(0.0, 1.0 - dot(xy, xy))));
}

// Roughness remap of skin, face, arms, hair and eye (5th-order polynomial).
float pes_f(float r) {
	return 0.15 + 0.85 * (15.0 * r + 15.0 * r * r + 10.0 * r * r * r + 5.0 * r * r * r * r + 2.0 * r * r * r * r * r) / 47.0;
}

// PES specular is SRM.r scaled by a preset F0. Godot's dielectric F0 is
// 0.16·specular²; at Godot's default F0 (0.04) that gives sqrt(SRM.r)/2.
// ponytail: preset F0 from material_params.fmtt not applied yet.
float pes_specular(float srm_r) {
	return sqrt(max(srm_r, 0.0)) * 0.5;
}

// Match atmosphere (docs/re/pes-re-lighting.md Part B §1-2): Bruneton
// analytic single scatter in km (Rg 6360, Rt 6420). Optical depth integrates
// the exponential density along the view ray (8 march steps from the camera
// at 10 m altitude, like the fog shader's r0 = 6360.00977 in §3); the
// inscatter .a is the Mie/Rayleigh ratio the sky shader splits getMie-style.
float pes_density(float h, float y) {
	return exp(-max(y, 0.0) / h);
}

float pes_ray_length(float mu) {
	float r = 6360.00977;
	float b = r * mu;
	float det = b * b - (r * r - 6420.0 * 6420.0);
	return -b + sqrt(max(det, 0.0)); // km to the top of the atmosphere
}

float pes_optical_depth(float mu, float h) {
	float len = pes_ray_length(mu);
	float od = 0.0;
	for (int i = 0; i < 8; i++) {
		float s = (float(i) + 0.5) / 8.0 * len;
		float y = sqrt(max(6360.00977 * 6360.00977 + s * s + 2.0 * 6360.00977 * mu * s, 0.0)) - 6360.0;
		od += pes_density(h, y) * (len / 8.0);
	}
	return od;
}

vec3 pes_transmittance(float mu, vec3 beta_r, float h_r, vec3 beta_m_ext, float h_m) {
	return exp(-(beta_r * pes_optical_depth(mu, h_r) + beta_m_ext * pes_optical_depth(mu, h_m)));
}

// Rayleigh single scatter toward v with cosine nu to the sun at cosine mu_s.
vec4 pes_inscatter(float mu, float nu, float mu_s, vec3 beta_r, float h_r, vec3 beta_m, float h_m, float g) {
	float len = pes_ray_length(mu);
	vec3 sum_r = vec3(0.0);
	float sum_m = 0.0;
	for (int i = 0; i < 16; i++) {
		float s = (float(i) + 0.5) / 16.0 * len;
		float y = sqrt(max(6360.00977 * 6360.00977 + s * s + 2.0 * 6360.00977 * mu * s, 0.0)) - 6360.0;
		vec3 t_view = exp(-min(beta_r * pes_optical_depth(mu, h_r) * (s / max(len, 1e-6)) + beta_m * (s / max(len, 1e-6)), vec3(20.0)));
		vec3 t_sun = pes_transmittance(mu_s, beta_r, h_r, beta_m / (1.0 - 0.1), h_m);
		sum_r += t_view * t_sun * pes_density(h_r, y) * (len / 16.0);
		sum_m += pes_density(h_m, y) * (len / 16.0);
	}
	sum_r *= beta_r;
	float ratio = sum_m * beta_m.r / max(sum_r.r, 1e-6);
	return vec4(sum_r, ratio);
}
