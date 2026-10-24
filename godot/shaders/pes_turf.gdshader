// PES pes3DDF_Turf3D2020: the 3D grass fins over the turf. Ported from the
// install's DXBC, read instruction by instruction
// (pes3ddf_turf3d_2020.vs.asm, 237 lines; pes3ddf_turf3d_2020.ps.asm, 84
// lines). Reports: docs/render/pes-render-TurfFins.md (pipeline),
// pes-render-TurfPlacer.md (placer + binds), pes-render-TurfMask.md
// (mask affine, BC7 texels).
//
// Per-fragment o0 = v3 * v1.z * r0.y (PS asm 48-49): the VS albedo, the
// planar-shadow density and the distance fade, each <= 1, over the alpha
// test t7.w < 0.3 (PS asm 26-29) and the keep-sign discard v2.y < 0 (PS
// asm 24). The fins only attenuate.
//
// Space: Fox world, Y up, the same as Godot's world. Names below (r0-r11,
// cb0/cb2/cb4/cb5, t0-t7) are the asm's; each block cites its lines.
shader_type spatial;
// The tile carries each quad twice (opposite windings); PES draws both faces.
render_mode cull_disabled;

// ---- textures: the technique's roles, in lua bind order -------------------
// Base_Tex_SRGB t0 (VS g_tex_diffuse): the pitch-block map, world-space UV.
// Grass_MaskMap_Tex_LIN t1 (VS g_tex_mask): the runtime ground mask.
// NormalMap_Tex_NRM t2 (VS g_tex_grain): the grain map, world-space UV.
// SpecularMap_Tex_LIN t3 (VS slope table): xwyz swizzle (VS asm 177).
// Grass_DetailDiffuseMap t4 (VS g_tex_base): the detail albedo, 32x24 UV.
// Grass_PlanarShadow_Tex_LIN t5 (VS g_tex_planarShadow): shadow density.
// Grass_DetailNormalMap t6 (VS g_tex_detailNormal): xywz swizzle (VS asm 117).
// Grass_DiffuseMap_Tex_SRGB PS t7 (g_tex_grassDiffuse): grassfin, the alpha test.
uniform sampler2D Base_Tex_SRGB : filter_linear;                        // t0
uniform sampler2D Grass_MaskMap_Tex_LIN : hint_default_black, filter_linear;  // t1, ponytail: the runtime ground mask (TurfMask 1.4 OPEN); the builder binds zero (no mask input: ramps fall back to t0 terms, gate open) until read live, never tuned
uniform sampler2D NormalMap_Tex_NRM : filter_linear;                    // t2 grain
uniform sampler2D SpecularMap_Tex_LIN : filter_linear;                  // t3 slope table
uniform sampler2D Grass_DetailDiffuseMap_Tex_SRGB : filter_linear;      // t4
uniform sampler2D Grass_PlanarShadow_Tex_LIN : hint_default_black, filter_linear;  // t5, ponytail: the runtime shadow target (TurfPlacer 1.4 OPEN); the builder binds zero (density 1-sum = 1, unshadowed, VS asm 99) until read live, never tuned
uniform sampler2D Grass_DetailNormalMap_Tex_NRM : filter_linear;        // t6
uniform sampler2D Grass_DiffuseMap_Tex_SRGB : filter_linear_mipmap_anisotropic;  // t7 grassfin

// ---- material params (lua packing; TurfMask 2) ----------------------------
// PitchOffset cb4[1] = (1/W, 1/H, 0.5, 0.5), read from the install's turf
// init path at run time (turffin.cpp); set per stadium by the builder.
uniform vec4 PitchOffset = vec4(0.00715205, 0.01267266, 0.5, 0.5);
uniform vec4 UvOffset = vec4(0.0, 0.0, 0.5, 0.5);  // cb4[2], image const 0x142aa30a0
// ponytail: Parameter0/Parameter1 (cb4[3]/cb4[4], the wind anchors) are live
// material values (TurfMask 5 item 1); still air until read live.
uniform vec4 Parameter0 = vec4(0.0);
uniform vec4 Parameter1 = vec4(0.0);

// PS asm 26-29: the alpha test on the grassfin sample.
const float ALPHA_TEST = 0.3;
// VS asm 143/145/153: the dry/mid/lush anchors.
const vec3 DRIED_ANCHOR = vec3(0.145098045, 0.321568638, 0.0352941193);
const vec3 MID_ANCHOR = vec3(0.270588249, 0.301960796, 0.227450982);
const vec3 LUSH_ANCHOR = vec3(0.312435001, 0.40170899, 0.0);
// VS asm 101: the detail UV scale over the mask UV.
const vec2 DETAIL_UV_SCALE = vec2(32.0, 24.0);
// VS asm 156: the withers curve over the detail albedo.
const vec3 WITHERS_SLOPE = vec3(0.779999971, 0.779999971, 0.961000025);
const vec3 WITHERS_OFFSET = vec3(0.0979999974, 0.0710000023, 0.0390000008);
// Shading model 3 (pes-re-shaders.md 2.3, SSLighting_Sun_NoShadow), as pes_pitch.
const vec3 ID3_TINT = vec3(0.9, 1.0, 0.8);
const float ID3_F0 = 0.04;
const vec3 UP = vec3(0.0, 1.0, 0.0);

// The VS outputs the PS reads: v1 = (fin UV, shadow density), v2 = (keep
// sign r6.y, lush gate o2.w), v3 = (albedo o3, distance term o3.w).
varying vec3 pes_v1;
varying vec2 pes_v2;
varying vec4 pes_v3;

vec3 srgb_to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

void vertex() {
	// VS asm 31-33: the object-space vertex through cb5[4..6] (the world
	// matrix) into world space; the camera from cb2[20].
	vec3 r1 = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	vec3 camPos = (INV_VIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	// VS asm 27-45: wind sway. r0.y = 0.3*|cb4[3]-cb4[4]|^2 + 0.0125,
	// minus half the summed squared distance to the two anchors, floored
	// at 0; r0.x = r0.y * (v0.y + 10) * 16, spread over x/z by the
	// anchor-mean offset.
	float swayAmp = 0.3 * dot(Parameter0.xyz - Parameter1.xyz, Parameter0.xyz - Parameter1.xyz) + 0.0125440005;
	vec3 d0 = r1 - Parameter0.xyz;
	vec3 d1 = r1 - Parameter1.xyz;
	swayAmp = max(swayAmp - 0.5 * (dot(d0, d0) + dot(d1, d1)), 0.0);
	float sway = swayAmp * (VERTEX.y + 10.0) * 16.0;
	vec3 swayOff = vec3((r1.x - (Parameter0.x + Parameter1.x) * 0.5) * sway, 0.0,
	                    (r1.z - (Parameter0.z + Parameter1.z) * 0.5) * sway);
	// VS asm 46-52: the slope-UV radius (r0.w, min 1) and the world-space
	// mask UV (r2.xy = worldXZ * cb4[1].xy + cb4[1].zw). r1 is world (the
	// mask spans the rect), so cb4[2].xy carries the per-patch centre live
	// (the M1 image const is the default only); GF centres each slot on
	// its instance origin (the placer position), the decoded per-slot
	// value. ponytail: per-patch (multi-slot) centre is OPEN (Placer 3.2).
	vec2 slotCentre = MODEL_MATRIX[3].zx;
	vec2 slopeD = (r1.zx - slotCentre) * (1.0 / 12.0);
	float slopeR = min(2.0 * dot(slopeD, slopeD), 1.0);
	vec2 maskUV = r1.xz * PitchOffset.xy + PitchOffset.zw;
	// VS asm 53-62: t0 (base, sRGB-decoded) and t1 (mask) at the mask UV,
	// lod 0; the height/density floor r1.w from the slope radius and the
	// max chain; r2z keeps the chain for the distance term (asm 149).
	vec4 t0 = textureLod(Base_Tex_SRGB, maskUV, 0.0);
	t0.rgb = srgb_to_linear(t0.rgb);
	vec4 t1 = textureLod(Grass_MaskMap_Tex_LIN, maskUV, 0.0);
	vec4 r5seed = vec4(t1.z * 0.5, t1.z, t1.w * 0.01, t1.z * 0.5);
	float r2w = max(t0.y, t1.y);
	float r2z = max(1.0 - t0.z, max(r2w, r5seed.z));
	float r1w = max(slopeR * slopeR * 0.06, max(r2z, r5seed.w) * 0.06);
	// VS asm 63-70: the swayed world position (r6), then the mask-alpha
	// height gate on r6.y.
	vec3 r6 = swayOff + r1 - vec3(0.0, r1w, 0.0);
	float gate = clamp(min((0.99 - t1.w) * 0.06, r6.y) + 0.06, 0.0, 1.0);
	r6.y = gate - 0.06;
	// VS asm 82-99: v1.z = 1 - sum(t5), the planar-shadow density.
	// ponytail: the shadow UV needs the placer's object->shadow matrix
	// (cb5[0..3]) and the system consts (cb0[1].x, cb0[2].zw), both OPEN;
	// the mask UV stands in until they land. The stub t5 is black, so the
	// density is 1 (unshadowed) whatever the UV.
	vec4 t5 = textureLod(Grass_PlanarShadow_Tex_LIN, maskUV, 0.0);
	float shadowDensity = clamp(1.0 - (t5.x + t5.y + t5.z + t5.w), 0.0, 1.0);
	// VS asm 101-102: t2 (grain) at the mask UV, lod 0; the detail UV is
	// the mask UV scaled by (32, 24).
	vec4 grain = textureLod(NormalMap_Tex_NRM, maskUV, 0.0);
	vec2 detailUV = maskUV * DETAIL_UV_SCALE;
	// VS asm 103-115: the explicit sample lod from the camera height over
	// the swayed position (r8.y = 0 - camY) and the clip w.
	vec3 toSw = vec3(r6.x, 0.0, r6.z) - camPos;
	float camH = toSw.y / length(toSw);
	float clipW = -(MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).z;
	float lodArg = -camH * PROJECTION_MATRIX[1][1] * 12.0 / max(clipW, 0.0625);
	float lod = camH * -1.7 - log2(max(lodArg, 1e-5));
	// VS asm 116-117: t4 (detail albedo, sRGB-decoded) and t6 (detail
	// normal, xywz) at the detail UV at the explicit lod.
	vec4 t4 = textureLod(Grass_DetailDiffuseMap_Tex_SRGB, detailUV, lod);
	t4.rgb = srgb_to_linear(t4.rgb);
	vec4 t6 = textureLod(Grass_DetailNormalMap_Tex_NRM, detailUV, lod);
	// VS asm 118-126: the alpha power r0.x = t4.a ^ (5/11) and the o1w
	// ramp r10 (o1.w feeds the G-buffer id pack, not the albedo).
	float alphaPow = exp2(log2(max(t4.a, 1e-30)) * 0.454545468);
	vec3 r10 = min(alphaPow * vec3(1.0, 0.238999993, 0.196078002) + vec3(0.0, 0.760999978, 0.195999995), vec3(1.0));
	// VS asm 127-135: the lush gate: max(t1.x, 1.5*t0.a), mixed toward
	// itself by r2.w (so lush = max * (1 - r2.w)), then doubled and sat.
	float lush = max(t1.x, 1.5 * t0.a) * (1.0 - r2w);
	float lushSat = clamp(2.0 * lush, 0.0, 1.0);
	// VS asm 136-142: the dry ramp (sat 1 - max(...)) and the mid weight.
	float dryRaw = max((0.04 - r2w) * 25.0, (r2w - 0.04) * 10.0);
	float midW = clamp((r2w - 0.21) * 2.70270276, 0.0, 1.0);
	float dryW = clamp(1.0 - dryRaw, 0.0, 1.0);
	// VS asm 143-146: the dry ramp toward DRIED, the mid mix toward MID.
	vec3 alb = mix(t4.rgb, DRIED_ANCHOR, dryW);
	vec3 albMid = mix(alb, MID_ANCHOR, midW);
	// VS asm 65/148-151: the distance term r0.w = sat(slopeR + max(2 *
	// sat((camY - wy) * 0.8 - 0.5) * chain, maskBlue * 0.5)). r1.x past asm
	// 65 is camY - wy, not the world x.
	float camTerm = clamp((camPos.y - r1.y) * 0.8 - 0.5, 0.0, 1.0);
	float distW = clamp(slopeR + max(2.0 * camTerm * r2z, r5seed.w), 0.0, 1.0);
	// VS asm 147/152-154: the distance mix back toward t4, then the lush
	// mix toward LUSH by the grain alpha.
	vec3 albDist = mix(albMid, t4.rgb, distW);
	vec3 albLush = mix(albDist, LUSH_ANCHOR, grain.a);
	// VS asm 155-159: the withers curve over t4, mixed by r2.w.
	vec3 withers = clamp(t4.rgb * WITHERS_SLOPE + WITHERS_OFFSET, 0.0, 1.0);
	vec3 albFin = mix(albLush, withers, r2w);
	// VS asm 128-131: the t0.r powers over (t0.r * alphaPow).
	float base = max(t0.x * alphaPow, 1e-30);
	float r3x = exp2(log2(base) * 0.1);
	float r3y = exp2(log2(base) * 1.5);
	float r3z = exp2(log2(base) * 17.0939999);
	// VS asm 161-176: the grain frame: the normalized blade and the
	// negated Gram-Schmidt of UP against it (the pitch T1 idiom); the
	// slope LUT UV from the view direction's dots with the frame.
	vec3 camDir = (camPos - r1) / length(camPos - r1);
	vec3 blade = normalize(grain.xzy * 2.0 - 1.0);
	vec3 frameY = normalize(blade.y * blade - UP);
	vec2 slopeUV = vec2(dot(camDir, blade), dot(camDir, frameY)) * 0.5 + 0.5;
	// VS asm 177: t3 (slope, xwyz) at the slope UV at the explicit lod.
	vec4 slope = textureLod(SpecularMap_Tex_LIN, slopeUV, lod);
	vec3 slopeSw = vec3(slope.x, slope.w, slope.y);
	// VS asm 157/179-189: r8 is 1 - t4 past asm 157; d = r3.y - r3.x;
	// S = 1 - slope.w * (1 - 0.85 * (1 - (camDir.y * d + r3.x)));
	// r2v = (1 - (1-t4.r)*S, -(1-t4.g)*S, 1 - (1-t4.b)*S).
	float d = r3y - r3x;
	float sInner = camDir.y * d + r3x;
	float S = 1.0 - slopeSw.y * (1.0 - 0.85 * (1.0 - sInner));
	vec3 oneMinusT4 = vec3(1.0) - t4.rgb;
	vec3 r2v = vec3(1.0 - oneMinusT4.x * S, -oneMinusT4.y * S, 1.0 - oneMinusT4.z * S);
	// VS asm 181-184: the camDir.y power term (feeds o3's r3.z lane via
	// asm 188; folded into r2v's slope weight below per asm 187-189).
	float camPow = exp2(log2(max(camDir.y, 1e-30)) * 0.05);
	float r3zNew = camPow * d + r3x;
	// VS asm 190-194: o3 = sat(lushSat * (K - albFin * r2v) + D) with
	// K = 0.2 * distW + 0.6 and D = (r2v.x*albR, r2v.x*albG, r2v.z*albB).
	float K = distW * 0.2 + 0.6;
	vec3 D = vec3(r2v.x * albFin.x, r2v.x * albFin.y, r2v.z * albFin.z);
	vec3 albOut = clamp(lushSat * (vec3(K) - albFin * r2v) + D, 0.0, 1.0);
	pes_v3 = vec4(albOut, distW);
	// VS asm 100/126/134: o1.xy = the fin UV, o2.y = r6.y (the keep sign),
	// o2.w = the lush gate.
	pes_v1 = vec3(UV, shadowDensity);
	pes_v2 = vec2(r6.y, lush);
	// [DEVIATION 1] o4/o5/o6/o7 (view-space detail normal, v7 spec/rough,
	// velocity) are not ported: Godot's G-buffer replaces them. The fin
	// normal rides NORMAL; specular is second-order on grass (ExposureMatch
	// X4: SPECULAR = 0 identical to 4 d.p.).
	NORMAL = normalize((VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	// PS asm 24: discard where v2.y < 0.
	if (pes_v2.x < 0.0) {
		discard;
	}
	// PS asm 26-29: the alpha test on the grassfin sample (t7 is sRGB, so
	// the green is hardware-decoded in PES and decoded here).
	vec4 fin = texture(Grass_DiffuseMap_Tex_SRGB, pes_v1.xy);
	if (fin.a < ALPHA_TEST) {
		discard;
	}
	// PS asm 30-47: the distance fade r0.y. x = v3.w*(1-2g) + 2g with
	// g = t7.g; k = (1-0.6*v2.w)*grade, grade from coarse derivatives;
	// r0.y = k*(x-1) + 1, applied only where x < 1.
	float g = srgb_to_linear(vec3(fin.g, 0.0, 0.0)).x;
	float x = pes_v3.w * (1.0 - 2.0 * g) + 2.0 * g;
	float fade = 1.0;
	if (x < 1.0) {
		vec2 px = pes_v1.xy * 128.0;
		vec2 dx = dFdx(px);
		vec2 dy = dFdy(px);
		float deriv = max(dot(dx, dx), dot(dy, dy));
		float grade = clamp(-log2(max(deriv, 1e-30)) * 0.2 + 1.2, 0.0, 1.0);
		float k = (1.0 - 0.6 * pes_v2.y) * grade;
		fade = k * (x - 1.0) + 1.0;
	}
	// PS asm 48-49: o0 = v3 * v1.z * r0.y.
	ALBEDO = pes_v3.rgb * pes_v1.z * fade;
	ALPHA = 1.0;
	ROUGHNESS = 1.0;
	METALLIC = 0.0;
	SPECULAR = 0.0;
}

// Shading model 3 (pes-re-shaders.md 2.3, SSLighting_Sun_NoShadow): the same
// wrap diffuse as the surface (pes_pitch.gdshader), fed by the near-vertical
// fin normals. The darkening is geometric: blades sit at NdotL <= 0 on the
// wrap floor while the surface sits at 0.39 (TurfFins 1.2).
void light() {
	float NdotL = dot(NORMAL, LIGHT);
	float sNdotL = clamp(NdotL, 0.0, 1.0);
	float T = 0.2;
	DIFFUSE_LIGHT = max(clamp(NdotL + T * (1.0 - NdotL * NdotL), 0.0, 1.0), T) * LIGHT_COLOR * ATTENUATION;
	SPECULAR_LIGHT = vec3(0.0);
}
