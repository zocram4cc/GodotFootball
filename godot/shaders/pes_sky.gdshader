// PES match sky: the Bruneton single-scatter port (docs/re/pes-re-lighting.md
// Part B §1-2) with the rig's TppAtmosphere coefficients as uniforms. PES
// looks up precomputed transmittance/inscatter tables built from these same
// coefficients; this evaluates the same math analytically per pixel (32
// march steps), so clear-sky output matches table builds up to the missing
// multiple-scattering orders. Sun disk: 10·I·T inside cos 1° (§2); below the
// horizon the flat ground colour; night airglow star·π·skyColorSunScale.
// AT_CUBEMAP_PASS renders the span's .atsh SH9 radiance (Fox's sky light, Part B §5), which Godot
// convolves into the ambient light; the analytic sky above feeds the background only.

shader_type sky;

#include "res://shaders/pes_common.gdshaderinc"

uniform vec3 beta_rayleigh = vec3(0.0058, 0.0135, 0.0331);
uniform float height_rayleigh = 4.0; // km, used verbatim for the clear set (§10)
uniform vec3 beta_mie = vec3(0.004);
uniform float height_mie = 1.2; // km
uniform float mie_g = 0.8;
uniform float mie_absorption = 0.1;
uniform vec3 sun_direction = vec3(0.0, 1.0, 0.0);
uniform float sun_intensity = 471239.0; // lux·π·skyColorSunScale (§10): 150000·π·1
uniform float star_intensity = 0.0; // starLight·π·skyColorSunScale
uniform vec3 ground_color = vec3(0.5);
uniform vec3 ambient_sh9_0 = vec3(0.0);  // .atsh row, span time: L00 (cd/m²·√4π)
uniform vec3 ambient_sh9_1 = vec3(0.0);  // L1-1
uniform vec3 ambient_sh9_2 = vec3(0.0);  // L10 (world up)
uniform vec3 ambient_sh9_3 = vec3(0.0);  // L11
uniform vec3 ambient_sh9_4 = vec3(0.0);  // L2-2
uniform vec3 ambient_sh9_5 = vec3(0.0);  // L2-1
uniform vec3 ambient_sh9_6 = vec3(0.0);  // L20
uniform vec3 ambient_sh9_7 = vec3(0.0);  // L21
uniform vec3 ambient_sh9_8 = vec3(0.0);  // L22
uniform float ambient_scale = 1.0; // skyLightLuminanceScale

const float Rg = 6360.0;
const float Rt = 6420.0;
const int STEPS = 32;
// PES's SH basis: the constants FUN_14178f8d0 reads (PES VA 0x142a26568..0x142a26580). It is the
// real SH9 with z = up and its own signs (the m = ±1 terms are negative), so the caller passes
// EYEDIR.xzy. The atsh coefficients use this basis and order.
const float SH_Y00 = 0.282094806;   // 0x142a26568
const float SH_Y20 = 0.315391570;   // 0x142a2656c
const float SH_Y1 = 0.488602519;    // 0x142a26570; -Y1 at 0x142a2657c
const float SH_Y22 = 0.546274245;   // 0x142a26574
const float SH_Y2X = 1.09254849;    // 0x142a26578; -Y2X at 0x142a26580

// Radiance in direction d (SH frame, z up) from the span's nine .atsh coefficients, in
// the order L00, L1-1, L10, L11, L2-2, L2-1, L20, L21, L22.
vec3 sh9_radiance(vec3 d) {
	return ambient_sh9_0 * SH_Y00
		- ambient_sh9_1 * (SH_Y1 * d.y)
		+ ambient_sh9_2 * (SH_Y1 * d.z)
		- ambient_sh9_3 * (SH_Y1 * d.x)
		+ ambient_sh9_4 * (SH_Y2X * d.x * d.y)
		- ambient_sh9_5 * (SH_Y2X * d.y * d.z)
		+ ambient_sh9_6 * (SH_Y20 * (3.0 * d.z * d.z - 1.0))
		- ambient_sh9_7 * (SH_Y2X * d.x * d.z)
		+ ambient_sh9_8 * (SH_Y22 * (d.x * d.x - d.y * d.y));
}

void sky() {
	// No early returns: the sky processor forbids them.
	vec3 v = EYEDIR;
	float below = step(v.y, 0.0);
	float mu = max(v.y, 0.02);
	float nu = dot(normalize(vec3(v.x, mu, v.z)), sun_direction);
	vec3 T = pes_transmittance(mu, beta_rayleigh, height_rayleigh, beta_mie / (1.0 - mie_absorption), height_mie);
	vec4 S4 = pes_inscatter(mu, nu, sun_direction.y, beta_rayleigh, height_rayleigh, beta_mie, height_mie, mie_g);
	vec3 S = S4.rgb;
	vec3 mie_part = S * S4.a / max(S.r, 1e-4) * (beta_rayleigh.r / max(beta_rayleigh, vec3(1e-6)));
	vec3 L = S * 0.0596831031 * (1.0 + nu * nu) // Rayleigh 3/(16π)
		+ mie_part * (1.0 - mie_g * mie_g) * 0.0795774683 / pow(max(1.0 - mie_g * nu, 1e-4), 2.0); // Schlick
	L = max(L, vec3(0.0)) * sun_intensity;
	float disk = step(0.999847690, nu); // the 1° disk (§2)
	L += (10.0 * sun_intensity * T) * disk;
	L += star_intensity * vec3(3.284e-3, 4.11e-3, 6.24e-3); // night airglow
	vec3 sky_col = mix(L, ground_color, below);
	if (AT_CUBEMAP_PASS) {
		// The sky-light SH9 radiance (docs/re/pes-re-lighting.md Part B §5): Godot convolves this
		// cubemap into the ambient light, as Fox convolves its atsh row.
		sky_col = max(sh9_radiance(EYEDIR.xzy), vec3(0.0)) * ambient_scale;
	}
	COLOR = sky_col;
}
