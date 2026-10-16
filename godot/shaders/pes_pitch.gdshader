// PES pes3DDF_Pitch2020: the turf under the grass. Ported from the install's DXBC
// (pes3ddf_pitch_2020 PS, 187 instructions) read instruction by instruction in
// B4Grass (asm/pes3ddf_pitch_2020.ps.asm). docs/re/pes-re-pitch-grass.md sections
// 3 and 7 give the same formulas; where that report and the asm differ, the asm
// is followed and the difference is named in the deviation list below.
//
// Not in this material: the mowing stripes. They come from the turf3d grass mesh
// (pes3ddf_turf3d_2020, priority 5 of the report, open), so this surface has none.
//
// Space: PES works in world space with Y up (Fox axes, the same as Godot's world).
// Godot's VIEW and NORMAL are view space, so the shader converts them with
// INV_VIEW_MATRIX and writes the normal back through VIEW_MATRIX.
//
// light() cannot read SPECULAR in Godot 4.7, so the two per-pixel values the
// shading-model-3 light needs travel in BACKLIGHT: .r = translucency T (GB1.T),
// .g = the specular scale (GB2.y). ROUGHNESS is readable there and is used as is.
shader_type spatial;

// ---- material params, by the fmdl's names (values read from st002) ----------
uniform vec4 AOParamX = vec4(1.0);
uniform vec4 AOParamY = vec4(0.1);
uniform vec4 AOParamZ = vec4(1.5);
uniform vec4 AOParamW = vec4(0.85);
uniform vec4 URepeat_UV = vec4(32.0);
uniform vec4 VRepeat_UV = vec4(24.0);
uniform vec4 NormalFlatness = vec4(0.5);
uniform vec4 RepairColor = vec4(0.312655, 0.402, 0.0, 1.0);
uniform vec4 CameraVectorBlend = vec4(0.0);
uniform vec4 WithersParameter0 = vec4(0.78);
uniform vec4 WithersParameter1 = vec4(0.78);
uniform vec4 WithersParameter2 = vec4(0.961);
uniform vec4 WithersParameter3 = vec4(0.098);
uniform vec4 WithersParameter4 = vec4(0.071);
uniform vec4 WithersParameter5 = vec4(0.039);
uniform vec4 SrmParameter0 = vec4(1.0);
uniform vec4 SrmParameter1 = vec4(0.239);
uniform vec4 SrmParameter2 = vec4(0.196);
uniform vec4 SrmParameter3 = vec4(0.0);
uniform vec4 SrmParameter4 = vec4(0.761);
uniform vec4 SrmParameter5 = vec4(0.196);
uniform vec4 EmissiveParam0 = vec4(0.05);
uniform vec4 EmissiveParam1 = vec4(0.1);
uniform vec4 EmissiveParam2 = vec4(17.094);

// ---- textures: the PS's slots (report 3.2), by the fmdl's role names --------
// The *_SRGB roles are sRGB-encoded colour (hardware-decoded in PES), so their
// RGB is decoded here by srgb_to_linear and their alpha is read raw.
uniform sampler2D Base_Tex_SRGB : filter_linear_mipmap_anisotropic;                  // t0 pitch_alp, untiled
uniform sampler2D NormalMap_Tex_NRM : filter_linear_mipmap_anisotropic, repeat_enable; // t1 turf_nrm, tiled
uniform sampler2D Base_Tex_2_SRGB : filter_nearest_mipmap, repeat_enable;            // t3 turf_bsm, tiled, point (s3)
uniform sampler2D GrainNormal_Tex_LIN : filter_linear_mipmap_anisotropic;            // t4 pitch_detail, untiled
uniform sampler2D SlopeTable_Tex_LIN : filter_linear, repeat_disable;                // t5 grass_slopetable, clamp
uniform sampler2D Mask_Tex_SRGB : hint_default_black, filter_nearest_mipmap, repeat_enable;  // t6, unbound in st002
uniform sampler2D Grain_Tex_LIN : filter_nearest_mipmap, repeat_enable;              // t7 pitch_grain_alp, untiled, point (s7)

// ---- constants the PS hard-codes ------------------------------------------
// SSLighting's shading-model-3 tint and F0 (pes-re-shaders.md 2.3; metalness 0).
const vec3 ID3_TINT = vec3(0.9, 1.0, 0.8);
const float ID3_F0 = 0.04;
// [DEVIATION 4] PES discards where pitch_alp.B < t15.w * cb0[0].w (both open,
// pes-re-shaders.md 8.1). B is 0 or 255 on 97% of st002's texels (the border
// band and the playable area), so any threshold between them decides only the
// edge texels; 0.5 is the midpoint.
const float VISIBILITY_THRESHOLD = 0.5;
const vec3 UP = vec3(0.0, 1.0, 0.0);

vec3 srgb_to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

void fragment() {
	vec4 raw0 = texture(Base_Tex_SRGB, UV);
	vec3 pitchRgb = srgb_to_linear(raw0.rgb);
	if (pitchRgb.b < VISIBILITY_THRESHOLD) {
		discard;  // outside the playable area (B channel mask, see DEVIATION 4)
	}

	// ---- view and grain frame (asm 44-75) ---------------------------------
	// V points from the surface to the camera in world space (asm 58-61 build the
	// same vector as camera minus surface). [DEVIATION 1] CameraVectorBlend pulls V
	// toward the sun (asm 62-66); it is 0 on st002, and the sun is per light, so the
	// blend is not applied.
	vec3 V = normalize((INV_VIEW_MATRIX * vec4(VIEW, 0.0)).xyz);
	vec4 grainRaw = texture(Grain_Tex_LIN, UV);                                  // t7 (asm 44)
	vec3 ng = normalize(vec3(grainRaw.r, grainRaw.b, grainRaw.g) * 2.0 - 1.0);   // asm 45-48, .xzy
	vec3 T1 = normalize(ng.y * ng - UP);                                         // asm 49-55
	float dVN = dot(V, ng);                                                      // asm 69-70
	float dVT = dot(V, T1);                                                      // asm 67-68
	vec4 slope = texture(SlopeTable_Tex_LIN, vec2(0.5 + 0.5 * dVN, 0.5 + 0.5 * dVT));  // asm 72-75

	// ---- height and density (asm 71-96) -----------------------------------
	float h = log2(max(V.y, 1e-5));                               // asm 71
	float qt = exp2(h * AOParamX.x);                              // asm 76, 79
	float pw = exp2(h * EmissiveParam0.x);                        // asm 77-78
	vec2 tiled = UV * vec2(URepeat_UV.x, VRepeat_UV.x);           // asm 80
	vec4 turf = texture(Base_Tex_2_SRGB, tiled);                  // t3 (asm 81)
	vec3 base = srgb_to_linear(turf.rgb);
	float lin = pow(turf.a, 1.0 / 2.2);                           // asm 83-85: alpha is linear in the sRGB format
	float v = lin * pitchRgb.r;                                   // asm 86
	float vlog = log2(max(v, 1e-30));                             // asm 87
	float aoy = exp2(vlog * AOParamY.x);                          // asm 88
	float aoz = exp2(vlog * AOParamZ.x);                          // asm 88
	float em1 = exp2(vlog * EmissiveParam1.x);                    // asm 89-90
	float em2 = exp2(vlog * EmissiveParam2.x);                    // asm 89-90
	float wither = 1.0 - (qt * (aoz - aoy) + aoy);                // asm 92-94
	float match = 1.0 - wither * AOParamW.x;                      // asm 95
	float spend = 1.0 - slope.g * match;                          // asm 96

	// ---- mask (t6, untiled; zero in st002) ----------------------------------
	vec3 maskRgb = srgb_to_linear(texture(Mask_Tex_SRGB, UV).rgb);   // asm 104-105
	float m2 = 1.0 - clamp(maskRgb.b * 0.1, 0.0, 1.0);               // asm 108-109
	float m3 = 1.0 - clamp(maskRgb.b * 0.5, 0.0, 1.0);
	float m4 = 1.0 - clamp(maskRgb.b, 0.0, 1.0);

	// ---- albedo (asm 97-112) ------------------------------------------------
	vec3 c = vec3(1.0) - (vec3(1.0) - base) * spend;                         // asm 97-98
	vec3 repaired = base + grainRaw.a * (RepairColor.rgb - base);            // asm 99-100
	vec3 curve = clamp(vec3(base.r * WithersParameter0.x + WithersParameter3.x,
	                        base.g * WithersParameter1.x + WithersParameter4.x,
	                        base.b * WithersParameter2.x + WithersParameter5.x), 0.0, 1.0);   // asm 101-102
	vec3 tint = curve - repaired;                                             // asm 103
	float ao = max(raw0.a, maskRgb.r);                                        // asm 105
	float gW = max(pitchRgb.g, maskRgb.g);                                    // asm 105
	vec3 y = gW * tint + repaired;                                            // asm 106
	vec3 n = c * y;                                                           // asm 107
	vec3 tOut = n * m2;                                                       // asm 110
	vec3 uOut = vec3(0.8) - tOut;                                             // asm 111
	ALBEDO = clamp(ao * uOut + tOut, 0.0, 1.0);                               // asm 112

	// ---- normal (asm 114-152) -----------------------------------------------
	// n1 is the turf normal, tiled (asm 114-121); g is the flattened grain normal
	// from t4 with the z lane raised by one (asm 123-127); sd, q and the up-mix
	// are asm 128-140 in order.
	vec4 turfN = texture(NormalMap_Tex_NRM, tiled);                           // t1 (asm 82)
	vec2 nxy = vec2(turfN.r * turfN.a, turfN.g) * 2.0 - 1.0;
	vec3 n1 = vec3(nxy, sqrt(max(1.0 - dot(nxy, nxy), 0.0) + 1e-4));
	vec3 detail = texture(GrainNormal_Tex_LIN, UV).rgb * 2.0 - 1.0;          // t4 (asm 123-124)
	vec3 gFlat = detail + NormalFlatness.x * (vec3(0.0, 0.0, 1.0) - detail); // asm 125-126
	vec3 g = gFlat + vec3(0.0, 0.0, 1.0);                                     // asm 127
	float sd = dot(g, vec3(-n1.x, -n1.y, n1.z));                              // asm 128
	float qx = sd * g.x / g.z;                                                // asm 129-130
	float qy = sd * g.y / g.z;
	vec3 mRaw = vec3(n1.x + qx, max(sd - n1.z, 0.0), n1.y + qy);             // asm 131-132, x / up / z
	vec3 mn = normalize(mRaw);                                               // asm 133-135
	vec3 nWorld = normalize(mix(mn, UP, maskRgb.b));                         // asm 136-140 (weight: Mask.B)
	NORMAL = normalize((VIEW_MATRIX * vec4(nWorld, 0.0)).xyz);

	// ---- GB1 and GB2 (asm 161-179) -----------------------------------------
	// Roughness is S1; the specular scale is m4 * slope.r * match * S0 (asm 174-178);
	// the translucency T = m3 * sat(S2*lin + S5) (asm 165) rides in BACKLIGHT.r.
	float s0 = clamp(lin * SrmParameter0.x + SrmParameter3.x, 0.0, 1.0);     // asm 164
	float s1 = clamp(lin * SrmParameter1.x + SrmParameter4.x, 0.0, 1.0);     // asm 164
	float tr = clamp(lin * SrmParameter2.x + SrmParameter5.x, 0.0, 1.0);     // asm 164
	float specScale = m4 * slope.r * match * s0;                             // GB2.y (asm 174-178)
	ROUGHNESS = s1;                                                          // GB2.x (asm 177)
	SPECULAR = specScale;                                                    // Godot's own ambient reflection
	METALLIC = 0.0;                                                          // GB2.z (asm 179)
	BACKLIGHT = vec3(m3 * tr, specScale, 0.0);                               // read by light() below
	// [DEVIATION 3] The VS fog terms and GB3/GB4 (velocity) are not ported: Godot's
	// fog and motion vectors replace them.
	// [DEVIATION 5] MatParamIndex_0 (205 on st002) is read by no instruction of the
	// PS (report 5.1), so no preset is applied and the parameter is not declared.
}

// Shading model 3 (pes-re-shaders.md 2.3, SSLighting_Sun_NoShadow): the diffuse is
// max(sat(NdotL + T(1-NdotL^2)), T) with wrapW = 1; the specular is GGX with
// Schlick Fresnel at F0 = 0.04, scaled by GB2.y and tinted (0.9, 1, 0.8).
// [DEVIATION 6] PES's Fresnel comes from a LUT (g_lut1) that is not found; Schlick
// stands in for it. [DEVIATION 7] The GGX term has no 1/pi, as the report writes
// it; that absolute level is not checked against a PES frame.
void light() {
	float NdotL = dot(NORMAL, LIGHT);
	float sNdotL = clamp(NdotL, 0.0, 1.0);
	float NdotV = clamp(dot(NORMAL, VIEW), 0.0, 1.0);
	vec3 H = normalize(LIGHT + VIEW);
	float NdotH = clamp(dot(NORMAL, H), 0.0, 1.0);

	float T = BACKLIGHT.r;
	float specScale = BACKLIGHT.g;
	DIFFUSE_LIGHT = max(clamp(NdotL + T * (1.0 - NdotL * NdotL), 0.0, 1.0), T) * LIGHT_COLOR * ATTENUATION;

	float r = max(ROUGHNESS, 0.02);
	float a = r * r;
	float a2 = a * a;
	float D = a2 / pow(NdotH * NdotH * (a2 - 1.0) + 1.0, 2.0);
	float k = a * 0.5;
	float vis = 1.0 / ((sNdotL * (1.0 - k) + k) * (NdotV * (1.0 - k) + k));
	float F = ID3_F0 + (1.0 - ID3_F0) * pow(1.0 - clamp(dot(H, LIGHT), 0.0, 1.0), 5.0);
	SPECULAR_LIGHT = min(LIGHT_COLOR * F * D * vis * sNdotL * 0.25 * specScale * ID3_TINT * ATTENUATION,
	                     vec3(1e4));
}
