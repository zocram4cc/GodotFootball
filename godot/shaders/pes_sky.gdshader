// PES match sky: the Bruneton single-scatter port (docs/re/pes-re-lighting.md
// Part B §1-2) with the rig's TppAtmosphere coefficients as uniforms. PES
// looks up precomputed transmittance/inscatter tables built from these same
// coefficients; this evaluates the same math analytically per pixel (32
// march steps), so clear-sky output matches table builds up to the missing
// multiple-scattering orders. Sun disk: 10·I·T inside cos 1° (§2); below the
// horizon the flat ground colour; night airglow star·π·skyColorSunScale.
// AT_CUBEMAP_PASS carries the .atsh SH9 row when the material user binds it
// (ambient path, Part B §5); the analytic sky itself feeds SKY radiance.

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
uniform vec3 ambient_sh9 = vec3(0.0); // .atsh L00 row (cd/m²·√4π), AT_CUBEMAP only
uniform float ambient_scale = 1.0; // skyLightLuminanceScale

const float Rg = 6360.0;
const float Rt = 6420.0;
const int STEPS = 32;

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
		sky_col = ambient_sh9 * ambient_scale;
	}
	COLOR = sky_col;
}
