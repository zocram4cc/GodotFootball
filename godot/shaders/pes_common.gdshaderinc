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
// analytic optical depth, transmittance and single-scatter inscatter in km
// (Rg 6360, Rt 6420). The .a of the inscatter return is the Mie/Rayleigh
// ratio the sky shader splits Bruneton-getMie-style.
float pes_optical_depth(float h, float mu, float r) {
	float a = sqrt(max(0.5 * r / h, 0.0));
	float erfc_approx = 2.3193 * abs(a) + sqrt(1.52 * a * a + 4.0);
	return exp(-h * (mu)) * erfc_approx;
}

vec3 pes_transmittance(float mu, vec3 beta_r, float h_r, vec3 beta_m_ext, float h_m) {
	float r = 6360.00977; // camera at 10 m, like the fog shader's r0 (§3)
	vec3 od = beta_r * pes_optical_depth(h_r, mu, r) + beta_m_ext * pes_optical_depth(h_m, mu, r);
	return exp(-od);
}

// Rayleigh single scatter toward v with cosine nu to the sun at cosine mu_s.
vec4 pes_inscatter(float mu, float nu, float mu_s, vec3 beta_r, float h_r, vec3 beta_m, float h_m, float g) {
	float r = 6360.00977;
	vec3 sum_r = vec3(0.0);
	float sum_m = 0.0;
	for (int i = 0; i < 16; i++) {
		float t = (float(i) + 0.5) / 16.0;
		float y = mu * t;
		vec3 tr = exp(-beta_r / h_r * y) * exp(-beta_r / h_r * max(mu_s, 0.0) * t);
		sum_r += tr / 16.0;
		sum_m += exp(-beta_m.r / h_m * y) / 16.0;
	}
	float ratio = sum_m / max(sum_r.r, 1e-6);
	return vec4(sum_r, ratio);
}
